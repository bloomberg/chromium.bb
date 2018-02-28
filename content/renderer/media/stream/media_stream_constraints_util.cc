// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_constraints_util.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/stream/media_stream_constraints_util_sets.h"
#include "content/renderer/media/stream/media_stream_constraints_util_video_device.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

// TODO(c.padhi): Allow frame rates lower than 1Hz,
// see https://crbug.com/814131.
const float kMinDeviceCaptureFrameRate = 1.0f;

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
    const auto& advanced_field = advanced_constraint.*picker;
    if (advanced_field.HasExact()) {
      *value = advanced_field.Exact();
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
    const auto& advanced_field = advanced_constraint.*picker;
    if (advanced_field.HasMax()) {
      *value = advanced_field.Max();
      return true;
    }
    if (advanced_field.HasExact()) {
      *value = advanced_field.Exact();
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
    const auto& advanced_field = advanced_constraint.*picker;
    if (advanced_field.HasMin()) {
      *value = advanced_field.Min();
      return true;
    }
    if (advanced_field.HasExact()) {
      *value = advanced_field.Exact();
      return true;
    }
  }
  return false;
}

}  // namespace

VideoCaptureSettings::VideoCaptureSettings() : VideoCaptureSettings("") {}

VideoCaptureSettings::VideoCaptureSettings(const char* failed_constraint_name)
    : failed_constraint_name_(failed_constraint_name) {
  DCHECK(failed_constraint_name_);
}

VideoCaptureSettings::VideoCaptureSettings(
    std::string device_id,
    media::VideoCaptureParams capture_params,
    base::Optional<bool> noise_reduction,
    const VideoTrackAdapterSettings& track_adapter_settings,
    base::Optional<double> min_frame_rate,
    base::Optional<double> max_frame_rate)
    : failed_constraint_name_(nullptr),
      device_id_(std::move(device_id)),
      capture_params_(capture_params),
      noise_reduction_(noise_reduction),
      track_adapter_settings_(track_adapter_settings),
      min_frame_rate_(min_frame_rate),
      max_frame_rate_(max_frame_rate) {
  DCHECK(!min_frame_rate ||
         *min_frame_rate_ <= capture_params.requested_format.frame_rate);
  DCHECK_LE(track_adapter_settings.max_width,
            capture_params.requested_format.frame_size.width());
  DCHECK_LE(track_adapter_settings.max_height,
            capture_params.requested_format.frame_size.height());
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

AudioCaptureSettings::AudioCaptureSettings() : AudioCaptureSettings("") {}

AudioCaptureSettings::AudioCaptureSettings(const char* failed_constraint_name)
    : failed_constraint_name_(failed_constraint_name) {
  DCHECK(failed_constraint_name_);
}

AudioCaptureSettings::AudioCaptureSettings(
    std::string device_id,
    const media::AudioParameters& audio_parameters,
    bool enable_hotword,
    bool disable_local_echo,
    bool enable_automatic_output_device_selection,
    const AudioProcessingProperties& audio_processing_properties)
    : failed_constraint_name_(nullptr),
      device_id_(std::move(device_id)),
      audio_parameters_(audio_parameters),
      hotword_enabled_(enable_hotword),
      disable_local_echo_(disable_local_echo),
      render_to_associated_sink_(enable_automatic_output_device_selection),
      audio_processing_properties_(audio_processing_properties) {}

AudioCaptureSettings::AudioCaptureSettings(const AudioCaptureSettings& other) =
    default;
AudioCaptureSettings& AudioCaptureSettings::operator=(
    const AudioCaptureSettings& other) = default;
AudioCaptureSettings::AudioCaptureSettings(AudioCaptureSettings&& other) =
    default;
AudioCaptureSettings& AudioCaptureSettings::operator=(
    AudioCaptureSettings&& other) = default;

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

std::string GetMediaStreamSource(
    const blink::WebMediaConstraints& constraints) {
  std::string source;
  if (constraints.Basic().media_stream_source.HasIdeal() &&
      constraints.Basic().media_stream_source.Ideal().size() > 0) {
    source = constraints.Basic().media_stream_source.Ideal()[0].Utf8();
  }
  if (constraints.Basic().media_stream_source.HasExact() &&
      constraints.Basic().media_stream_source.Exact().size() > 0) {
    source = constraints.Basic().media_stream_source.Exact()[0].Utf8();
  }

  return source;
}

bool IsDeviceCapture(const blink::WebMediaConstraints& constraints) {
  return GetMediaStreamSource(constraints).empty();
}

VideoTrackAdapterSettings SelectVideoTrackAdapterSettings(
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const ResolutionSet& resolution_set,
    const NumericRangeSet<double>& frame_rate_set,
    const media::VideoCaptureFormat& source_format) {
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
  // VideoTrackAdapter uses a frame rate of 0.0 to disable frame-rate
  // adjustment.
  double track_max_frame_rate = frame_rate_set.Max().value_or(0.0);
  if (basic_constraint_set.frame_rate.HasIdeal()) {
    track_max_frame_rate = basic_constraint_set.frame_rate.Ideal();
    if (frame_rate_set.Min() && track_max_frame_rate < *frame_rate_set.Min())
      track_max_frame_rate = *frame_rate_set.Min();
    if (frame_rate_set.Max() && track_max_frame_rate > *frame_rate_set.Max())
      track_max_frame_rate = *frame_rate_set.Max();
  }
  // Disable frame-rate adjustment if the requested rate is greater than the
  // source rate.
  if (track_max_frame_rate >= source_format.frame_rate)
    track_max_frame_rate = 0.0;

  return VideoTrackAdapterSettings(
      track_max_width, track_max_height, track_min_aspect_ratio,
      track_max_aspect_ratio, track_max_frame_rate);
}

double NumericConstraintFitnessDistance(double value1, double value2) {
  if (std::fabs(value1 - value2) <= blink::DoubleConstraint::kConstraintEpsilon)
    return 0.0;

  return std::fabs(value1 - value2) /
         std::max(std::fabs(value1), std::fabs(value2));
}

double StringConstraintFitnessDistance(
    const blink::WebString& value,
    const blink::StringConstraint& constraint) {
  if (!constraint.HasIdeal())
    return 0.0;

  for (auto& ideal_value : constraint.Ideal()) {
    if (value == ideal_value)
      return 0.0;
  }

  return 1.0;
}

blink::WebMediaStreamSource::Capabilities ComputeCapabilitiesForVideoSource(
    const blink::WebString& device_id,
    const media::VideoCaptureFormats& formats,
    media::VideoFacingMode facing_mode,
    bool is_device_capture) {
  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = device_id;
  if (is_device_capture)
    capabilities.facing_mode = ToWebFacingMode(facing_mode);
  if (!formats.empty()) {
    int max_width = 1;
    int max_height = 1;
    float min_frame_rate =
        is_device_capture ? kMinDeviceCaptureFrameRate : 0.0f;
    float max_frame_rate = min_frame_rate;
    for (const auto& format : formats) {
      max_width = std::max(max_width, format.frame_size.width());
      max_height = std::max(max_height, format.frame_size.height());
      max_frame_rate = std::max(max_frame_rate, format.frame_rate);
    }
    capabilities.width = {1, max_width};
    capabilities.height = {1, max_height};
    capabilities.aspect_ratio = {1.0 / max_height,
                                 static_cast<double>(max_width)};
    capabilities.frame_rate = {min_frame_rate, max_frame_rate};
  }
  return capabilities;
}

}  // namespace content
