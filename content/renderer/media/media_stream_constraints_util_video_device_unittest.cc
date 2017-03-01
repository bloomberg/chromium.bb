// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util_video_device.h"

#include <algorithm>
#include <utility>

#include "base/optional.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"

namespace content {

namespace {

const char kDeviceID1[] = "fake_device_1";
const char kDeviceID2[] = "fake_device_2";
const char kDeviceID3[] = "fake_device_3";
const char kDeviceID4[] = "fake_device_4";
}

class MediaStreamConstraintsUtilVideoDeviceTest : public testing::Test {
 public:
  void SetUp() override {
    // Default device. It is default because it is the first in the enumeration.
    ::mojom::VideoInputDeviceCapabilitiesPtr device =
        ::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kDeviceID1;
    device->facing_mode = ::mojom::FacingMode::NONE;
    device->formats = {
        media::VideoCaptureFormat(gfx::Size(200, 200), 40.0f,
                                  media::PIXEL_FORMAT_I420),
        // This entry is is the closest to defaults.
        media::VideoCaptureFormat(gfx::Size(500, 500), 40.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1000, 1000), 20.0f,
                                  media::PIXEL_FORMAT_I420),
    };
    capabilities_.device_capabilities.push_back(std::move(device));

    // A low-resolution device.
    device = ::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kDeviceID2;
    device->facing_mode = ::mojom::FacingMode::ENVIRONMENT;
    device->formats = {
        media::VideoCaptureFormat(gfx::Size(40, 30), 20.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(320, 240), 30.0f,
                                  media::PIXEL_FORMAT_I420),
        // This format has defaults for all settings
        media::VideoCaptureFormat(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            MediaStreamVideoSource::kDefaultFrameRate,
            media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(800, 600), 20.0f,
                                  media::PIXEL_FORMAT_I420),
    };
    capabilities_.device_capabilities.push_back(std::move(device));

    // A high-resolution device.
    device = ::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kDeviceID3;
    device->facing_mode = ::mojom::FacingMode::USER;
    device->formats = {
        media::VideoCaptureFormat(gfx::Size(600, 400), 10.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(640, 480), 10.0f,
                                  media::PIXEL_FORMAT_I420),
        // This format has defaults for all settings
        media::VideoCaptureFormat(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            MediaStreamVideoSource::kDefaultFrameRate,
            media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1280, 720), 60.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1920, 1080), 60.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(2304, 1536), 10.0f,
                                  media::PIXEL_FORMAT_I420),
    };
    capabilities_.device_capabilities.push_back(std::move(device));

    // A depth capture device.
    device = ::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kDeviceID4;
    device->facing_mode = ::mojom::FacingMode::ENVIRONMENT;
    device->formats = {media::VideoCaptureFormat(gfx::Size(640, 480), 30.0f,
                                                 media::PIXEL_FORMAT_Y16)};
    capabilities_.device_capabilities.push_back(std::move(device));

    capabilities_.power_line_capabilities = {
        media::PowerLineFrequency::FREQUENCY_DEFAULT,
        media::PowerLineFrequency::FREQUENCY_50HZ,
        media::PowerLineFrequency::FREQUENCY_60HZ,
    };

    capabilities_.noise_reduction_capabilities = {
        rtc::Optional<bool>(), rtc::Optional<bool>(true),
        rtc::Optional<bool>(false),
    };

    default_device_ = capabilities_.device_capabilities[0].get();
    low_res_device_ = capabilities_.device_capabilities[1].get();
    high_res_device_ = capabilities_.device_capabilities[2].get();
    default_closest_format_ = &default_device_->formats[1];
    low_res_closest_format_ = &low_res_device_->formats[2];
    high_res_closest_format_ = &high_res_device_->formats[2];
    high_res_highest_format_ = &high_res_device_->formats[5];
  }

 protected:
  VideoDeviceCaptureSourceSelectionResult SelectSettings() {
    blink::WebMediaConstraints constraints =
        constraint_factory_.CreateWebMediaConstraints();
    return SelectVideoDeviceCaptureSourceSettings(capabilities_, constraints);
  }

  VideoDeviceCaptureCapabilities capabilities_;
  const mojom::VideoInputDeviceCapabilities* default_device_;
  const mojom::VideoInputDeviceCapabilities* low_res_device_;
  const mojom::VideoInputDeviceCapabilities* high_res_device_;
  // Closest formats to the default settings.
  const media::VideoCaptureFormat* default_closest_format_;
  const media::VideoCaptureFormat* low_res_closest_format_;
  const media::VideoCaptureFormat* high_res_closest_format_;
  const media::VideoCaptureFormat* high_res_highest_format_;

  MockConstraintFactory constraint_factory_;
};

// The Unconstrained test checks the default selection criteria.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, Unconstrained) {
  constraint_factory_.Reset();
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Should select the default device with closest-to-default settings.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(default_device_->facing_mode, result.facing_mode);
  EXPECT_EQ(*default_closest_format_, result.Format());
  // Should select default settings for other constraints.
  EXPECT_EQ(media::PowerLineFrequency::FREQUENCY_DEFAULT,
            result.PowerLineFrequency());
  EXPECT_EQ(rtc::Optional<bool>(), result.noise_reduction);
}

// The "Overconstrained" tests verify that failure of any single required
// constraint results in failure to select a candidate.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnDeviceID) {
  constraint_factory_.Reset();
  constraint_factory_.basic().deviceId.setExact(
      blink::WebString::fromASCII("NONEXISTING"));
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().deviceId.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnFacingMode) {
  constraint_factory_.Reset();
  // No device in |capabilities_| has facing mode equal to LEFT.
  constraint_factory_.basic().facingMode.setExact(
      blink::WebString::fromASCII("left"));
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().facingMode.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnVideoKind) {
  constraint_factory_.Reset();
  // No device in |capabilities_| has video kind infrared.
  constraint_factory_.basic().videoKind.setExact(
      blink::WebString::fromASCII("infrared"));
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().videoKind.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnHeight) {
  constraint_factory_.Reset();
  constraint_factory_.basic().height.setExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().height.setMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().height.setMax(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnWidth) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.setExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().width.setMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().width.setMax(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnAspectRatio) {
  constraint_factory_.Reset();
  constraint_factory_.basic().aspectRatio.setExact(123467890.0);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspectRatio.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().aspectRatio.setMin(123467890.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspectRatio.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  // This value is lower than the minimum supported by sources.
  double kLowAspectRatio = 0.01;
  constraint_factory_.basic().aspectRatio.setMax(kLowAspectRatio);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspectRatio.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnFrameRate) {
  constraint_factory_.Reset();
  constraint_factory_.basic().frameRate.setExact(123467890.0);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frameRate.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().frameRate.setMin(123467890.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frameRate.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().frameRate.setMax(0.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frameRate.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnPowerLineFrequency) {
  constraint_factory_.Reset();
  constraint_factory_.basic().googPowerLineFrequency.setExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().googPowerLineFrequency.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().googPowerLineFrequency.setMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().googPowerLineFrequency.name(),
            result.failed_constraint_name);

  constraint_factory_.Reset();
  constraint_factory_.basic().googPowerLineFrequency.setMax(-1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().googPowerLineFrequency.name(),
            result.failed_constraint_name);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnNoiseReduction) {
  // Simulate a system that does not support noise reduction.
  // Manually adding device capabilities because VideoDeviceCaptureCapabilities
  // is move only.
  VideoDeviceCaptureCapabilities capabilities;
  ::mojom::VideoInputDeviceCapabilitiesPtr device =
      ::mojom::VideoInputDeviceCapabilities::New();
  device->device_id = kDeviceID1;
  device->facing_mode = ::mojom::FacingMode::NONE;
  device->formats = {
      media::VideoCaptureFormat(gfx::Size(200, 200), 40.0f,
                                media::PIXEL_FORMAT_I420),
  };
  capabilities.device_capabilities.push_back(std::move(device));
  capabilities.power_line_capabilities = capabilities_.power_line_capabilities;
  capabilities.noise_reduction_capabilities = {rtc::Optional<bool>(false)};

  constraint_factory_.Reset();
  constraint_factory_.basic().googNoiseReduction.setExact(true);
  auto constraints = constraint_factory_.CreateWebMediaConstraints();
  auto result =
      SelectVideoDeviceCaptureSourceSettings(capabilities, constraints);
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().googNoiseReduction.name(),
            result.failed_constraint_name);
}

// The "Mandatory" and "Ideal" tests check that various selection criteria work
// for each individual constraint in the basic constraint set.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryDeviceID) {
  constraint_factory_.Reset();
  constraint_factory_.basic().deviceId.setExact(
      blink::WebString::fromASCII(default_device_->device_id));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(*default_closest_format_, result.Format());
  EXPECT_EQ(media::PowerLineFrequency::FREQUENCY_DEFAULT,
            result.PowerLineFrequency());

  constraint_factory_.basic().deviceId.setExact(
      blink::WebString::fromASCII(low_res_device_->device_id));
  result = SelectSettings();
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  EXPECT_EQ(media::PowerLineFrequency::FREQUENCY_DEFAULT,
            result.PowerLineFrequency());

  constraint_factory_.basic().deviceId.setExact(
      blink::WebString::fromASCII(high_res_device_->device_id));
  result = SelectSettings();
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_closest_format_, result.Format());
  EXPECT_EQ(media::PowerLineFrequency::FREQUENCY_DEFAULT,
            result.PowerLineFrequency());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryFacingMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().facingMode.setExact(
      blink::WebString::fromASCII("environment"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(::mojom::FacingMode::ENVIRONMENT, result.facing_mode);
  // Only the low-res device supports environment facing mode. Should select
  // default settings for everything else.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  EXPECT_EQ(media::PowerLineFrequency::FREQUENCY_DEFAULT,
            result.PowerLineFrequency());

  constraint_factory_.basic().facingMode.setExact(
      blink::WebString::fromASCII("user"));
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(::mojom::FacingMode::USER, result.facing_mode);
  // Only the high-res device supports user facing mode. Should select default
  // settings for everything else.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_closest_format_, result.Format());
  EXPECT_EQ(media::PowerLineFrequency::FREQUENCY_DEFAULT,
            result.PowerLineFrequency());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryVideoKind) {
  constraint_factory_.Reset();
  constraint_factory_.basic().videoKind.setExact(
      blink::WebString::fromASCII("depth"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDeviceID4, result.device_id);
  EXPECT_EQ(media::PIXEL_FORMAT_Y16, result.Format().pixel_format);

  constraint_factory_.basic().videoKind.setExact(
      blink::WebString::fromASCII("color"));
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(default_device_->device_id, result.device_id);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryPowerLineFrequency) {
  constraint_factory_.Reset();
  const media::PowerLineFrequency kPowerLineFrequencies[] = {
      media::PowerLineFrequency::FREQUENCY_50HZ,
      media::PowerLineFrequency::FREQUENCY_60HZ};
  for (auto power_line_frequency : kPowerLineFrequencies) {
    constraint_factory_.basic().googPowerLineFrequency.setExact(
        static_cast<long>(power_line_frequency));
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(power_line_frequency, result.PowerLineFrequency());
    // The default device and settings closest to the default should be
    // selected.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(default_device_->facing_mode, result.facing_mode);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryNoiseReduction) {
  constraint_factory_.Reset();
  const bool kNoiseReductionValues[] = {true, false};
  for (auto noise_reduction : kNoiseReductionValues) {
    constraint_factory_.basic().googNoiseReduction.setExact(noise_reduction);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(noise_reduction, result.noise_reduction);
    // The default device and settings closest to the default should be
    // selected.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(default_device_->facing_mode, result.facing_mode);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactHeight) {
  constraint_factory_.Reset();
  const int kHeight = MediaStreamVideoSource::kDefaultHeight;
  constraint_factory_.basic().height.setExact(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested height. The algorithm
  // should prefer the first device that supports the requested height natively,
  // which is the low-res device.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(kHeight, result.Height());

  const int kLargeHeight = 1500;
  constraint_factory_.basic().height.setExact(kLargeHeight);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device at the highest resolution supports the requested
  // height, even if not natively.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_highest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinHeight) {
  constraint_factory_.Reset();
  const int kHeight = MediaStreamVideoSource::kDefaultHeight;
  constraint_factory_.basic().height.setMin(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested height range. The
  // algorithm should prefer the default device.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_LE(kHeight, result.Height());

  const int kLargeHeight = 1500;
  constraint_factory_.basic().height.setMin(kLargeHeight);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device at the highest resolution supports the requested
  // height range.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_highest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxHeight) {
  constraint_factory_.Reset();
  const int kLowHeight = 20;
  constraint_factory_.basic().height.setMax(kLowHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested height range. The
  // algorithm should prefer the settings that natively exceed the requested
  // maximum by the lowest amount. In this case it is the low-res device.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(low_res_device_->formats[0], result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryHeightRange) {
  constraint_factory_.Reset();
  {
    const int kMinHeight = 480;
    const int kMaxHeight = 720;
    constraint_factory_.basic().height.setMin(kMinHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Height(), kMinHeight);
    EXPECT_LE(result.Height(), kMaxHeight);
    // All devices in |capabilities_| support the constraint range. The
    // algorithm should prefer the default device since it has at least one
    // native format (the closest-to-default format) included in the requested
    // range.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const int kMinHeight = 550;
    const int kMaxHeight = 650;
    constraint_factory_.basic().height.setMin(kMinHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Height(), kMinHeight);
    EXPECT_LE(result.Height(), kMaxHeight);
    // In this case, the algorithm should prefer the low-res device since it is
    // the first device with a native format (800x600) included in the requested
    // range.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
  }

  {
    const int kMinHeight = 700;
    const int kMaxHeight = 800;
    constraint_factory_.basic().height.setMin(kMinHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Height(), kMinHeight);
    EXPECT_LE(result.Height(), kMaxHeight);
    // In this case, the algorithm should prefer the high-res device since it is
    // the only device with a native format (1280x720) included in the requested
    // range.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealHeight) {
  constraint_factory_.Reset();
  {
    const int kIdealHeight = 480;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first device that supports the ideal
    // height natively.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(kIdealHeight, result.Height());
  }

  {
    const int kIdealHeight = 481;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the default device is selected because it can satisfy the
    // ideal at a lower cost than the other devices (500 vs 600 or 720).
    // Note that a native resolution of 480 is further from the ideal than
    // 500 cropped to 480.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const int kIdealHeight = 1079;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the high-res device has two configurations that satisfy
    // the ideal value (1920x1080 and 2304x1536). Select the one with shortest
    // native distance to the ideal value (1920x1080).
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
  }

  {
    const int kIdealHeight = 1200;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must the select the only device that can satisfy the ideal,
    // which is the high-res device at the highest resolution.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(*high_res_highest_format_, result.Format());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactWidth) {
  constraint_factory_.Reset();
  const int kWidth = 640;
  constraint_factory_.basic().width.setExact(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested width. The algorithm
  // should prefer the first device that supports the requested width natively,
  // which is the low-res device.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(kWidth, result.Width());

  const int kLargeWidth = 2000;
  constraint_factory_.basic().width.setExact(kLargeWidth);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_LE(kLargeWidth, result.Width());
  // Only the high-res device at the highest resolution supports the requested
  // width, even if not natively.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_highest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinWidth) {
  constraint_factory_.Reset();
  const int kWidth = 640;
  constraint_factory_.basic().width.setMin(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested width range. The
  // algorithm should prefer the default device at 1000x1000, which is the
  // first configuration that satisfies the minimum width.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_LE(kWidth, result.Width());
  EXPECT_EQ(1000, result.Width());
  EXPECT_EQ(1000, result.Height());

  const int kLargeWidth = 2000;
  constraint_factory_.basic().width.setMin(kLargeWidth);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device at the highest resolution supports the requested
  // minimum width.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_LE(kLargeWidth, result.Width());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxWidth) {
  constraint_factory_.Reset();
  const int kLowWidth = 30;
  constraint_factory_.basic().width.setMax(kLowWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested width range. The
  // algorithm should prefer the settings that natively exceed the requested
  // maximum by the lowest amount. In this case it is the low-res device at its
  // lowest resolution.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(low_res_device_->formats[0], result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryWidthRange) {
  constraint_factory_.Reset();
  {
    const int kMinWidth = 640;
    const int kMaxWidth = 1280;
    constraint_factory_.basic().width.setMin(kMinWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Width(), kMinWidth);
    EXPECT_LE(result.Width(), kMaxWidth);
    // All devices in |capabilities_| support the constraint range. The
    // algorithm should prefer the default device since it has at least one
    // native format (1000x1000) included in the requested range.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(1000, result.Width());
    EXPECT_EQ(1000, result.Height());
  }

  {
    const int kMinWidth = 750;
    const int kMaxWidth = 850;
    constraint_factory_.basic().width.setMin(kMinWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Width(), kMinWidth);
    EXPECT_LE(result.Width(), kMaxWidth);
    // In this case, the algorithm should prefer the low-res device since it is
    // the first device with a native format (800x600) included in the requested
    // range.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
  }

  {
    const int kMinWidth = 1900;
    const int kMaxWidth = 2000;
    constraint_factory_.basic().width.setMin(kMinWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Width(), kMinWidth);
    EXPECT_LE(result.Width(), kMaxWidth);
    // In this case, the algorithm should prefer the high-res device since it is
    // the only device with a native format (1920x1080) included in the
    // requested range.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealWidth) {
  constraint_factory_.Reset();
  {
    const int kIdealWidth = 320;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first device that supports the ideal
    // width natively, which is the low-res device at 320x240.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(kIdealWidth, result.Width());
  }

  {
    const int kIdealWidth = 321;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the default device is selected because it can satisfy the
    // ideal at a lower cost than the other devices (500 vs 640).
    // Note that a native resolution of 320 is further from the ideal value of
    // 321 than 500 cropped to 321.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const int kIdealWidth = 2000;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must the select the only device that can satisfy the ideal.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(*high_res_highest_format_, result.Format());
  }

  {
    const int kIdealWidth = 3000;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must the select the device and setting with less distance
    // to the ideal.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(*high_res_highest_format_, result.Format());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = MediaStreamVideoSource::kDefaultFrameRate;
  constraint_factory_.basic().frameRate.setExact(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested frame rate. The
  // algorithm should prefer the first device that supports the requested frame
  // rate natively, which is the low-res device at 640x480x30Hz.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(kFrameRate, result.FrameRate());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());

  const double kLargeFrameRate = 50;
  constraint_factory_.basic().frameRate.setExact(kLargeFrameRate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device supports the requested frame rate, even if not
  // natively. The least expensive configuration that supports the requested
  // frame rate is 1280x720x60Hz.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_EQ(1280, result.Width());
  EXPECT_EQ(720, result.Height());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = MediaStreamVideoSource::kDefaultFrameRate;
  constraint_factory_.basic().frameRate.setMin(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested frame-rate range. The
  // algorithm should prefer the default device.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  // The format closest to the default satisfies the constraint.
  EXPECT_EQ(*default_closest_format_, result.Format());

  const double kLargeFrameRate = 50;
  constraint_factory_.basic().frameRate.setMin(kLargeFrameRate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device supports the requested frame-rate range.
  // The least expensive configuration is 1280x720x60Hz.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_LE(kLargeFrameRate, result.FrameRate());
  EXPECT_EQ(1280, result.Width());
  EXPECT_EQ(720, result.Height());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxFrameRate) {
  constraint_factory_.Reset();
  const double kLowFrameRate = 10;
  constraint_factory_.basic().frameRate.setMax(kLowFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested frame-rate range. The
  // algorithm should prefer the settings that natively exceed the requested
  // maximum by the lowest amount. In this case it is the high-res device with
  // default resolution .
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(kLowFrameRate, result.FrameRate());
  EXPECT_EQ(MediaStreamVideoSource::kDefaultHeight, result.Height());
  EXPECT_EQ(MediaStreamVideoSource::kDefaultWidth, result.Width());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryFrameRateRange) {
  constraint_factory_.Reset();
  {
    const double kMinFrameRate = 10;
    const double kMaxFrameRate = 40;
    constraint_factory_.basic().frameRate.setMin(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_LE(kMinFrameRate, result.FrameRate());
    EXPECT_GE(kMaxFrameRate, result.FrameRate());
    // All devices in |capabilities_| support the constraint range. The
    // algorithm should prefer the default device since its closest-to-default
    // format has a frame rate included in the requested range.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const double kMinFrameRate = 25;
    const double kMaxFrameRate = 35;
    constraint_factory_.basic().frameRate.setMin(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.FrameRate(), kMinFrameRate);
    EXPECT_LE(result.FrameRate(), kMaxFrameRate);
    // In this case, the algorithm should prefer the low-res device since it is
    // the first device with a native frame rate included in the requested
    // range. The default resolution should be preferred as secondary criterion.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(*low_res_closest_format_, result.Format());
  }

  {
    const double kMinFrameRate = 50;
    const double kMaxFrameRate = 70;
    constraint_factory_.basic().frameRate.setMin(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.FrameRate(), kMinFrameRate);
    EXPECT_LE(result.FrameRate(), kMaxFrameRate);
    // In this case, the algorithm should prefer the high-res device since it is
    // the only device with a native format included in the requested range.
    // The 1280x720 resolution should be selected due to closeness to default
    // settings, which is the second tie-breaker criterion that applies.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealFrameRate) {
  constraint_factory_.Reset();
  {
    const double kIdealFrameRate = MediaStreamVideoSource::kDefaultFrameRate;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first configuration that supports the
    // ideal frame rate natively, which is the low-res device. Default
    // resolution should be selected as secondary criterion.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(*low_res_closest_format_, result.Format());
  }

  {
    const double kIdealFrameRate = 31;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the default device is selected because it can satisfy the
    // ideal at a lower cost than the other devices (40 vs 60).
    // Note that a native frame rate of 30 is further from the ideal than
    // 31 adjusted to 30.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const double kIdealFrameRate = 55;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The high-res device format 1280x720x60.0 must be selected because its
    // frame rate can satisfy the ideal frame rate and has resolution closest
    // to the default.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    EXPECT_EQ(60, result.FrameRate());
  }

  {
    const double kIdealFrameRate = 100;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must select settings with frame rate closest to the ideal.
    // The high-res device format 1280x720x60.0 must be selected because its
    // frame rate it closest to the ideal value and it has resolution closest to
    // the default.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    EXPECT_EQ(60, result.FrameRate());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 4.0 / 3.0;
  constraint_factory_.basic().aspectRatio.setExact(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  double min_width = 1.0;
  double max_width = result.Width();
  double min_height = 1.0;
  double max_height = result.Height();
  double min_aspect_ratio = min_width / max_height;
  double max_aspect_ratio = max_width / min_height;
  // The requested aspect ratio must be within the supported range.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // All devices in |capabilities_| support the requested aspect ratio.
  // The algorithm should prefer the first device that supports the requested
  // aspect ratio.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(*default_closest_format_, result.Format());

  const int kMinWidth = 500;
  const int kMaxWidth = 1000;
  const int kMaxHeight = 500;
  constraint_factory_.basic().height.setMax(kMaxHeight);
  constraint_factory_.basic().width.setMin(kMinWidth);
  constraint_factory_.basic().width.setMax(kMaxWidth);
  constraint_factory_.basic().aspectRatio.setExact(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  min_width = std::max(1, kMinWidth);
  max_width = std::min(result.Width(), kMaxWidth);
  min_height = 1.0;
  max_height = std::min(result.Height(), kMaxHeight);
  min_aspect_ratio = min_width / max_height;
  max_aspect_ratio = max_width / min_height;
  // The requested aspect ratio must be within the supported range.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // The default device can support the requested aspect ratio with the default
  // settings (500x500) using cropping.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(*default_closest_format_, result.Format());

  const int kMinHeight = 480;
  constraint_factory_.basic().height.setMin(kMinHeight);
  constraint_factory_.basic().height.setMax(kMaxHeight);
  constraint_factory_.basic().width.setMin(kMinWidth);
  constraint_factory_.basic().width.setMax(kMaxWidth);
  constraint_factory_.basic().aspectRatio.setExact(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  min_width = std::max(1, kMinWidth);
  max_width = std::min(result.Width(), kMaxWidth);
  min_height = std::max(1, kMinHeight);
  max_height = std::min(result.Height(), kMaxHeight);
  min_aspect_ratio = min_width / max_height;
  max_aspect_ratio = max_width / min_height;
  // The requested aspect ratio must be within the supported range.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // Given resolution constraints, the default device with closest-to-default
  // settings cannot satisfy the required aspect ratio.
  // The first device that can do it is the low-res device with a native
  // resolution of 640x480. Higher resolutions for the default device are more
  // penalized by the constraints than the default native resolution of the
  // low-res device.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(*low_res_closest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 4.0 / 3.0;
  constraint_factory_.basic().aspectRatio.setMin(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  double max_width = result.Width();
  double min_height = 1.0;
  double max_aspect_ratio = max_width / min_height;
  // Minimum constraint aspect ratio must be less than or equal to the maximum
  // supported by the source.
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // All devices in |capabilities_| support the requested aspect-ratio range.
  // The algorithm should prefer the first device that supports the requested
  // aspect-ratio range, which in this case is the default device.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(*default_closest_format_, result.Format());

  const int kMinWidth = 500;
  const int kMaxWidth = 1000;
  const int kMinHeight = 480;
  const int kMaxHeight = 500;
  constraint_factory_.basic().width.setMin(kMinWidth);
  constraint_factory_.basic().width.setMax(kMaxWidth);
  constraint_factory_.basic().height.setMin(kMinHeight);
  constraint_factory_.basic().height.setMax(kMaxHeight);
  constraint_factory_.basic().aspectRatio.setMin(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  max_width = std::min(result.Width(), kMaxWidth);
  min_height = std::max(1, kMinHeight);
  max_aspect_ratio = max_width / min_height;
  // Minimum constraint aspect ratio must be less than or equal to the minimum
  // supported by the source.
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // Given resolution constraints, the default device with closest-to-default
  // settings cannot satisfy the required minimum aspect ratio (maximum would
  // be 500/480).  The first device that can is the low-res device with a native
  // resolution of 640x480.
  // Higher resolutions for the default device are more penalized by the
  // constraints than the default native resolution of the low-res device.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(*low_res_closest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 0.5;
  constraint_factory_.basic().aspectRatio.setMax(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  double min_width = 1.0;
  double max_height = result.Height();
  double min_aspect_ratio = min_width / max_height;
  // Minimum constraint aspect ratio must be less than or equal to the maximum
  // supported by the source.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  // All devices in |capabilities_| support the requested aspect-ratio range.
  // The algorithm should prefer the first device that supports the requested
  // aspect-ratio range, which in this case is the default device.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(*default_closest_format_, result.Format());

  const int kExactWidth = 360;
  const int kMinHeight = 360;
  const int kMaxHeight = 720;
  constraint_factory_.basic().width.setExact(kExactWidth);
  constraint_factory_.basic().height.setMin(kMinHeight);
  constraint_factory_.basic().height.setMax(kMaxHeight);
  constraint_factory_.basic().aspectRatio.setMax(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  min_width = std::max(1, kExactWidth);
  max_height = std::min(result.Height(), kMaxHeight);
  min_aspect_ratio = min_width / max_height;
  // Minimum constraint aspect ratio must be less than or equal to the minimum
  // supported by the source.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  // Given resolution constraints, the default device with closest-to-default
  // settings cannot satisfy the required maximum aspect ratio (maximum would
  // be 360/500).
  // The high-res device with a native resolution of 1280x720 can support
  // 360x720 with cropping with less penalty than the default device at
  // 1000x1000.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(1280, result.Width());
  EXPECT_EQ(720, result.Height());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryAspectRatioRange) {
  constraint_factory_.Reset();
  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 1.0;

    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    double min_width = 1.0;
    double max_width = result.Width();
    double min_height = 1.0;
    double max_height = result.Height();
    double min_aspect_ratio = min_width / max_height;
    double max_aspect_ratio = max_width / min_height;
    // Constraint aspect-ratio range must have nonempty intersection with
    // supported range.
    EXPECT_LE(kMinAspectRatio, max_aspect_ratio);
    EXPECT_GE(kMaxAspectRatio, min_aspect_ratio);
    // All devices in |capabilities_| support the requested aspect-ratio range.
    // The algorithm should prefer the first device that supports the requested
    // aspect-ratio range, which in this case is the default device.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const double kMinAspectRatio = 3.0;
    const double kMaxAspectRatio = 4.0;

    const long kExactHeight = 600;
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(kExactHeight);
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    double min_width = 1.0;
    double max_width = result.Width();
    double min_height = 1.0;
    double max_height = result.Height();
    double min_aspect_ratio = min_width / max_height;
    double max_aspect_ratio = max_width / min_height;
    // Constraint aspect-ratio range must have nonempty intersection with
    // supported range.
    EXPECT_LE(kMinAspectRatio, max_aspect_ratio);
    EXPECT_GE(kMaxAspectRatio, min_aspect_ratio);
    // The only device that supports the resolution and aspect ratio constraint
    // is the high-res device. The 1920x1080 is the least expensive format.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealAspectRatio) {
  constraint_factory_.Reset();
  {
    const double kIdealAspectRatio = 0.5;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    double min_width = 1.0;
    double max_width = result.Width();
    double min_height = 1.0;
    double max_height = result.Height();
    double min_aspect_ratio = min_width / max_height;
    double max_aspect_ratio = max_width / min_height;
    // All devices in |capabilities_| support the ideal aspect-ratio.
    // The algorithm should prefer the default device with closest-to-default
    // settings.
    EXPECT_LE(kIdealAspectRatio, max_aspect_ratio);
    EXPECT_GE(kIdealAspectRatio, min_aspect_ratio);
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_EQ(*default_closest_format_, result.Format());
  }

  {
    const double kIdealAspectRatio = 1500.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The only device that supports the ideal aspect ratio is the high-res
    // device. The least expensive way to support it with the 1920x1080 format
    // cropped to 1500x1.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
  }

  {
    const double kIdealAspectRatio = 2000.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The only device that supports the ideal aspect ratio is the high-res
    // device with its highest resolution, cropped to 2000x1.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(*high_res_highest_format_, result.Format());
  }

  {
    const double kIdealAspectRatio = 4000.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The configuration closest to the ideal aspect ratio is is the high-res
    // device with its highest resolution, cropped to 2304x1.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(*high_res_highest_format_, result.Format());
  }

  {
    const double kIdealAspectRatio = 2.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    constraint_factory_.basic().height.setExact(400);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The first device to support the ideal aspect ratio and the resolution
    // constraint is the low-res device. The 800x600 format cropped to 800x400
    // is the lest expensive way to achieve it.
    EXPECT_EQ(low_res_device_->device_id, result.device_id);
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
  }

  {
    const double kIdealAspectRatio = 3.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    constraint_factory_.basic().height.setExact(400);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The only device that supports the ideal aspect ratio and the resolution
    // constraint is the high-res device. The 1280x720 cropped to 1200x400 is
    // the lest expensive way to achieve it.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
  }
}

// The "Advanced" tests check selection criteria involving advanced constraint
// sets.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedMinMaxResolutionFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMin(4000);
  advanced1.height.setMin(4000);
  // No device supports the first advanced set. This first advanced constraint
  // set is therefore ignored in all calls to SelectSettings().
  // Tie-breaker rule that applies is closeness to default settings.
  auto result = SelectSettings();
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(*default_closest_format_, result.Format());

  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMin(320);
  advanced2.height.setMin(240);
  advanced2.width.setMax(640);
  advanced2.height.setMax(480);
  result = SelectSettings();
  // The device that best supports this advanced set is the low-res device,
  // which natively supports the maximum resolution.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());

  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frameRate.setMax(10.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device natively supports the third advanced set in addition
  // to the previous set and should be selected.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());

  blink::WebMediaTrackConstraintSet& advanced4 =
      constraint_factory_.AddAdvanced();
  advanced4.width.setMax(1000);
  advanced4.height.setMax(1000);
  result = SelectSettings();
  // Even though the default device supports the resolution in the fourth
  // advanced natively, having better support for the previous sets has
  // precedence, so the high-res device is selected.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());

  constraint_factory_.basic().width.setIdeal(100);
  constraint_factory_.basic().height.setIdeal(100);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device at 600x400@10Hz supports all advanced sets and is
  // better at supporting the ideal value.
  // It beats 320x240@30Hz because the penalty for the native frame rate takes
  // precedence over the native fitness distance.
  // Both support standard fitness distance equally, since 600x400 can be
  // cropped to 320x240.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(600, result.Width());
  EXPECT_EQ(400, result.Height());

  constraint_factory_.basic().width.setIdeal(2000);
  constraint_factory_.basic().height.setIdeal(1500);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device at 640x480@10Hz is closer to the large ideal
  // resolution.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedResolutionAndFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setExact(1920);
  advanced1.height.setExact(1080);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frameRate.setExact(60.0);
  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.width.setExact(2304);
  advanced3.height.setExact(1536);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device is the only one that satisfies the first advanced
  // set. 2304x1536x10.0 satisfies sets 1 and 3, while 1920x1080x60.0
  // satisfies sets 1, and 2. The latter must be selected, regardless of
  // any other criteria.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(1920, result.Width());
  EXPECT_EQ(1080, result.Height());
  EXPECT_EQ(60.0, result.FrameRate());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedNoiseReduction) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMin(640);
  advanced1.height.setMin(480);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMin(1920);
  advanced2.height.setMin(1080);
  advanced2.googNoiseReduction.setExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_LE(1920, result.Width());
  EXPECT_LE(1080, result.Height());
  EXPECT_TRUE(result.noise_reduction && !*result.noise_reduction);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryNoiseReduction) {
  {
    constraint_factory_.Reset();
    blink::WebMediaTrackConstraintSet& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.width.setMin(640);
    advanced1.height.setMin(480);
    advanced1.googNoiseReduction.setExact(true);
    blink::WebMediaTrackConstraintSet& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.setMin(1920);
    advanced2.height.setMin(1080);
    advanced2.googNoiseReduction.setExact(false);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The second advanced set cannot be satisfied because it contradicts the
    // first set. The default device supports the first set and should be
    // selected.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_LE(640, result.Width());
    EXPECT_LE(480, result.Height());
    EXPECT_TRUE(result.noise_reduction && *result.noise_reduction);
  }

  // Same test without noise reduction
  {
    constraint_factory_.Reset();
    blink::WebMediaTrackConstraintSet& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.width.setMin(640);
    advanced1.height.setMin(480);
    blink::WebMediaTrackConstraintSet& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.setMin(1920);
    advanced2.height.setMin(1080);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Only the high-res device can satisfy the second advanced set.
    EXPECT_EQ(high_res_device_->device_id, result.device_id);
    EXPECT_LE(1920, result.Width());
    EXPECT_LE(1080, result.Height());
    // Should select default noise reduction setting.
    EXPECT_TRUE(!result.noise_reduction);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryExactResolution) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setExact(640);
  advanced1.height.setExact(480);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setExact(1920);
  advanced2.height.setExact(1080);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. The low-res device is the one that best supports the requested
  // resolution.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryMaxMinResolutionFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMax(640);
  advanced1.height.setMax(480);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMin(1920);
  advanced2.height.setMin(1080);
  advanced2.frameRate.setExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. The default device with the 200x200@40Hz format should be selected.
  // That format satisfies the first advanced set as well as any other, so the
  // tie breaker rule that applies is default device ID.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(200, result.Width());
  EXPECT_EQ(200, result.Height());
  EXPECT_EQ(40, result.FrameRate());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryMinMaxResolutionFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMin(800);
  advanced1.height.setMin(600);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMax(640);
  advanced2.height.setMax(480);
  advanced2.frameRate.setExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. The default device with the 1000x1000@20Hz format should be selected.
  // That format satisfies the first advanced set as well as any other, so the
  // tie breaker rule that applies is default device ID.
  EXPECT_EQ(default_device_->device_id, result.device_id);
  EXPECT_EQ(1000, result.Width());
  EXPECT_EQ(1000, result.Height());
  EXPECT_EQ(20, result.FrameRate());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryExactAspectRatio) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.aspectRatio.setExact(2300.0);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.aspectRatio.setExact(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. Only the high-res device in the highest-resolution format supports the
  // requested aspect ratio.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_highest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryAspectRatioRange) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.aspectRatio.setMin(2300.0);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.aspectRatio.setMax(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. Only the high-res device in the highest-resolution format supports the
  // requested aspect ratio.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(*high_res_highest_format_, result.Format());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryExactFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.frameRate.setExact(40.0);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frameRate.setExact(45.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_EQ(40.0, result.FrameRate());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryFrameRateRange) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.frameRate.setMin(40.0);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frameRate.setMax(35.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_LE(40.0, result.FrameRate());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryWidthFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMax(1920);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMin(2000);
  advanced2.frameRate.setExact(10.0);
  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frameRate.setExact(30.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The low-res device at 320x240@30Hz satisfies advanced sets 1 and 3.
  // The high-res device at 2304x1536@10.0f can satisfy sets 1 and 2, but not
  // both at the same time. Thus, low-res device must be preferred.
  EXPECT_EQ(low_res_device_->device_id, result.device_id);
  EXPECT_EQ(30.0, result.FrameRate());
  EXPECT_GE(1920, result.Width());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryHeightFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.height.setMax(1080);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.height.setMin(1500);
  advanced2.frameRate.setExact(10.0);
  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frameRate.setExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device at 1280x768@60Hz and 1920x1080@60Hz satisfies advanced
  // sets 1 and 3. The same device at 2304x1536@10.0f can satisfy sets 1 and 2,
  // but not both at the same time. Thus, the format closest to default that
  // satisfies sets 1 and 3 must be chosen.
  EXPECT_EQ(high_res_device_->device_id, result.device_id);
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_GE(1080, result.Height());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedDeviceID) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  blink::WebString id_vector1[] = {blink::WebString::fromASCII(kDeviceID1),
                                   blink::WebString::fromASCII(kDeviceID2)};
  advanced1.deviceId.setExact(
      blink::WebVector<blink::WebString>(id_vector1, arraysize(id_vector1)));
  blink::WebString id_vector2[] = {blink::WebString::fromASCII(kDeviceID2),
                                   blink::WebString::fromASCII(kDeviceID3)};
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.deviceId.setExact(
      blink::WebVector<blink::WebString>(id_vector2, arraysize(id_vector2)));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kDeviceID2 must be selected because it is the only one that satisfies both
  // advanced sets.
  EXPECT_EQ(std::string(kDeviceID2), result.device_id);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryDeviceID) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  blink::WebString id_vector1[] = {blink::WebString::fromASCII(kDeviceID1),
                                   blink::WebString::fromASCII(kDeviceID2)};
  advanced1.deviceId.setExact(
      blink::WebVector<blink::WebString>(id_vector1, arraysize(id_vector1)));
  blink::WebString id_vector2[] = {blink::WebString::fromASCII(kDeviceID3),
                                   blink::WebString::fromASCII(kDeviceID4)};
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.deviceId.setExact(
      blink::WebVector<blink::WebString>(id_vector2, arraysize(id_vector2)));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_EQ(std::string(kDeviceID1), result.device_id);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryPowerLineFrequency) {
  {
    constraint_factory_.Reset();
    blink::WebMediaTrackConstraintSet& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.width.setMin(640);
    advanced1.height.setMin(480);
    advanced1.googPowerLineFrequency.setExact(50);
    blink::WebMediaTrackConstraintSet& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.setMin(1920);
    advanced2.height.setMin(1080);
    advanced2.googPowerLineFrequency.setExact(60);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The second advanced set cannot be satisfied because it contradicts the
    // first set. The default device supports the first set and should be
    // selected.
    EXPECT_EQ(default_device_->device_id, result.device_id);
    EXPECT_LE(640, result.Width());
    EXPECT_LE(480, result.Height());
    EXPECT_EQ(50, static_cast<int>(result.PowerLineFrequency()));
  }
}

// The "NoDevices" tests verify that the algorithm returns the expected result
// when there are no candidates to choose from.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, NoDevicesNoConstraints) {
  constraint_factory_.Reset();
  VideoDeviceCaptureCapabilities capabilities;
  auto result = SelectVideoDeviceCaptureSourceSettings(
      capabilities, constraint_factory_.CreateWebMediaConstraints());
  EXPECT_FALSE(result.HasValue());
  EXPECT_TRUE(std::string(result.failed_constraint_name).empty());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, NoDevicesWithConstraints) {
  constraint_factory_.Reset();
  constraint_factory_.basic().height.setExact(100);
  VideoDeviceCaptureCapabilities capabilities;
  auto result = SelectVideoDeviceCaptureSourceSettings(
      capabilities, constraint_factory_.CreateWebMediaConstraints());
  EXPECT_FALSE(result.HasValue());
  EXPECT_TRUE(std::string(result.failed_constraint_name).empty());
}

}  // namespace content
