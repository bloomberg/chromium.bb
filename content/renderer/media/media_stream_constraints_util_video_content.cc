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

const int kDefaultScreenCastWidth = 2880;
const int kDefaultScreenCastHeight = 1800;
const double kDefaultScreenCastAspectRatio =
    static_cast<double>(kDefaultScreenCastWidth) / kDefaultScreenCastHeight;
const double kDefaultScreenCastFrameRate =
    MediaStreamVideoSource::kDefaultFrameRate;
const int kMinScreenCastDimension = 1;
const int kMaxScreenCastDimension = media::limits::kMaxDimension - 1;

namespace {

using Point = ResolutionSet::Point;
using StringSet = DiscreteSet<std::string>;
using BoolSet = DiscreteSet<bool>;

// Hard upper and lower bound frame rates for tab/desktop capture.
const double kMaxScreenCastFrameRate = 120.0;
const double kMinScreenCastFrameRate = 1.0 / 60.0;

constexpr double kMinScreenCastAspectRatio =
    static_cast<double>(kMinScreenCastDimension) /
    static_cast<double>(kMaxScreenCastDimension);
constexpr double kMaxScreenCastAspectRatio =
    static_cast<double>(kMaxScreenCastDimension) /
    static_cast<double>(kMinScreenCastDimension);

StringSet StringSetFromConstraint(const blink::StringConstraint& constraint) {
  if (!constraint.HasExact())
    return StringSet::UniversalSet();

  std::vector<std::string> elements;
  for (const auto& entry : constraint.Exact())
    elements.push_back(entry.Ascii());

  return StringSet(std::move(elements));
}

BoolSet BoolSetFromConstraint(const blink::BooleanConstraint& constraint) {
  if (!constraint.HasExact())
    return BoolSet::UniversalSet();

  return BoolSet({constraint.Exact()});
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
            DoubleRangeSet::FromConstraint(constraint_set.frame_rate)),
        device_id_set(StringSetFromConstraint(constraint_set.device_id)),
        noise_reduction_set(
            BoolSetFromConstraint(constraint_set.goog_noise_reduction)) {}

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
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    double default_frame_rate) {
  double frame_rate = basic_constraint_set.frame_rate.HasIdeal()
                          ? basic_constraint_set.frame_rate.Ideal()
                          : default_frame_rate;
  if (frame_rate > candidate_set.Max())
    frame_rate = candidate_set.Max();
  else if (frame_rate < candidate_set.Min())
    frame_rate = candidate_set.Min();

  return frame_rate;
}

media::VideoCaptureParams SelectVideoCaptureParamsFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    int default_height,
    int default_width,
    double default_frame_rate) {
  double requested_frame_rate = SelectFrameRateFromCandidates(
      candidates.frame_rate_set, basic_constraint_set, default_frame_rate);
  Point requested_resolution =
      candidates.resolution_set.SelectClosestPointToIdeal(
          basic_constraint_set, default_height, default_width);
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
  if (basic_constraint_set.device_id.HasIdeal()) {
    // If there are multiple elements specified by ideal, break ties by choosing
    // the first one that satisfies the constraints.
    for (const auto& ideal_entry : basic_constraint_set.device_id.Ideal()) {
      std::string ideal_value = ideal_entry.Ascii();
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

base::Optional<bool> SelectNoiseReductionFromCandidates(
    const BoolSet& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  DCHECK(!candidates.IsEmpty());
  if (basic_constraint_set.goog_noise_reduction.HasIdeal() &&
      candidates.Contains(basic_constraint_set.goog_noise_reduction.Ideal())) {
    return base::Optional<bool>(
        basic_constraint_set.goog_noise_reduction.Ideal());
  }

  if (candidates.is_universal())
    return base::Optional<bool>();

  // A non-universal BoolSet can have at most one element.
  return base::Optional<bool>(candidates.FirstElement());
}

VideoCaptureSettings SelectResultFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  std::string device_id = SelectDeviceIDFromCandidates(candidates.device_id_set,
                                                       basic_constraint_set);
  // If a maximum width or height is explicitly given, use them as default.
  // If only one of them is given, use the default aspect ratio to determine the
  // other default value.
  // TODO(guidou): Use native screen-capture resolution as default.
  // http://crbug.com/257097
  int default_height = kDefaultScreenCastHeight;
  int default_width = kDefaultScreenCastWidth;
  bool has_explicit_max_height =
      candidates.resolution_set.max_height() < kMaxScreenCastDimension;
  bool has_explicit_max_width =
      candidates.resolution_set.max_width() < kMaxScreenCastDimension;
  if (has_explicit_max_height && has_explicit_max_width) {
    default_height = candidates.resolution_set.max_height();
    default_width = candidates.resolution_set.max_width();
  } else if (has_explicit_max_height) {
    default_height = candidates.resolution_set.max_height();
    default_width = static_cast<int>(
        std::round(default_height * kDefaultScreenCastAspectRatio));
  } else if (has_explicit_max_width) {
    default_width = candidates.resolution_set.max_width();
    default_height = static_cast<int>(
        std::round(default_width / kDefaultScreenCastAspectRatio));
  }
  media::VideoCaptureParams capture_params =
      SelectVideoCaptureParamsFromCandidates(candidates, basic_constraint_set,
                                             default_height, default_width,
                                             kDefaultScreenCastFrameRate);

  base::Optional<bool> noise_reduction = SelectNoiseReductionFromCandidates(
      candidates.noise_reduction_set, basic_constraint_set);

  auto track_adapter_settings = SelectVideoTrackAdapterSettings(
      basic_constraint_set, candidates.resolution_set,
      candidates.frame_rate_set, capture_params.requested_format);

  return VideoCaptureSettings(std::move(device_id), capture_params,
                              noise_reduction, track_adapter_settings,
                              candidates.frame_rate_set.Min());
}

VideoCaptureSettings UnsatisfiedConstraintsResult(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& constraint_set) {
  DCHECK(candidates.IsEmpty());
  if (candidates.resolution_set.IsHeightEmpty()) {
    return VideoCaptureSettings(constraint_set.height.GetName());
  } else if (candidates.resolution_set.IsWidthEmpty()) {
    return VideoCaptureSettings(constraint_set.width.GetName());
  } else if (candidates.resolution_set.IsAspectRatioEmpty()) {
    return VideoCaptureSettings(constraint_set.aspect_ratio.GetName());
  } else if (candidates.frame_rate_set.IsEmpty()) {
    return VideoCaptureSettings(constraint_set.frame_rate.GetName());
  } else if (candidates.noise_reduction_set.IsEmpty()) {
    return VideoCaptureSettings(constraint_set.goog_noise_reduction.GetName());
  } else {
    DCHECK(candidates.device_id_set.IsEmpty());
    return VideoCaptureSettings(constraint_set.device_id.GetName());
  }
}

}  // namespace

VideoCaptureSettings SelectSettingsVideoContentCapture(
    const blink::WebMediaConstraints& constraints) {
  VideoContentCaptureCandidates candidates;
  candidates.resolution_set = ScreenCastResolutionCapabilities();
  candidates.frame_rate_set =
      DoubleRangeSet(kMinScreenCastFrameRate, kMaxScreenCastFrameRate);
  // candidates.device_id_set and candidates.noise_reduction_set are
  // automatically initialized with the universal set.

  candidates = candidates.Intersection(
      VideoContentCaptureCandidates(constraints.Basic()));
  if (candidates.IsEmpty())
    return UnsatisfiedConstraintsResult(candidates, constraints.Basic());

  for (const auto& advanced_set : constraints.Advanced()) {
    VideoContentCaptureCandidates advanced_candidates(advanced_set);
    VideoContentCaptureCandidates intersection =
        candidates.Intersection(advanced_candidates);
    if (!intersection.IsEmpty())
      candidates = std::move(intersection);
  }

  DCHECK(!candidates.IsEmpty());
  return SelectResultFromCandidates(candidates, constraints.Basic());
}

}  // namespace content
