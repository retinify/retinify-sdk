// SPDX-FileCopyrightText: Copyright (c) 2025 Sensui Yagi. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "mat.hpp"
#include "session.hpp"

#include "retinify/log.hpp"
#include "retinify/path.hpp"
#include "retinify/pipeline.hpp"
#include "retinify/status.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <new>

namespace retinify
{
class Pipeline::Impl
{
  public:
    Impl() noexcept = default;

    ~Impl() noexcept
    {
        initialized_ = false;
        (void)left_.Free();
        (void)right_.Free();
        (void)disparity_.Free();
    }

    Impl(const Impl &) = delete;
    auto operator=(const Impl &) noexcept -> Impl & = delete;
    Impl(Impl &&) noexcept = delete;
    auto operator=(Impl &&other) noexcept -> Impl & = delete;

    auto Initialize(const std::size_t imageHeight, const std::size_t imageWidth) noexcept -> Status
    {
        Status status;

        if ((imageHeight != 320 || imageWidth != 640) && //
            (imageHeight != 480 || imageWidth != 640) && //
            (imageHeight != 720 || imageWidth != 1280))
        {
            LogError("Height and width must be one of the following: 320x640, 480x640, or 720x1280.");
            status = Status(StatusCategory::RETINIFY, StatusCode::INVALID_ARGUMENT);
            return status;
        }

        status = left_.Allocate(imageHeight, imageWidth, 1, sizeof(float));
        if (!status.IsOK())
        {
            return status;
        }

        status = right_.Allocate(imageHeight, imageWidth, 1, sizeof(float));
        if (!status.IsOK())
        {
            return status;
        }

        status = disparity_.Allocate(imageHeight, imageWidth, 1, sizeof(float));
        if (!status.IsOK())
        {
            return status;
        }

        status = session_.Initialize(ONNXModelFilePath());
        if (!status.IsOK())
        {
            return status;
        }

        status = session_.BindInput("left", left_);
        if (!status.IsOK())
        {
            return status;
        }

        status = session_.BindInput("right", right_);
        if (!status.IsOK())
        {
            return status;
        }

        status = session_.BindOutput("disparity", disparity_);
        if (!status.IsOK())
        {
            return status;
        }

        initialized_ = true;
        return status;
    }

    auto Run(const void *leftImageData, const std::size_t leftImageStride, const void *rightImageData, const std::size_t rightImageStride, void *disparityData, const std::size_t disparityStride) const noexcept -> Status
    {
        Status status;

        if (!initialized_)
        {
            LogError("Pipeline is not initialized. Call Initialize() before Run().");
            status = Status(StatusCategory::RETINIFY, StatusCode::FAIL);
            return status;
        }

        status = left_.Upload(leftImageData, leftImageStride);
        if (!status.IsOK())
        {
            return status;
        }

        status = right_.Upload(rightImageData, rightImageStride);
        if (!status.IsOK())
        {
            return status;
        }

        status = left_.Wait();
        if (!status.IsOK())
        {
            return status;
        }

        status = right_.Wait();
        if (!status.IsOK())
        {
            return status;
        }

        status = disparity_.Wait();
        if (!status.IsOK())
        {
            return status;
        }

        status = session_.Run();
        if (!status.IsOK())
        {
            return status;
        }

        status = disparity_.Download(disparityData, disparityStride);
        if (!status.IsOK())
        {
            return status;
        }

        status = disparity_.Wait();
        if (!status.IsOK())
        {
            return status;
        }

        return status;
    }

  private:
    bool initialized_{false};
    Session session_;
    Mat left_;
    Mat right_;
    Mat disparity_;
};

Pipeline::Pipeline() noexcept
{
    static_assert(sizeof(buffer_) >= sizeof(Impl), "Buffer too small for Impl");
    static_assert(alignof(std::max_align_t) >= alignof(Impl), "Buffer alignment insufficient");

    new (&buffer_) Impl();
}

Pipeline::~Pipeline() noexcept
{
    impl()->~Impl();
}

auto Pipeline::impl() noexcept -> Impl *
{
    return std::launder(reinterpret_cast<Impl *>(&buffer_)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

auto Pipeline::impl() const noexcept -> const Impl *
{
    return std::launder(reinterpret_cast<const Impl *>(&buffer_)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

auto Pipeline::Initialize(std::size_t imageHeight, std::size_t imageWidth) noexcept -> Status
{
    return this->impl()->Initialize(imageHeight, imageWidth);
}

auto Pipeline::Run(const void *leftImageData, std::size_t leftImageStride, const void *rightImageData, std::size_t rightImageStride, void *disparityData, std::size_t disparityStride) const noexcept -> Status
{
    return this->impl()->Run(leftImageData, leftImageStride, rightImageData, rightImageStride, disparityData, disparityStride);
}
} // namespace retinify