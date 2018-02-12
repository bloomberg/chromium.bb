// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_constraints_util_video_content.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "content/common/media/media_stream_controls.h"
#include "content/renderer/media/stream/media_stream_constraints_util_sets.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "media/base/limits.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

const int kMinScreenCastDimension = 1;
// Use kMaxDimension/2 as maximum to ensure selected resolutions have area less
// than media::limits::kMaxCanvas.
const int kMaxScreenCastDimension = media::limits::kMaxDimension / 2;
static_assert(kMaxScreenCastDimension * kMaxScreenCastDimension <
                  media::limits::kMaxCanvas,
              "Invalid kMaxScreenCastDimension");

const int kDefaultScreenCastWidth = 2880;
const int kDefaultScreenCastHeight = 1800;
const double kDefaultScreenCastAspectRatio =
    static_cast<double>(kDefaultScreenCastWidth) / kDefaultScreenCastHeight;
static_assert(kDefaultScreenCastWidth <= kMaxScreenCastDimension,
              "Invalid kDefaultScreenCastWidth");
static_assert(kDefaultScreenCastHeight <= kMaxScreenCastDimension,
              "Invalid kDefaultScreenCastHeight");

const double kMaxScreenCastFrameRate = 120.0;
const double kDefaultScreenCastFrameRate =
    MediaStreamVideoSource::kDefaultFrameRate;

namespace {

using Point = ResolutionSet::Point;
using StringSet = DiscreteSet<std::string>;
using BoolSet = DiscreteSet<bool>;

constexpr double kMinScreenCastAspectRatio =
    static_cast<double>(kMinScreenCastDimension) /
    static_cast<double>(kMaxScreenCastDimension);
constexpr double kMaxScreenCastAspectRatio =
    static_cast<double>(kMaxScreenCastDimension) /
    static_cast<double>(kMinScreenCastDimension);

using DoubleRangeSet = NumericRangeSet<double>;

class VideoContentCaptureCandidates {
 public:
  VideoContentCaptureCandidates()
      : has_explicit_max_height_(false), has_explicit_max_width_(false) {}
  explicit VideoContentCaptureCandidates(
      const blink::WebMediaTrackConstraintSet& constraint_set)
      : resolution_set_(ResolutionSet::FromConstraintSet(constraint_set)),
        has_explicit_max_height_(ConstraintHasMax(constraint_set.height) &&
                                 ConstraintMax(constraint_set.height) <=
                                     kMaxScreenCastDimension),
        has_explicit_max_width_(ConstraintHasMax(constraint_set.width) &&
                                ConstraintMax(constraint_set.width) <=
                                    kMaxScreenCastDimension),
        frame_rate_set_(
            DoubleRangeSet::FromConstraint(constraint_set.frame_rate,
                                           0.0,
                                           kMaxScreenCastFrameRate)),
        device_id_set_(StringSetFromConstraint(constraint_set.device_id)),
        noise_reduction_set_(
            BoolSetFromConstraint(constraint_set.goog_noise_reduction)) {}

  VideoContentCaptureCandidates(VideoContentCaptureCandidates&& other) =
      default;
  VideoContentCaptureCandidates& operator=(
      VideoContentCaptureCandidates&& other) = default;

  bool IsEmpty() const {
    return resolution_set_.IsEmpty() || frame_rate_set_.IsEmpty() ||
           device_id_set_.IsEmpty() || noise_reduction_set_.IsEmpty();
  }

  VideoContentCaptureCandidates Intersection(
      const VideoContentCaptureCandidates& other) {
    VideoContentCaptureCandidates intersection;
    intersection.resolution_set_ =
        resolution_set_.Intersection(other.resolution_set_);
    intersection.has_explicit_max_height_ =
        has_explicit_max_height_ || other.has_explicit_max_height_;
    intersection.has_explicit_max_width_ =
        has_explicit_max_width_ || other.has_explicit_max_width_;
    intersection.frame_rate_set_ =
        frame_rate_set_.Intersection(other.frame_rate_set_);
    intersection.device_id_set_ =
        device_id_set_.Intersection(other.device_id_set_);
    intersection.noise_reduction_set_ =
        noise_reduction_set_.Intersection(other.noise_reduction_set_);
    return intersection;
  }

  const ResolutionSet& resolution_set() const { return resolution_set_; }
  bool has_explicit_max_height() const { return has_explicit_max_height_; }
  bool has_explicit_max_width() const { return has_explicit_max_width_; }
  const DoubleRangeSet& frame_rate_set() const { return frame_rate_set_; }
  const StringSet& device_id_set() const { return device_id_set_; }
  const BoolSet& noise_reduction_set() const { return noise_reduction_set_; }
  void set_resolution_set(const ResolutionSet& set) { resolution_set_ = set; }
  void set_frame_rate_set(const DoubleRangeSet& set) { frame_rate_set_ = set; }

 private:
  ResolutionSet resolution_set_;
  bool has_explicit_max_height_;
  bool has_explicit_max_width_;
  DoubleRangeSet frame_rate_set_;
  StringSet device_id_set_;
  BoolSet noise_reduction_set_;
};

ResolutionSet ScreenCastResolutionCapabilities() {
  return ResolutionSet(kMinScreenCastDimension, kMaxScreenCastDimension,
                       kMinScreenCastDimension, kMaxScreenCastDimension,
                       kMinScreenCastAspectRatio, kMaxScreenCastAspectRatio);
}

// This algorithm for selecting policy matches the old non-spec compliant
// algorithm in order to be more compatible with existing applications.
// TODO(guidou): Update this algorithm to properly take into account the minimum
// width and height, and the aspect_ratio constraint once most existing
// applications migrate to the new syntax. See http://crbug.com/701302.
media::ResolutionChangePolicy SelectResolutionPolicyFromCandidates(
    const ResolutionSet& resolution_set,
    media::ResolutionChangePolicy default_policy) {
  if (resolution_set.max_height() < kMaxScreenCastDimension &&
      resolution_set.max_width() < kMaxScreenCastDimension &&
      resolution_set.min_height() > kMinScreenCastDimension &&
      resolution_set.min_width() > kMinScreenCastDimension) {
    if (resolution_set.min_height() == resolution_set.max_height() &&
        resolution_set.min_width() == resolution_set.max_width()) {
      return media::ResolutionChangePolicy::FIXED_RESOLUTION;
    }

    int approx_aspect_ratio_min_resolution =
        100 * resolution_set.min_width() / resolution_set.min_height();
    int approx_aspect_ratio_max_resolution =
        100 * resolution_set.max_width() / resolution_set.max_height();
    if (approx_aspect_ratio_min_resolution ==
        approx_aspect_ratio_max_resolution) {
      return media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    }

    return media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }

  return default_policy;
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
  if (candidate_set.Max() && frame_rate > *candidate_set.Max())
    frame_rate = *candidate_set.Max();
  else if (candidate_set.Min() && frame_rate < *candidate_set.Min())
    frame_rate = *candidate_set.Min();

  return frame_rate;
}

media::VideoCaptureParams SelectVideoCaptureParamsFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    int default_height,
    int default_width,
    double default_frame_rate,
    media::ResolutionChangePolicy default_resolution_policy) {
  double requested_frame_rate = SelectFrameRateFromCandidates(
      candidates.frame_rate_set(), basic_constraint_set, default_frame_rate);
  Point requested_resolution =
      candidates.resolution_set().SelectClosestPointToIdeal(
          basic_constraint_set, default_height, default_width);
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      ToGfxSize(requested_resolution), static_cast<float>(requested_frame_rate),
      media::PIXEL_FORMAT_I420);
  params.resolution_change_policy = SelectResolutionPolicyFromCandidates(
      candidates.resolution_set(), default_resolution_policy);
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

int ClampToValidScreenCastDimension(int value) {
  if (value > kMaxScreenCastDimension)
    return kMaxScreenCastDimension;
  else if (value < kMinScreenCastDimension)
    return kMinScreenCastDimension;
  return value;
}

VideoCaptureSettings SelectResultFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const std::string& stream_source) {
  std::string device_id = SelectDeviceIDFromCandidates(
      candidates.device_id_set(), basic_constraint_set);
  // If a maximum width or height is explicitly given, use them as default.
  // If only one of them is given, use the default aspect ratio to determine the
  // other default value.
  // TODO(guidou): Use native screen-capture resolution as default.
  // http://crbug.com/257097
  int default_height = kDefaultScreenCastHeight;
  int default_width = kDefaultScreenCastWidth;
  if (candidates.has_explicit_max_height() &&
      candidates.has_explicit_max_width()) {
    default_height = candidates.resolution_set().max_height();
    default_width = candidates.resolution_set().max_width();
  } else if (candidates.has_explicit_max_height()) {
    default_height = candidates.resolution_set().max_height();
    default_width = static_cast<int>(
        std::round(default_height * kDefaultScreenCastAspectRatio));
  } else if (candidates.has_explicit_max_width()) {
    default_width = candidates.resolution_set().max_width();
    default_height = static_cast<int>(
        std::round(default_width / kDefaultScreenCastAspectRatio));
  }
  // When the given maximum values are large, the computed values using default
  // aspect ratio may fall out of range. Ensure the defaults are in the valid
  // range.
  default_height = ClampToValidScreenCastDimension(default_height);
  default_width = ClampToValidScreenCastDimension(default_width);

  // If a maximum frame rate is explicitly given, use it as default for
  // better compatibility with the old constraints algorithm.
  // TODO(guidou): Use the actual default when applications migrate to the new
  // constraint syntax.  http://crbug.com/710800
  double default_frame_rate =
      candidates.frame_rate_set().Max().value_or(kDefaultScreenCastFrameRate);

  // This default comes from the old algorithm.
  media::ResolutionChangePolicy default_resolution_policy =
      stream_source == kMediaStreamSourceTab
          ? media::ResolutionChangePolicy::FIXED_RESOLUTION
          : media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;

  media::VideoCaptureParams capture_params =
      SelectVideoCaptureParamsFromCandidates(
          candidates, basic_constraint_set, default_height, default_width,
          default_frame_rate, default_resolution_policy);

  base::Optional<bool> noise_reduction = SelectNoiseReductionFromCandidates(
      candidates.noise_reduction_set(), basic_constraint_set);

  auto track_adapter_settings = SelectVideoTrackAdapterSettings(
      basic_constraint_set, candidates.resolution_set(),
      candidates.frame_rate_set(), capture_params.requested_format);

  return VideoCaptureSettings(std::move(device_id), capture_params,
                              noise_reduction, track_adapter_settings,
                              candidates.frame_rate_set().Min(),
                              candidates.frame_rate_set().Max());
}

VideoCaptureSettings UnsatisfiedConstraintsResult(
    const VideoContentCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& constraint_set) {
  DCHECK(candidates.IsEmpty());
  if (candidates.resolution_set().IsHeightEmpty()) {
    return VideoCaptureSettings(constraint_set.height.GetName());
  } else if (candidates.resolution_set().IsWidthEmpty()) {
    return VideoCaptureSettings(constraint_set.width.GetName());
  } else if (candidates.resolution_set().IsAspectRatioEmpty()) {
    return VideoCaptureSettings(constraint_set.aspect_ratio.GetName());
  } else if (candidates.frame_rate_set().IsEmpty()) {
    return VideoCaptureSettings(constraint_set.frame_rate.GetName());
  } else if (candidates.noise_reduction_set().IsEmpty()) {
    return VideoCaptureSettings(constraint_set.goog_noise_reduction.GetName());
  } else {
    DCHECK(candidates.device_id_set().IsEmpty());
    return VideoCaptureSettings(constraint_set.device_id.GetName());
  }
}

}  // namespace

VideoCaptureSettings SelectSettingsVideoContentCapture(
    const blink::WebMediaConstraints& constraints,
    const std::string& stream_source) {
  VideoContentCaptureCandidates candidates;
  candidates.set_resolution_set(ScreenCastResolutionCapabilities());

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
  return SelectResultFromCandidates(candidates, constraints.Basic(),
                                    stream_source);
}

}  // namespace content
