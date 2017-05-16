// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_constraints_util_sets.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kSourceHeight = 1000;
const int kSourceWidth = 1500;
constexpr double kSourceAspectRatio =
    static_cast<double>(kSourceWidth) / static_cast<double>(kSourceHeight);
constexpr double kSourceFrameRate = 100.0;

media::VideoCaptureFormat SourceFormat() {
  return media::VideoCaptureFormat(gfx::Size(kSourceWidth, kSourceHeight),
                                   kSourceFrameRate, media::PIXEL_FORMAT_I420);
}

}  // namespace

namespace content {

class MediaStreamConstraintsUtilTest : public testing::Test {
 protected:
  using DoubleRangeSet = NumericRangeSet<double>;
};

TEST_F(MediaStreamConstraintsUtilTest, BooleanConstraints) {
  static const std::string kValueTrue = "true";
  static const std::string kValueFalse = "false";

  MockConstraintFactory constraint_factory;
  // Mandatory constraints.
  constraint_factory.basic().echo_cancellation.SetExact(true);
  constraint_factory.basic().goog_echo_cancellation.SetExact(false);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();
  bool value_true = false;
  bool value_false = false;
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::echo_cancellation,
      &value_true));
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::goog_echo_cancellation,
      &value_false));
  EXPECT_TRUE(value_true);
  EXPECT_FALSE(value_false);

  // Optional constraints, represented as "advanced"
  constraint_factory.Reset();
  constraint_factory.AddAdvanced().echo_cancellation.SetExact(false);
  constraint_factory.AddAdvanced().goog_echo_cancellation.SetExact(true);
  constraints = constraint_factory.CreateWebMediaConstraints();
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::echo_cancellation,
      &value_false));
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::goog_echo_cancellation,
      &value_true));
  EXPECT_TRUE(value_true);
  EXPECT_FALSE(value_false);

  // A mandatory constraint should override an optional one.
  constraint_factory.Reset();
  constraint_factory.AddAdvanced().echo_cancellation.SetExact(false);
  constraint_factory.basic().echo_cancellation.SetExact(true);
  constraints = constraint_factory.CreateWebMediaConstraints();
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::echo_cancellation,
      &value_true));
  EXPECT_TRUE(value_true);
}

TEST_F(MediaStreamConstraintsUtilTest, DoubleConstraints) {
  MockConstraintFactory constraint_factory;
  const double test_value = 0.01f;

  constraint_factory.basic().aspect_ratio.SetExact(test_value);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();

  double value;
  EXPECT_FALSE(GetConstraintValueAsDouble(
      constraints, &blink::WebMediaTrackConstraintSet::frame_rate, &value));
  EXPECT_TRUE(GetConstraintValueAsDouble(
      constraints, &blink::WebMediaTrackConstraintSet::aspect_ratio, &value));
  EXPECT_EQ(test_value, value);
}

TEST_F(MediaStreamConstraintsUtilTest, IntConstraints) {
  MockConstraintFactory constraint_factory;
  const int test_value = 327;

  constraint_factory.basic().width.SetExact(test_value);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();

  int value;
  EXPECT_TRUE(GetConstraintValueAsInteger(
      constraints, &blink::WebMediaTrackConstraintSet::width, &value));
  EXPECT_EQ(test_value, value);

  // An exact value should also be reflected as min and max.
  EXPECT_TRUE(GetConstraintMaxAsInteger(
      constraints, &blink::WebMediaTrackConstraintSet::width, &value));
  EXPECT_EQ(test_value, value);
  EXPECT_TRUE(GetConstraintMinAsInteger(
      constraints, &blink::WebMediaTrackConstraintSet::width, &value));
  EXPECT_EQ(test_value, value);
}

TEST_F(MediaStreamConstraintsUtilTest, VideoTrackAdapterSettingsUnconstrained) {
  ResolutionSet resolution_set;
  DoubleRangeSet frame_rate_set;

  // No ideal values.
  {
    MockConstraintFactory constraint_factory;
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(0.0, result.max_frame_rate);
  }

  // Ideal height.
  {
    const int kIdealHeight = 400;
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kIdealHeight, result.max_height);
    EXPECT_EQ(std::round(kIdealHeight * kSourceAspectRatio), result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(0.0, result.max_frame_rate);
  }

  // Ideal width.
  {
    const int kIdealWidth = 400;
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(std::round(kIdealWidth / kSourceAspectRatio), result.max_height);
    EXPECT_EQ(kIdealWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(0.0, result.max_frame_rate);
  }

  // Ideal aspect ratio.
  {
    const double kIdealAspectRatio = 2.0;
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(std::round(kSourceHeight * kIdealAspectRatio), result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(0.0, result.max_frame_rate);
  }

  // Ideal frame rate.
  {
    const double kIdealFrameRate = 33;
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(kIdealFrameRate, result.max_frame_rate);
  }

  // All ideals supplied.
  {
    const int kIdealHeight = 400;
    const int kIdealWidth = 600;
    const int kIdealAspectRatio = 2.0;
    const double kIdealFrameRate = 33;
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    // Ideal aspect ratio is ignored if ideal width and height are supplied.
    constraint_factory.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kIdealHeight, result.max_height);
    EXPECT_EQ(kIdealWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(kIdealFrameRate, result.max_frame_rate);
  }
}

TEST_F(MediaStreamConstraintsUtilTest, VideoTrackAdapterSettingsConstrained) {
  const int kMinHeight = 500;
  const int kMaxHeight = 1200;
  const int kMinWidth = 1000;
  const int kMaxWidth = 2000;
  constexpr double kMinAspectRatio = 1.0;
  constexpr double kMaxAspectRatio = 2.0;
  constexpr double kMinFrameRate = 20.0;
  constexpr double kMaxFrameRate = 44.0;
  ResolutionSet resolution_set(kMinHeight, kMaxHeight, kMinWidth, kMaxWidth,
                               kMinAspectRatio, kMaxAspectRatio);
  DoubleRangeSet frame_rate_set(kMinFrameRate, kMaxFrameRate);

  // No ideal values.
  {
    MockConstraintFactory constraint_factory;
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal height < min.
  {
    const int kIdealHeight = 400;
    static_assert(kIdealHeight < kMinHeight,
                  "kIdealHeight must be less than kMinHeight");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kMinHeight, result.max_height);
    // kMinWidth > kMinHeight * kNativeAspectRatio
    EXPECT_EQ(kMinWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // min < Ideal height < max.
  {
    const int kIdealHeight = 1100;
    static_assert(kIdealHeight > kMinHeight,
                  "kIdealHeight must be greater than kMinHeight");
    static_assert(kIdealHeight < kMaxHeight,
                  "kIdealHeight must be less than kMaxHeight");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kIdealHeight, result.max_height);
    EXPECT_EQ(std::round(kIdealHeight * kSourceAspectRatio), result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal height > max.
  {
    const int kIdealHeight = 2000;
    static_assert(kIdealHeight > kMaxHeight,
                  "kIdealHeight must be greater than kMaxHeight");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kMaxHeight, result.max_height);
    EXPECT_EQ(std::round(kMaxHeight * kSourceAspectRatio), result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal width < min.
  {
    const int kIdealWidth = 800;
    static_assert(kIdealWidth < kMinWidth,
                  "kIdealWidth must be less than kMinWidth");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(std::round(kMinWidth / kSourceAspectRatio), result.max_height);
    EXPECT_EQ(kMinWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // min < Ideal width < max.
  {
    const int kIdealWidth = 1800;
    static_assert(kIdealWidth > kMinWidth,
                  "kIdealWidth must be greater than kMinWidth");
    static_assert(kIdealWidth < kMaxWidth,
                  "kIdealWidth must be less than kMaxWidth");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(std::round(kIdealWidth / kSourceAspectRatio), result.max_height);
    EXPECT_EQ(kIdealWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal width > max.
  {
    const int kIdealWidth = 3000;
    static_assert(kIdealWidth > kMaxWidth,
                  "kIdealWidth must be greater than kMaxWidth");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    // kMaxHeight < kMaxWidth / kNativeAspectRatio
    EXPECT_EQ(kMaxHeight, result.max_height);
    EXPECT_EQ(kMaxWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal aspect ratio < min.
  {
    constexpr double kIdealAspectRatio = 0.5;
    static_assert(kIdealAspectRatio < kMinAspectRatio,
                  "kIdealAspectRatio must be less than kMinAspectRatio");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    // Desired point is (kNativeWidth/kMinAspectRatio, kNativeWidth), but it
    // is outside the size constraints. Closest to that while maintaining the
    // same aspect ratio is (kMaxHeight, kMaxHeight * kMinAspectRatio).
    EXPECT_EQ(kMaxHeight, result.max_height);
    EXPECT_EQ(std::round(kMaxHeight * kMinAspectRatio), result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // min < Ideal aspect ratio < max.
  {
    constexpr double kIdealAspectRatio = 1.25;
    static_assert(kIdealAspectRatio > kMinAspectRatio,
                  "kIdealAspectRatio must be greater than kMinAspectRatio");
    static_assert(kIdealAspectRatio < kMaxAspectRatio,
                  "kIdealAspectRatio must be less than kMaxAspectRatio");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(std::round(kSourceWidth / kIdealAspectRatio), result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal aspect ratio > max.
  {
    constexpr double kIdealAspectRatio = 3.0;
    static_assert(kIdealAspectRatio > kMaxAspectRatio,
                  "kIdealAspectRatio must be greater than kMaxAspectRatio");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(std::round(kSourceHeight * kMaxAspectRatio), result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal frame rate < min.
  {
    constexpr double kIdealFrameRate = 3.0;
    static_assert(kIdealFrameRate < kMinFrameRate,
                  "kIdealFrameRate must be less than kMinFrameRate");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMinFrameRate, result.max_frame_rate);
  }

  // min < Ideal frame rate < max.
  {
    constexpr double kIdealFrameRate = 31.0;
    static_assert(kIdealFrameRate > kMinFrameRate,
                  "kIdealFrameRate must be greater than kMinFrameRate");
    static_assert(kIdealFrameRate < kMaxFrameRate,
                  "kIdealFrameRate must be less than kMaxFrameRate");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kIdealFrameRate, result.max_frame_rate);
  }

  // Ideal frame rate > max.
  {
    constexpr double kIdealFrameRate = 1000.0;
    static_assert(kIdealFrameRate > kMaxFrameRate,
                  "kIdealFrameRate must be greater than kMaxFrameRate");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Ideal values inside constraints.
  {
    const int kIdealHeight = 900;
    const int kIdealWidth = 1600;
    constexpr double kIdealFrameRate = 35.0;
    static_assert(kIdealHeight > kMinHeight,
                  "kMinHeight must be greater than kMinHeight");
    static_assert(kIdealHeight < kMaxHeight,
                  "kMinHeight must be less than kMaxHeight");
    static_assert(kIdealWidth > kMinWidth,
                  "kIdealWidth must be greater than kMinWidth");
    static_assert(kIdealWidth < kMaxWidth,
                  "kIdealWidth must be less than kMaxHeight");
    static_assert(kIdealFrameRate > kMinFrameRate,
                  "kIdealFrameRate must be greater than kMinFrameRate");
    static_assert(kIdealFrameRate < kMaxFrameRate,
                  "kIdealFrameRate must be less than kMaxFrameRate");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kIdealHeight, result.max_height);
    EXPECT_EQ(kIdealWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kIdealFrameRate, result.max_frame_rate);
  }

  // Ideal values outside constraints.
  {
    const int kIdealHeight = 2900;
    const int kIdealWidth = 3600;
    constexpr double kIdealFrameRate = 350.0;
    static_assert(kIdealHeight > kMaxHeight,
                  "kMinHeight must be greater than kMaxHeight");
    static_assert(kIdealWidth > kMaxWidth,
                  "kIdealWidth must be greater than kMaxHeight");
    static_assert(kIdealFrameRate > kMaxFrameRate,
                  "kIdealFrameRate must be greater than kMaxFrameRate");
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kMaxHeight, result.max_height);
    EXPECT_EQ(kMaxWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate);
  }

  // Source frame rate.
  {
    DoubleRangeSet frame_rate_set(kMinFrameRate, kSourceFrameRate);
    MockConstraintFactory constraint_factory;
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    // No frame-rate adjustment because the track will use the same frame rate
    // as the source.
    EXPECT_EQ(0.0, result.max_frame_rate);
  }

  // High frame rate.
  {
    constexpr double kHighFrameRate = 400.0;  // Greater than source.
    DoubleRangeSet frame_rate_set(kMinFrameRate, kHighFrameRate);
    static_assert(kHighFrameRate > kSourceFrameRate,
                  "kIdealFrameRate must be greater than kSourceFrameRate");
    MockConstraintFactory constraint_factory;
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(kMinAspectRatio, result.min_aspect_ratio);
    EXPECT_EQ(kMaxAspectRatio, result.max_aspect_ratio);
    // No frame-rate adjustment because the track will use a frame rate that is
    // greater than the source's.
    EXPECT_EQ(0.0, result.max_frame_rate);
  }
}

TEST_F(MediaStreamConstraintsUtilTest,
       VideoTrackAdapterSettingsExpectedNativeSize) {
  ResolutionSet resolution_set;
  DoubleRangeSet frame_rate_set;

  {
    MockConstraintFactory constraint_factory;
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), false);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(0.0, result.max_frame_rate);
    EXPECT_FALSE(result.expected_native_size);
  }

  {
    MockConstraintFactory constraint_factory;
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), true);
    EXPECT_EQ(kSourceHeight, result.max_height);
    EXPECT_EQ(kSourceWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(0.0, result.max_frame_rate);
    EXPECT_TRUE(result.expected_native_size);
    EXPECT_EQ(gfx::Size(kSourceWidth, kSourceHeight),
              *result.expected_native_size);
  }

  // Ideals supplied.
  {
    const int kIdealHeight = 400;
    const int kIdealWidth = 600;
    const int kIdealAspectRatio = 2.0;
    const double kIdealFrameRate = 33;
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().height.SetIdeal(kIdealHeight);
    constraint_factory.basic().width.SetIdeal(kIdealWidth);
    // Ideal aspect ratio is ignored if ideal width and height are supplied.
    constraint_factory.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectVideoTrackAdapterSettings(
        constraint_factory.CreateWebMediaConstraints().Basic(), resolution_set,
        frame_rate_set, SourceFormat(), true);
    EXPECT_EQ(kIdealHeight, result.max_height);
    EXPECT_EQ(kIdealWidth, result.max_width);
    EXPECT_EQ(0.0, result.min_aspect_ratio);
    EXPECT_EQ(HUGE_VAL, result.max_aspect_ratio);
    EXPECT_EQ(kIdealFrameRate, result.max_frame_rate);
    EXPECT_TRUE(result.expected_native_size);
    EXPECT_EQ(gfx::Size(kSourceWidth, kSourceHeight),
              *result.expected_native_size);
  }
}

}  // namespace content
