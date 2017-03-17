// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util_video_content.h"

#include <cmath>
#include <utility>
#include <vector>

#include "content/renderer/media/media_stream_constraints_util_sets.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/limits.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

using Point = ResolutionSet::Point;
using StringSet = DiscreteSet<std::string>;
using BoolSet = DiscreteSet<bool>;

// Hard upper and lower bound frame rates for tab/desktop capture.
constexpr double kMaxScreenCastFrameRate = 120.0;
constexpr double kMinScreenCastFrameRate = 1.0 / 60.0;

constexpr double kDefaultFrameRate = MediaStreamVideoSource::kDefaultFrameRate;

constexpr int kMinScreenCastDimension = 1;
constexpr int kMaxScreenCastDimension = media::limits::kMaxDimension - 1;
constexpr double kMinScreenCastAspectRatio =
    static_cast<double>(kMinScreenCastDimension) /
    static_cast<double>(kMaxScreenCastDimension);
constexpr double kMaxScreenCastAspectRatio =
    static_cast<double>(kMaxScreenCastDimension) /
    static_cast<double>(kMinScreenCastDimension);

StringSet StringSetFromConstraint(const blink::StringConstraint& constraint) {
  if (!constraint.hasExact())
    return StringSet::UniversalSet();

  std::vector<std::string> elements;
  for (const auto& entry : constraint.exact())
    elements.push_back(entry.ascii());

  return StringSet(std::move(elements));
}

BoolSet BoolSetFromConstraint(const blink::BooleanConstraint& constraint) {
  if (!constraint.hasExact())
    return BoolSet::UniversalSet();

  return BoolSet({constraint.exact()});
}

using DoubleRangeSet = NumericRangeSet<double>;

class VideoContentCaptureCandidates {
 public:
  VideoContentCaptureCandidates()
      : device_id_set(StringSet::UniversalSet()),
        noise_reduction_set(BoolSet::UniversalSet()) {}
  explicit VideoContentCaptureCandidates(
      const blink::WebMediaTrackConstraintSet& constraint_set)
      : resolution_set(ResolutionSet::FromConstraintSet(constraint_set)),
        frame_rate_set(
            DoubleRangeSet::FromConstraint(constraint_set.frameRate)),
        device_id_set(StringSetFromConstraint(constraint_set.deviceId)),
        noise_reduction_set(
            BoolSetFromConstraint(constraint_set.googNoiseReduction)) {}

  VideoContentCaptureCandidates(VideoContentCaptureCandidates&& other) =
      default;
  VideoContentCaptureCandidates& operator=(
      VideoContentCaptureCandidates&& other) = default;

  bool IsEmpty() const {
    return resolution_set.IsEmpty() || frame_rate_set.IsEmpty() ||
           device_id_set.IsEmpty() || noise_reduction_set.IsEmpty();
  }

  VideoContentCaptureCandidates Intersection(
      const VideoContentCaptureCandidates& other) {
    VideoContentCaptureCandidates intersection;
    intersection.resolution_set =
        resolution_set.Intersection(other.resolution_set);
    intersection.frame_rate_set =
        frame_rate_set.Intersection(other.frame_rate_set);
    intersection.device_id_set =
        device_id_set.Intersection(other.device_id_set);
    intersection.noise_reduction_set =
        noise_reduction_set.Intersection(other.noise_reduction_set);
    return intersection;
  }

  ResolutionSet resolution_set;
  DoubleRangeSet frame_rate_set;
  StringSet device_id_set;
  BoolSet noise_reduction_set;
};

ResolutionSet ScreenCastResolutionCapabilities() {
  return ResolutionSet(kMinScreenCastDimension, kMaxScreenCastDimension,
                       kMinScreenCastDimension, kMaxScreenCastDimension,
                       kMinScreenCastAspectRatio, kMaxScreenCastAspectRatio);
}

// TODO(guidou): Update this policy to better match the way
// WebContentsCaptureMachine::ComputeOptimalViewSize() interprets
// resolution-change policies. See http://crbug.com/701302.
media::ResolutionChangePolicy SelectResolutionPolicyFromCandidates(
    const ResolutionSet& resolution_set) {
  ResolutionSet capabilities = ScreenCastResolutionCapabilities();
  bool can_adjust_resolution =
      resolution_set.min_height() <= capabilities.min_height() &&
      resolution_set.max_height() >= capabilities.max_height() &&
      resolution_set.min_width() <= capabilities.min_width() &&
      resolution_set.max_width() >= capabilities.max_width() &&
      resolution_set.min_aspect_ratio() <= capabilities.min_aspect_ratio() &&
      resolution_set.max_aspect_ratio() >= capabilities.max_aspect_ratio();

  return can_adjust_resolution ? media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT
                               : media::RESOLUTION_POLICY_FIXED_RESOLUTION;
}

int RoundToInt(double d) {
  return static_cast<int>(std::round(d));
}

gfx::Size ToGfxSize(const Point& point) {
  return gfx::Size(RoundToInt(point.width()), RoundToInt(point.height()));
}

double SelectFrameRateFromCandidates(
    const DoubleRangeSet& candidate_set,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  double frame_rate = basic_constraint_set.frameRate.hasIdeal()
                          ? basic_constraint_set.frameRate.ideal()
                          : kDefaultFrameRate;
  if (frame_rate > candidate_set.Max())
    frame_rate = candidate_set.Max();
  else if (frame_rate < candidate_set.Min())
    frame_rate = candidate_set.Min();

  return frame_rate;
}

media::VideoCaptureParams SelectVideoCaptureParamsFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  double requested_frame_rate = SelectFrameRateFromCandidates(
      candidates.frame_rate_set, basic_constraint_set);
  Point requested_resolution =
      candidates.resolution_set.SelectClosestPointToIdeal(basic_constraint_set);
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      ToGfxSize(requested_resolution), static_cast<float>(requested_frame_rate),
      media::PIXEL_FORMAT_I420);
  params.resolution_change_policy =
      SelectResolutionPolicyFromCandidates(candidates.resolution_set);
  // Content capture always uses default power-line frequency.
  DCHECK(params.IsValid());

  return params;
}

std::string SelectDeviceIDFromCandidates(
    const StringSet& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  DCHECK(!candidates.IsEmpty());
  if (basic_constraint_set.deviceId.hasIdeal()) {
    // If there are multiple elements specified by ideal, break ties by choosing
    // the first one that satisfies the constraints.
    for (const auto& ideal_entry : basic_constraint_set.deviceId.ideal()) {
      std::string ideal_value = ideal_entry.ascii();
      if (candidates.Contains(ideal_value)) {
        return ideal_value;
      }
    }
  }

  // Return the empty string if nothing is specified in the constraints.
  // The empty string is treated as a default device ID by the browser.
  if (candidates.is_universal()) {
    return std::string();
  }

  // If there are multiple elements that satisfy the constraints, break ties by
  // using the element that was specified first.
  return candidates.FirstElement();
}

rtc::Optional<bool> SelectNoiseReductionFromCandidates(
    const BoolSet& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  DCHECK(!candidates.IsEmpty());
  if (basic_constraint_set.googNoiseReduction.hasIdeal() &&
      candidates.Contains(basic_constraint_set.googNoiseReduction.ideal())) {
    return rtc::Optional<bool>(basic_constraint_set.googNoiseReduction.ideal());
  }

  if (candidates.is_universal())
    return rtc::Optional<bool>();

  // A non-universal BoolSet can have at most one element.
  return rtc::Optional<bool>(candidates.FirstElement());
}

VideoContentCaptureSourceSelectionResult SelectResultFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  std::string device_id = SelectDeviceIDFromCandidates(candidates.device_id_set,
                                                       basic_constraint_set);
  media::VideoCaptureParams capture_params =
      SelectVideoCaptureParamsFromCandidates(candidates, basic_constraint_set);

  rtc::Optional<bool> noise_reduction = SelectNoiseReductionFromCandidates(
      candidates.noise_reduction_set, basic_constraint_set);

  return VideoContentCaptureSourceSelectionResult(
      std::move(device_id), noise_reduction, capture_params);
}

VideoContentCaptureSourceSelectionResult UnsatisfiedConstraintsResult(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& constraint_set) {
  DCHECK(candidates.IsEmpty());
  if (candidates.resolution_set.IsHeightEmpty()) {
    return VideoContentCaptureSourceSelectionResult(
        constraint_set.height.name());
  } else if (candidates.resolution_set.IsWidthEmpty()) {
    return VideoContentCaptureSourceSelectionResult(
        constraint_set.width.name());
  } else if (candidates.resolution_set.IsAspectRatioEmpty()) {
    return VideoContentCaptureSourceSelectionResult(
        constraint_set.aspectRatio.name());
  } else if (candidates.frame_rate_set.IsEmpty()) {
    return VideoContentCaptureSourceSelectionResult(
        constraint_set.frameRate.name());
  } else if (candidates.noise_reduction_set.IsEmpty()) {
    return VideoContentCaptureSourceSelectionResult(
        constraint_set.googNoiseReduction.name());
  } else {
    DCHECK(candidates.device_id_set.IsEmpty());
    return VideoContentCaptureSourceSelectionResult(
        constraint_set.deviceId.name());
  }
}

}  // namespace

VideoContentCaptureSourceSelectionResult::
    VideoContentCaptureSourceSelectionResult()
    : VideoContentCaptureSourceSelectionResult("") {}

VideoContentCaptureSourceSelectionResult::
    VideoContentCaptureSourceSelectionResult(const char* failed_constraint_name)
    : failed_constraint_name_(failed_constraint_name) {}

VideoContentCaptureSourceSelectionResult::
    VideoContentCaptureSourceSelectionResult(
        std::string device_id,
        const rtc::Optional<bool>& noise_reduction,
        media::VideoCaptureParams capture_params)
    : failed_constraint_name_(nullptr),
      device_id_(std::move(device_id)),
      noise_reduction_(noise_reduction),
      capture_params_(capture_params) {}

VideoContentCaptureSourceSelectionResult::
    VideoContentCaptureSourceSelectionResult(
        const VideoContentCaptureSourceSelectionResult& other) = default;
VideoContentCaptureSourceSelectionResult::
    VideoContentCaptureSourceSelectionResult(
        VideoContentCaptureSourceSelectionResult&& other) = default;
VideoContentCaptureSourceSelectionResult::
    ~VideoContentCaptureSourceSelectionResult() = default;
VideoContentCaptureSourceSelectionResult&
VideoContentCaptureSourceSelectionResult::operator=(
    const VideoContentCaptureSourceSelectionResult& other) = default;
VideoContentCaptureSourceSelectionResult&
VideoContentCaptureSourceSelectionResult::operator=(
    VideoContentCaptureSourceSelectionResult&& other) = default;

int VideoContentCaptureSourceSelectionResult::Height() const {
  DCHECK(HasValue());
  return capture_params_.requested_format.frame_size.height();
}

int VideoContentCaptureSourceSelectionResult::Width() const {
  DCHECK(HasValue());
  return capture_params_.requested_format.frame_size.width();
}

float VideoContentCaptureSourceSelectionResult::FrameRate() const {
  DCHECK(HasValue());
  return capture_params_.requested_format.frame_rate;
}

media::ResolutionChangePolicy
VideoContentCaptureSourceSelectionResult::ResolutionChangePolicy() const {
  DCHECK(HasValue());
  return capture_params_.resolution_change_policy;
}

VideoContentCaptureSourceSelectionResult
SelectVideoContentCaptureSourceSettings(
    const blink::WebMediaConstraints& constraints) {
  VideoContentCaptureCandidates candidates;
  candidates.resolution_set = ScreenCastResolutionCapabilities();
  candidates.frame_rate_set =
      DoubleRangeSet(kMinScreenCastFrameRate, kMaxScreenCastFrameRate);
  // candidates.device_id_set and candidates.noise_reduction_set are
  // automatically initialized with the universal set.

  candidates = candidates.Intersection(
      VideoContentCaptureCandidates(constraints.basic()));
  if (candidates.IsEmpty())
    return UnsatisfiedConstraintsResult(candidates, constraints.basic());

  for (const auto& advanced_set : constraints.advanced()) {
    VideoContentCaptureCandidates advanced_candidates(advanced_set);
    VideoContentCaptureCandidates intersection =
        candidates.Intersection(advanced_candidates);
    if (!intersection.IsEmpty())
      candidates = std::move(intersection);
  }

  DCHECK(!candidates.IsEmpty());
  return SelectResultFromCandidates(candidates, constraints.basic());
}

}  // namespace content
