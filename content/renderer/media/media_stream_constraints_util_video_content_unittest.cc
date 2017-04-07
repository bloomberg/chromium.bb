// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util_video_content.h"

#include <cmath>
#include <string>

#include "content/renderer/media/mock_constraint_factory.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"

namespace content {

namespace {

void CheckNonResolutionDefaults(const VideoCaptureSettings& result) {
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
}

void CheckNonFrameRateDefaults(const VideoCaptureSettings& result) {
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
}

void CheckTrackAdapterSettingsEqualsFormat(const VideoCaptureSettings& result) {
  // For content capture, resolution and frame rate should always be the same
  // for source and track.
  EXPECT_EQ(result.Width(), result.track_adapter_settings().max_width);
  EXPECT_EQ(result.Height(), result.track_adapter_settings().max_height);
  EXPECT_EQ(0.0, result.track_adapter_settings().max_frame_rate);
}

void CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(
    const VideoCaptureSettings& result) {
  EXPECT_EQ(
      static_cast<double>(kMinScreenCastDimension) / kMaxScreenCastDimension,
      result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

}  // namespace

class MediaStreamConstraintsUtilVideoContentTest : public testing::Test {
 protected:
  VideoCaptureSettings SelectSettings() {
    blink::WebMediaConstraints constraints =
        constraint_factory_.CreateWebMediaConstraints();
    return SelectSettingsVideoContentCapture(constraints);
  }

  MockConstraintFactory constraint_factory_;
};

// The Unconstrained test checks the default selection criteria.
TEST_F(MediaStreamConstraintsUtilVideoContentTest, Unconstrained) {
  constraint_factory_.Reset();
  auto result = SelectSettings();

  // All settings should have default values.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

// The "Overconstrained" tests verify that failure of any single required
// constraint results in failure to select a candidate.
TEST_F(MediaStreamConstraintsUtilVideoContentTest, OverconstrainedOnHeight) {
  constraint_factory_.Reset();
  constraint_factory_.basic().height.setExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().height.setMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().height.setMax(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.name(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, OverconstrainedOnWidth) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.setExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().width.setMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().width.setMax(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.name(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       OverconstrainedOnAspectRatio) {
  constraint_factory_.Reset();
  constraint_factory_.basic().aspectRatio.setExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspectRatio.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().aspectRatio.setMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspectRatio.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().aspectRatio.setMax(0.00001);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspectRatio.name(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, OverconstrainedOnFrameRate) {
  constraint_factory_.Reset();
  constraint_factory_.basic().frameRate.setExact(123467890.0);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frameRate.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().frameRate.setMin(123467890.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frameRate.name(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().frameRate.setMax(0.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frameRate.name(),
            result.failed_constraint_name());
}

// The "Mandatory" and "Ideal" tests check that various selection criteria work
// for each individual constraint in the basic constraint set.
TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryDeviceID) {
  const std::string kDeviceID = "Some ID";
  constraint_factory_.Reset();
  constraint_factory_.basic().deviceId.setExact(
      blink::WebString::fromASCII(kDeviceID));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDeviceID, result.device_id());
  // Other settings should have default values.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealDeviceID) {
  const std::string kDeviceID = "Some ID";
  const std::string kIdealID = "Ideal ID";
  blink::WebVector<blink::WebString> device_ids(static_cast<size_t>(2));
  device_ids[0] = blink::WebString::fromASCII(kDeviceID);
  device_ids[1] = blink::WebString::fromASCII(kIdealID);
  constraint_factory_.Reset();
  constraint_factory_.basic().deviceId.setExact(device_ids);

  blink::WebVector<blink::WebString> ideal_id(static_cast<size_t>(1));
  ideal_id[0] = blink::WebString::fromASCII(kIdealID);
  constraint_factory_.basic().deviceId.setIdeal(ideal_id);

  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kIdealID, result.device_id());
  // Other settings should have default values.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryNoiseReduction) {
  constraint_factory_.Reset();
  const bool kNoiseReductionValues[] = {true, false};
  for (auto noise_reduction : kNoiseReductionValues) {
    constraint_factory_.basic().googNoiseReduction.setExact(noise_reduction);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(noise_reduction, result.noise_reduction());
    // Other settings should have default values.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_EQ(std::string(), result.device_id());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealNoiseReduction) {
  constraint_factory_.Reset();
  const bool kNoiseReductionValues[] = {true, false};
  for (auto noise_reduction : kNoiseReductionValues) {
    constraint_factory_.basic().googNoiseReduction.setIdeal(noise_reduction);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(noise_reduction, result.noise_reduction());
    // Other settings should have default values.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_EQ(std::string(), result.device_id());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactHeight) {
  constraint_factory_.Reset();
  const int kHeight = 1000;
  constraint_factory_.basic().height.setExact(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kHeight, result.Height());
  // The algorithm tries to preserve the default aspect ratio.
  EXPECT_EQ(std::round(kHeight * kDefaultScreenCastAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(1.0 / kHeight, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kHeight,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinHeight) {
  constraint_factory_.Reset();
  const int kHeight = 2000;
  constraint_factory_.basic().height.setMin(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kHeight is greater that the default, so expect kHeight.
  EXPECT_EQ(kHeight, result.Height());
  EXPECT_EQ(std::round(kHeight * kDefaultScreenCastAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(1.0 / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kHeight,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  const int kSmallHeight = 100;
  constraint_factory_.basic().height.setMin(kSmallHeight);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallHeight is less that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(1.0 / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kSmallHeight,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxHeight) {
  // kMaxHeight smaller than the default.
  {
    constraint_factory_.Reset();
    const int kMaxHeight = kDefaultScreenCastHeight - 100;
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxHeight greater than the default.
  {
    constraint_factory_.Reset();
    const int kMaxHeight = kDefaultScreenCastHeight + 100;
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxHeight greater than the maximum allowed.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMax(kMaxScreenCastDimension + 100);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(
        std::round(kDefaultScreenCastHeight * kDefaultScreenCastAspectRatio),
        result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryHeightRange) {
  // Range includes the default.
  {
    constraint_factory_.Reset();
    const int kMinHeight = kDefaultScreenCastHeight - 100;
    const int kMaxHeight = kDefaultScreenCastHeight + 100;
    constraint_factory_.basic().height.setMin(kMinHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The whole range is greater than the default.
  {
    constraint_factory_.Reset();
    const int kMinHeight = kDefaultScreenCastHeight + 100;
    const int kMaxHeight = kDefaultScreenCastHeight + 200;
    constraint_factory_.basic().height.setMin(kMinHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The whole range is less than the default.
  {
    constraint_factory_.Reset();
    const int kMinHeight = kDefaultScreenCastHeight - 200;
    const int kMaxHeight = kDefaultScreenCastHeight - 100;
    constraint_factory_.basic().height.setMin(kMinHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealHeight) {
  // Unconstrained.
  {
    constraint_factory_.Reset();
    const int kIdealHeight = 1000;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealHeight, result.Height());
    // When ideal height is given, the algorithm returns a width that is closest
    // to height * kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kIdealHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const int kIdealHeight = 1000;
    const int kMaxHeight = 800;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal height is greater than the maximum, expect maximum.
    EXPECT_EQ(kMaxHeight, result.Height());
    // Expect closest to kMaxHeight * kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const int kIdealHeight = 1000;
    const int kMinHeight = 1200;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    constraint_factory_.basic().height.setMin(kMinHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal height is less than the minimum, expect minimum.
    EXPECT_EQ(kMinHeight, result.Height());
    // Expect closest to kMinHeight * kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMinHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal intersects a box.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(500);
    constraint_factory_.basic().height.setMax(1000);
    constraint_factory_.basic().width.setMin(100);
    constraint_factory_.basic().width.setMax(500);
    const int kIdealHeight = 750;
    constraint_factory_.basic().height.setIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal height is included in the bounding box.
    EXPECT_EQ(kIdealHeight, result.Height());
    double default_aspect_ratio =
        static_cast<double>(constraint_factory_.basic().width.max()) /
        constraint_factory_.basic().height.max();
    // Expect width closest to kIdealHeight * default aspect ratio.
    EXPECT_EQ(std::round(kIdealHeight * default_aspect_ratio), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / 1000.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(500.0 / 500.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the box, closest to the side coinciding with max height.
  {
    const int kMaxHeight = 1000;
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(500);
    constraint_factory_.basic().height.setMax(kMaxHeight);
    constraint_factory_.basic().width.setMin(100);
    constraint_factory_.basic().width.setMax(500);
    constraint_factory_.basic().height.setIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    // Expect width closest to kMaxHeight * default aspect ratio, which is
    // outside the box. Closest it max width.
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(500.0 / 500.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained set, closest to a single point.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(500);
    constraint_factory_.basic().height.setMax(1000);
    constraint_factory_.basic().width.setMin(500);
    constraint_factory_.basic().width.setMax(1000);
    constraint_factory_.basic().aspectRatio.setMin(1.0);
    constraint_factory_.basic().height.setIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // (max-height, max-width) is the single point closest to the ideal line.
    EXPECT_EQ(constraint_factory_.basic().height.max(), result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1000.0 / 500.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactWidth) {
  constraint_factory_.Reset();
  const int kWidth = 1000;
  constraint_factory_.basic().width.setExact(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kWidth, result.Width());
  EXPECT_EQ(std::round(kWidth / kDefaultScreenCastAspectRatio),
            result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kWidth) / kMinScreenCastDimension,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinWidth) {
  constraint_factory_.Reset();
  const int kWidth = 3000;
  constraint_factory_.basic().width.setMin(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kWidth is greater that the default, so expect kWidth.
  EXPECT_EQ(kWidth, result.Width());
  EXPECT_EQ(std::round(kWidth / kDefaultScreenCastAspectRatio),
            result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  const int kSmallWidth = 100;
  constraint_factory_.basic().width.setMin(kSmallWidth);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallWidth is less that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kSmallWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxWidth) {
  // kMaxWidth less than the default.
  {
    constraint_factory_.Reset();
    const int kMaxWidth = kDefaultScreenCastWidth - 100;
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // kSmallWidth is less that the default, so expect kSmallWidth.
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxWidth greater than the default.
  {
    constraint_factory_.Reset();
    const int kMaxWidth = kDefaultScreenCastWidth + 100;
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // kSmallWidth is less that the default, so expect kSmallWidth.
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxWidth greater than the maximum allowed (gets ignored).
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setMax(kMaxScreenCastDimension + 100);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // kSmallWidth is less that the default, so expect kSmallWidth.
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(
        std::round(kDefaultScreenCastWidth / kDefaultScreenCastAspectRatio),
        result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(
        static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
        result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryWidthRange) {
  // The whole range is less than the default.
  {
    constraint_factory_.Reset();
    const int kMinWidth = kDefaultScreenCastWidth - 200;
    const int kMaxWidth = kDefaultScreenCastWidth - 100;
    constraint_factory_.basic().width.setMin(kMinWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The range includes the default.
  {
    constraint_factory_.Reset();
    const int kMinWidth = kDefaultScreenCastWidth - 100;
    const int kMaxWidth = kDefaultScreenCastWidth + 100;
    constraint_factory_.basic().width.setMin(kMinWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The whole range is greater than the default.
  {
    constraint_factory_.Reset();
    const int kMinWidth = kDefaultScreenCastWidth + 100;
    const int kMaxWidth = kDefaultScreenCastWidth + 200;
    constraint_factory_.basic().width.setMin(kMinWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealWidth) {
  // Unconstrained
  {
    constraint_factory_.Reset();
    const int kIdealWidth = 1000;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealWidth, result.Width());
    // When ideal width is given, the algorithm returns a height that is closest
    // to width / kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kIdealWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const int kIdealWidth = 1000;
    const int kMaxWidth = 800;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    // Expect closest to kMaxWidth / kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const int kIdealWidth = 1000;
    const int kMinWidth = 1200;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    constraint_factory_.basic().width.setMin(kMinWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMinWidth, result.Width());
    // Expect closest to kMinWidth / kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMinWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(
        static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
        result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal intersects a box.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setMin(500);
    constraint_factory_.basic().width.setMax(1000);
    constraint_factory_.basic().height.setMin(100);
    constraint_factory_.basic().height.setMax(500);
    const int kIdealWidth = 750;
    constraint_factory_.basic().width.setIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal width is included in the bounding box.
    EXPECT_EQ(kIdealWidth, result.Width());
    // Expect height closest to kIdealWidth / default aspect ratio.
    double default_aspect_ratio =
        static_cast<double>(constraint_factory_.basic().width.max()) /
        constraint_factory_.basic().height.max();
    EXPECT_EQ(std::round(kIdealWidth / default_aspect_ratio), result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(500.0 / 500.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1000.0 / 100.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the box, closest to the side coinciding with max width.
  {
    const int kMaxWidth = 1000;
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setMin(500);
    constraint_factory_.basic().width.setMax(kMaxWidth);
    constraint_factory_.basic().height.setMin(100);
    constraint_factory_.basic().height.setMax(500);
    constraint_factory_.basic().width.setIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    // kMaxWidth / kDefaultScreenCastAspectRatio is outside the box. Closest is
    // max height.
    EXPECT_EQ(constraint_factory_.basic().height.max(), result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(500.0 / 500.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(static_cast<double>(kMaxWidth) / 100.0,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained set, closest to a single point.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setMin(100);
    constraint_factory_.basic().width.setMax(500);
    constraint_factory_.basic().height.setMin(100);
    constraint_factory_.basic().height.setMax(500);
    constraint_factory_.basic().aspectRatio.setMax(1.0);
    constraint_factory_.basic().width.setIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // (max-width, max-height) is the single point closest to the ideal line.
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    EXPECT_EQ(constraint_factory_.basic().height.max(), result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / 500.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 2.0;
  constraint_factory_.basic().aspectRatio.setExact(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Given that the default aspect ratio cannot be preserved, the algorithm
  // tries to preserve, among the default height or width, the one that leads
  // to highest area. In this case, height is preserved.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * kAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 2.0;
  constraint_factory_.basic().aspectRatio.setMin(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kAspectRatio is greater that the default, so expect kAspectRatio.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * kAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) /
                static_cast<double>(kMinScreenCastDimension),
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  const double kSmallAspectRatio = 0.5;
  constraint_factory_.basic().aspectRatio.setMin(kSmallAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallAspectRatio is less that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kSmallAspectRatio,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) /
                static_cast<double>(kMinScreenCastDimension),
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 2.0;
  constraint_factory_.basic().aspectRatio.setMax(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kAspectRatio is greater that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kMinScreenCastDimension) /
                static_cast<double>(kMaxScreenCastDimension),
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  const double kSmallAspectRatio = 0.5;
  constraint_factory_.basic().aspectRatio.setMax(kSmallAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallAspectRatio is less that the default, so expect kSmallAspectRatio.
  // Prefer to preserve default width since that leads to larger area than
  // preserving default height.
  EXPECT_EQ(std::round(kDefaultScreenCastWidth / kSmallAspectRatio),
            result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kMinScreenCastDimension) /
                static_cast<double>(kMaxScreenCastDimension),
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(kSmallAspectRatio,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryRangeAspectRatio) {
  constraint_factory_.Reset();
  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 2.0;
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Range includes default, so expect the default.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  {
    const double kMinAspectRatio = 2.0;
    const double kMaxAspectRatio = 3.0;
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The whole range is greater than the default. Expect the minimum.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(std::round(kDefaultScreenCastHeight * kMinAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 1.0;
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The whole range is less than the default. Expect the maximum.
    EXPECT_EQ(std::round(kDefaultScreenCastWidth / kMaxAspectRatio),
              result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealAspectRatio) {
  // Unconstrained.
  {
    constraint_factory_.Reset();
    const double kIdealAspectRatio = 2.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(std::round(kDefaultScreenCastHeight * kIdealAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const double kIdealAspectRatio = 2.0;
    const double kMaxAspectRatio = 1.5;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect ratio is greater than the maximum, expect maximum.
    EXPECT_EQ(std::round(kDefaultScreenCastWidth / kMaxAspectRatio),
              result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(
        static_cast<double>(kMinScreenCastDimension) / kMaxScreenCastDimension,
        result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const double kIdealAspectRatio = 1.0;
    const double kMinAspectRatio = 1.5;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect ratio is less than the maximum, expect minimum.
    EXPECT_EQ(std::round(kDefaultScreenCastWidth / kMinAspectRatio),
              result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(
        static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
        result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal intersects a box.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(100);
    constraint_factory_.basic().height.setMax(500);
    constraint_factory_.basic().width.setMin(100);
    constraint_factory_.basic().width.setMax(500);
    const double kIdealAspectRatio = 2.0;
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect-ratio is included in the bounding box, with the value
    // closest to a standard width or height being the cut with the maximum
    // width.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.max() / kIdealAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / 500.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(500.0 / 100.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().height.setMin(1000);
    constraint_factory_.basic().height.setMax(5000);
    constraint_factory_.basic().width.setMin(1000);
    constraint_factory_.basic().width.setMax(5000);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect-ratio is included in the bounding box.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.max() / kIdealAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1000.0 / 5000.0,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(5000.0 / 1000.0,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.Reset();
    constraint_factory_.basic().aspectRatio.setIdeal(kIdealAspectRatio);
    constraint_factory_.basic().height.setMin(250);
    constraint_factory_.basic().width.setMin(250);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect-ratio is included in the bounding box. Preserving default
    // height leads to larger area than preserving default width.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastHeight * kIdealAspectRatio, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(250.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxScreenCastDimension / 250.0,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained area, closest to min or max aspect ratio.
  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 2.0;
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(100);
    constraint_factory_.basic().height.setMax(500);
    constraint_factory_.basic().width.setMin(100);
    constraint_factory_.basic().width.setMax(500);
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    constraint_factory_.basic().aspectRatio.setIdeal(3.0);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMaxAspectRatio.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.max() / kMaxAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().aspectRatio.setIdeal(0.3);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMinAspectRatio.
    EXPECT_EQ(constraint_factory_.basic().height.max(), result.Height());
    EXPECT_EQ(
        std::round(constraint_factory_.basic().height.max() * kMinAspectRatio),
        result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    // Use a box that is bigger and further from the origin to force closeness
    // to a different default dimension.
    constraint_factory_.Reset();
    constraint_factory_.basic().aspectRatio.setMin(kMinAspectRatio);
    constraint_factory_.basic().aspectRatio.setMax(kMaxAspectRatio);
    constraint_factory_.basic().height.setMin(3000);
    constraint_factory_.basic().width.setMin(3000);
    constraint_factory_.basic().aspectRatio.setIdeal(3.0);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMaxAspectRatio.
    EXPECT_EQ(constraint_factory_.basic().height.min(), result.Height());
    EXPECT_EQ(
        std::round(constraint_factory_.basic().height.min() * kMaxAspectRatio),
        result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().aspectRatio.setIdeal(0.3);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMinAspectRatio.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.min() / kMinAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.min(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained area, closest to a single point.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.setMin(100);
    constraint_factory_.basic().height.setMax(500);
    constraint_factory_.basic().width.setMin(100);
    constraint_factory_.basic().width.setMax(500);
    constraint_factory_.basic().aspectRatio.setMin(1.0);
    constraint_factory_.basic().aspectRatio.setIdeal(10.0);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to the min height and max width.
    EXPECT_EQ(constraint_factory_.basic().height.min(), result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(500.0 / 100.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = 45.0;
  constraint_factory_.basic().frameRate.setExact(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kFrameRate, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = 45.0;
  constraint_factory_.basic().frameRate.setMin(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kFrameRate is greater that the default, so expect kFrameRate.
  EXPECT_EQ(kFrameRate, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);

  const double kSmallFrameRate = 5.0;
  constraint_factory_.basic().frameRate.setMin(kSmallFrameRate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kFrameRate is greater that the default, so expect kFrameRate.
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = 45.0;
  constraint_factory_.basic().frameRate.setMax(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kFrameRate is greater that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);

  const double kSmallFrameRate = 5.0;
  constraint_factory_.basic().frameRate.setMax(kSmallFrameRate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kFrameRate is less that the default, so expect kFrameRate.
  EXPECT_EQ(kSmallFrameRate, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryRangeFrameRate) {
  constraint_factory_.Reset();
  {
    const double kMinFrameRate = 15.0;
    const double kMaxFrameRate = 45.0;
    constraint_factory_.basic().frameRate.setMax(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The range includes the default, so expect the default.
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  {
    const double kMinFrameRate = 45.0;
    const double kMaxFrameRate = 55.0;
    constraint_factory_.basic().frameRate.setMax(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The whole range is greater that the default, so expect the minimum.
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  {
    const double kMinFrameRate = 10.0;
    const double kMaxFrameRate = 15.0;
    constraint_factory_.basic().frameRate.setMax(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The whole range is less that the default, so expect the maximum.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealFrameRate) {
  // Unconstrained.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    const double kMaxFrameRate = 30.0;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    const double kMinFrameRate = 50.0;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    constraint_factory_.basic().frameRate.setMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMinFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal within range.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    const double kMinFrameRate = 35.0;
    const double kMaxFrameRate = 50.0;
    constraint_factory_.basic().frameRate.setIdeal(kIdealFrameRate);
    constraint_factory_.basic().frameRate.setMin(kMinFrameRate);
    constraint_factory_.basic().frameRate.setMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealFrameRate, result.FrameRate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

// The "Advanced" tests check selection criteria involving advanced constraint
// sets.
TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedMinMaxResolutionFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMin(2000000000);
  advanced1.height.setMin(2000000000);
  // The first advanced set cannot be satisfied and is therefore ignored in all
  // calls to SelectSettings().
  // In this case, default settings must be selected.
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);

  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.height.setMax(400);
  advanced2.width.setMax(500);
  advanced2.aspectRatio.setExact(5.0 / 4.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frameRate.setMax(10.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The third advanced set is supported in addition to the previous set.
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  blink::WebMediaTrackConstraintSet& advanced4 =
      constraint_factory_.AddAdvanced();
  advanced4.width.setExact(1000);
  advanced4.height.setExact(1000);
  result = SelectSettings();
  // The fourth advanced set cannot be supported in combination with the
  // previous two sets, so it must be ignored.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().width.setIdeal(100);
  constraint_factory_.basic().height.setIdeal(100);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The closest point to (100, 100) that satisfies all previous constraint
  // sets is its projection on the aspect-ratio line 5.0/4.0.
  // This is a point m*(4, 5) such that Dot((4,5), (100 - m(4,5))) == 0.
  // This works out to be m = 900/41.
  EXPECT_EQ(std::round(4.0 * 900.0 / 41.0), result.Height());
  EXPECT_EQ(std::round(5.0 * 900.0 / 41.0), result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().width.setIdeal(2000);
  constraint_factory_.basic().height.setIdeal(1500);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The projection of (2000,1500) on the aspect-ratio line 5.0/4.0 is beyond
  // the maximum of (400, 500), so use the maximum allowed resolution.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedExactResolution) {
  {
    constraint_factory_.Reset();
    blink::WebMediaTrackConstraintSet& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.width.setExact(40000000);
    advanced1.height.setExact(40000000);
    blink::WebMediaTrackConstraintSet& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.setExact(300000000);
    advanced2.height.setExact(300000000);
    auto result = SelectSettings();
    // None of the constraint sets can be satisfied. Default resolution should
    // be selected.
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);

    blink::WebMediaTrackConstraintSet& advanced3 =
        constraint_factory_.AddAdvanced();
    advanced3.width.setExact(1920);
    advanced3.height.setExact(1080);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    blink::WebMediaTrackConstraintSet& advanced4 =
        constraint_factory_.AddAdvanced();
    advanced4.width.setExact(640);
    advanced4.height.setExact(480);
    result = SelectSettings();
    // The fourth constraint set contradicts the third set. The fourth set
    // should be ignored.
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().width.setIdeal(800);
    constraint_factory_.basic().height.setIdeal(600);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The exact constraints has priority over ideal.
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedResolutionAndFrameRate) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setExact(1920);
  advanced1.height.setExact(1080);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frameRate.setExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(1920, result.Width());
  EXPECT_EQ(1080, result.Height());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_EQ(1920.0 / 1080.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(1920.0 / 1080.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedNoiseReduction) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMin(640);
  advanced1.height.setMin(480);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  const int kMinWidth = 4000;
  const int kMinHeight = 2000;
  advanced2.width.setMin(kMinWidth);
  advanced2.height.setMin(kMinHeight);
  advanced2.googNoiseReduction.setExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kMinWidth, result.Width());
  // Preserves default aspect ratio.
  EXPECT_EQ(static_cast<int>(
                std::round(result.Width() / kDefaultScreenCastAspectRatio)),
            result.Height());
  EXPECT_TRUE(result.noise_reduction() && !*result.noise_reduction());
  EXPECT_EQ(kMinWidth / static_cast<double>(kMaxScreenCastDimension),
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

// The "AdvancedContradictory" tests check that advanced constraint sets that
// contradict previous constraint sets are ignored.
TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryNoiseReduction) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setExact(640);
  advanced1.height.setExact(480);
  advanced1.googNoiseReduction.setExact(true);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setExact(1920);
  advanced2.height.setExact(1080);
  advanced2.googNoiseReduction.setExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_TRUE(result.noise_reduction() && *result.noise_reduction());
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
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
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
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
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  // Resolution cannot exceed the requested resolution.
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(kMinScreenCastDimension / 480.0,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(640.0 / kMinScreenCastDimension,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryMinMaxResolutionFrameRate) {
  const int kMinHeight = 2600;
  const int kMinWidth = 2800;
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMin(kMinWidth);
  advanced1.height.setMin(kMinHeight);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMax(640);
  advanced2.height.setMax(480);
  advanced2.frameRate.setExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(std::round(kMinHeight * kDefaultScreenCastAspectRatio),
            result.Width());
  EXPECT_EQ(kMinHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryExactAspectRatio) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.aspectRatio.setExact(10.0);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.aspectRatio.setExact(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * 10.0), result.Width());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(10.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(10.0, result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryAspectRatioRange) {
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.aspectRatio.setMin(10.0);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.aspectRatio.setMax(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * 10.0), result.Width());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(10.0, result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(
      kMaxScreenCastDimension / static_cast<double>(kMinScreenCastDimension),
      result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
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
  EXPECT_EQ(40.0, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
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
  EXPECT_LE(40.0, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryWidthFrameRate) {
  const int kMaxWidth = 1920;
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.setMax(kMaxWidth);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.setMin(2000);
  advanced2.frameRate.setExact(10.0);
  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frameRate.setExact(90.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kMaxWidth, result.Width());
  EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
            result.Height());
  EXPECT_EQ(90.0, result.FrameRate());
  EXPECT_EQ(
      static_cast<double>(kMinScreenCastDimension) / kMaxScreenCastDimension,
      result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
            result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryHeightFrameRate) {
  const int kMaxHeight = 2000;
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.height.setMax(kMaxHeight);
  blink::WebMediaTrackConstraintSet& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.height.setMin(4500);
  advanced2.frameRate.setExact(10.0);
  blink::WebMediaTrackConstraintSet& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frameRate.setExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kMaxHeight * kDefaultScreenCastAspectRatio, result.Width());
  // Height defaults to explicitly given max constraint.
  EXPECT_EQ(kMaxHeight, result.Height());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_EQ(static_cast<double>(kMinScreenCastDimension) / kMaxHeight,
            result.track_adapter_settings().min_aspect_ratio);
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio);
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedDeviceID) {
  const std::string kDeviceID1 = "fake_device_1";
  const std::string kDeviceID2 = "fake_device_2";
  const std::string kDeviceID3 = "fake_device_3";
  const std::string kDeviceID4 = "fake_device_4";
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
  EXPECT_EQ(kDeviceID2, result.device_id());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryDeviceID) {
  const std::string kDeviceID1 = "fake_device_1";
  const std::string kDeviceID2 = "fake_device_2";
  const std::string kDeviceID3 = "fake_device_3";
  const std::string kDeviceID4 = "fake_device_4";
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
  EXPECT_EQ(std::string(kDeviceID1), result.device_id());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedIdealDeviceID) {
  const std::string kDeviceID1 = "fake_device_1";
  const std::string kDeviceID2 = "fake_device_2";
  const std::string kDeviceID3 = "fake_device_3";
  constraint_factory_.Reset();
  blink::WebMediaTrackConstraintSet& advanced =
      constraint_factory_.AddAdvanced();
  blink::WebString id_vector1[] = {blink::WebString::fromASCII(kDeviceID1),
                                   blink::WebString::fromASCII(kDeviceID2)};
  advanced.deviceId.setExact(
      blink::WebVector<blink::WebString>(id_vector1, arraysize(id_vector1)));

  blink::WebString id_vector2[] = {blink::WebString::fromASCII(kDeviceID2),
                                   blink::WebString::fromASCII(kDeviceID3)};
  constraint_factory_.basic().deviceId.setIdeal(
      blink::WebVector<blink::WebString>(id_vector2, arraysize(id_vector2)));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Should select kDeviceID2, which appears in ideal and satisfies the advanced
  // set.
  EXPECT_EQ(std::string(kDeviceID2), result.device_id());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, ResolutionChangePolicy) {
  {
    constraint_factory_.Reset();
    auto result = SelectSettings();
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    // Resolution can be adjusted.
    EXPECT_EQ(media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setIdeal(630);
    constraint_factory_.basic().height.setIdeal(470);
    auto result = SelectSettings();
    EXPECT_EQ(630, result.Width());
    EXPECT_EQ(470, result.Height());
    // Resolution can be adjusted because ideal was used to select the
    // resolution.
    EXPECT_EQ(media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setExact(640);
    constraint_factory_.basic().height.setExact(480);
    auto result = SelectSettings();
    EXPECT_EQ(640, result.Width());
    EXPECT_EQ(480, result.Height());
    EXPECT_EQ(media::RESOLUTION_POLICY_FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setExact(1000);
    constraint_factory_.basic().height.setExact(500);
    auto result = SelectSettings();
    EXPECT_EQ(1000, result.Width());
    EXPECT_EQ(500, result.Height());
    EXPECT_EQ(media::RESOLUTION_POLICY_FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(1000.0 / 500.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1000.0 / 500.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setExact(630);
    constraint_factory_.basic().height.setExact(470);
    auto result = SelectSettings();
    EXPECT_EQ(630, result.Width());
    EXPECT_EQ(470, result.Height());
    EXPECT_EQ(media::RESOLUTION_POLICY_FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(630.0 / 470.0, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(630.0 / 470.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.setIdeal(630);
    constraint_factory_.basic().width.setMin(629);
    constraint_factory_.basic().width.setMax(631);
    constraint_factory_.basic().height.setIdeal(470);
    constraint_factory_.basic().height.setMin(469);
    auto result = SelectSettings();
    EXPECT_EQ(630, result.Width());
    EXPECT_EQ(470, result.Height());
    // Min/Max ranges prevent the resolution from being adjusted.
    EXPECT_EQ(media::RESOLUTION_POLICY_FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(629.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(631.0 / 469.0, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().aspectRatio.setExact(1.32);
    constraint_factory_.basic().height.setIdeal(480);
    auto result = SelectSettings();
    EXPECT_EQ(std::round(480 * 1.32), result.Width());
    EXPECT_EQ(480, result.Height());
    // Exact aspect ratio prevents the resolution from being adjusted.
    EXPECT_EQ(media::RESOLUTION_POLICY_FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(1.32, result.track_adapter_settings().min_aspect_ratio);
    EXPECT_EQ(1.32, result.track_adapter_settings().max_aspect_ratio);
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

}  // namespace content
