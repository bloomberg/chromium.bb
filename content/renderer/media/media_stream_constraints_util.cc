// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_constraints_util_sets.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

template <typename P, typename T>
bool ScanConstraintsForExactValue(const blink::WebMediaConstraints& constraints,
                                  P picker,
                                  T* value) {
  if (constraints.IsNull())
    return false;

  const auto& the_field = constraints.Basic().*picker;
  if (the_field.HasExact()) {
    *value = the_field.Exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.Advanced()) {
    const auto& the_field = advanced_constraint.*picker;
    if (the_field.HasExact()) {
      *value = the_field.Exact();
      return true;
    }
  }
  return false;
}

template <typename P, typename T>
bool ScanConstraintsForMaxValue(const blink::WebMediaConstraints& constraints,
                                P picker,
                                T* value) {
  if (constraints.IsNull())
    return false;

  const auto& the_field = constraints.Basic().*picker;
  if (the_field.HasMax()) {
    *value = the_field.Max();
    return true;
  }
  if (the_field.HasExact()) {
    *value = the_field.Exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.Advanced()) {
    const auto& the_field = advanced_constraint.*picker;
    if (the_field.HasMax()) {
      *value = the_field.Max();
      return true;
    }
    if (the_field.HasExact()) {
      *value = the_field.Exact();
      return true;
    }
  }
  return false;
}

template <typename P, typename T>
bool ScanConstraintsForMinValue(const blink::WebMediaConstraints& constraints,
                                P picker,
                                T* value) {
  if (constraints.IsNull())
    return false;

  const auto& the_field = constraints.Basic().*picker;
  if (the_field.HasMin()) {
    *value = the_field.Min();
    return true;
  }
  if (the_field.HasExact()) {
    *value = the_field.Exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.Advanced()) {
    const auto& the_field = advanced_constraint.*picker;
    if (the_field.HasMin()) {
      *value = the_field.Min();
      return true;
    }
    if (the_field.HasExact()) {
      *value = the_field.Exact();
      return true;
    }
  }
  return false;
}

}  // namespace

VideoCaptureSettings::VideoCaptureSettings() : VideoCaptureSettings("") {}

VideoCaptureSettings::VideoCaptureSettings(const char* failed_constraint_name)
    : failed_constraint_name_(failed_constraint_name) {}

VideoCaptureSettings::VideoCaptureSettings(
    std::string device_id,
    media::VideoCaptureParams capture_params,
    base::Optional<bool> noise_reduction,
    const VideoTrackAdapterSettings& track_adapter_settings,
    double min_frame_rate)
    : failed_constraint_name_(nullptr),
      device_id_(std::move(device_id)),
      capture_params_(capture_params),
      noise_reduction_(noise_reduction),
      track_adapter_settings_(track_adapter_settings),
      min_frame_rate_(min_frame_rate) {
  DCHECK_LE(min_frame_rate_, capture_params.requested_format.frame_rate);
  DCHECK_LE(track_adapter_settings.max_width,
            capture_params.requested_format.frame_size.width());
  DCHECK_LE(track_adapter_settings.max_height,
            capture_params.requested_format.frame_size.height());
  DCHECK_LT(track_adapter_settings.max_frame_rate,
            capture_params.requested_format.frame_rate);
}

VideoCaptureSettings::VideoCaptureSettings(const VideoCaptureSettings& other) =
    default;
VideoCaptureSettings::VideoCaptureSettings(VideoCaptureSettings&& other) =
    default;
VideoCaptureSettings::~VideoCaptureSettings() = default;
VideoCaptureSettings& VideoCaptureSettings::operator=(
    const VideoCaptureSettings& other) = default;
VideoCaptureSettings& VideoCaptureSettings::operator=(
    VideoCaptureSettings&& other) = default;

bool GetConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*picker,
    bool* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintMinAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value) {
  return ScanConstraintsForMinValue(constraints, picker, value);
}

bool GetConstraintMaxAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value) {
  return ScanConstraintsForMaxValue(constraints, picker, value);
}

bool GetConstraintValueAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintMinAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value) {
  return ScanConstraintsForMinValue(constraints, picker, value);
}

bool GetConstraintMaxAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value) {
  return ScanConstraintsForMaxValue(constraints, picker, value);
}

bool GetConstraintValueAsString(
    const blink::WebMediaConstraints& constraints,
    const blink::StringConstraint blink::WebMediaTrackConstraintSet::*picker,
    std::string* value) {
  blink::WebVector<blink::WebString> return_value;
  if (ScanConstraintsForExactValue(constraints, picker, &return_value)) {
    *value = return_value[0].Utf8();
    return true;
  }
  return false;
}

rtc::Optional<bool> ConstraintToOptional(
    const blink::WebMediaConstraints& constraints,
    const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*picker) {
  bool value;
  if (GetConstraintValueAsBoolean(constraints, picker, &value)) {
    return rtc::Optional<bool>(value);
  }
  return rtc::Optional<bool>();
}

VideoTrackAdapterSettings SelectVideoTrackAdapterSettings(
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const ResolutionSet& resolution_set,
    const NumericRangeSet<double>& frame_rate_set,
    const media::VideoCaptureFormat& source_format,
    bool expect_source_native_size) {
  ResolutionSet::Point resolution = resolution_set.SelectClosestPointToIdeal(
      basic_constraint_set, source_format.frame_size.height(),
      source_format.frame_size.width());
  int track_max_height = static_cast<int>(std::round(resolution.height()));
  int track_max_width = static_cast<int>(std::round(resolution.width()));
  double track_min_aspect_ratio =
      std::max(resolution_set.min_aspect_ratio(),
               static_cast<double>(resolution_set.min_width()) /
                   static_cast<double>(resolution_set.max_height()));
  double track_max_aspect_ratio =
      std::min(resolution_set.max_aspect_ratio(),
               static_cast<double>(resolution_set.max_width()) /
                   static_cast<double>(resolution_set.min_height()));
  double track_max_frame_rate = frame_rate_set.Max();
  if (basic_constraint_set.frame_rate.HasIdeal()) {
    track_max_frame_rate = std::min(
        track_max_frame_rate, std::max(basic_constraint_set.frame_rate.Ideal(),
                                       frame_rate_set.Min()));
  }
  // VideoTrackAdapter uses a frame rate of 0.0 to disable frame-rate
  // adjustment.
  if (track_max_frame_rate >= source_format.frame_rate)
    track_max_frame_rate = 0.0;

  base::Optional<gfx::Size> expected_native_size;
  if (expect_source_native_size)
    expected_native_size = source_format.frame_size;

  return VideoTrackAdapterSettings(
      track_max_width, track_max_height, track_min_aspect_ratio,
      track_max_aspect_ratio, track_max_frame_rate, expected_native_size);
}

}  // namespace content
