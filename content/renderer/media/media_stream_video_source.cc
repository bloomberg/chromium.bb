// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_process.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/media_stream_constraints_util_video_device.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/video_track_adapter.h"

namespace content {

namespace {

const char* const kLegalVideoConstraints[] = {"width",
                                              "height",
                                              "aspectRatio",
                                              "frameRate",
                                              "facingMode",
                                              "deviceId",
                                              "groupId",
                                              "mediaStreamSource",
                                              "googNoiseReduction",
                                              "videoKind"};

// Returns true if |constraint| has mandatory constraints.
bool HasMandatoryConstraints(const blink::WebMediaConstraints& constraints) {
  return constraints.Basic().HasMandatory();
}

// Retrieve the desired max width and height from |constraints|. If not set,
// the |desired_width| and |desired_height| are set to
// std::numeric_limits<int>::max();
// If either max or exact width or height is set as a mandatory constraint,
// the advanced constraints are not checked.
void GetDesiredMaxWidthAndHeight(const blink::WebMediaConstraints& constraints,
                                 int* desired_width, int* desired_height) {
  *desired_width = std::numeric_limits<int>::max();
  *desired_height = std::numeric_limits<int>::max();

  const auto& basic_constraints = constraints.Basic();

  if (basic_constraints.width.HasMax() || basic_constraints.height.HasMax() ||
      basic_constraints.width.HasExact() ||
      basic_constraints.height.HasExact()) {
    if (basic_constraints.width.HasMax())
      *desired_width = basic_constraints.width.Max();
    if (basic_constraints.height.HasMax())
      *desired_height = basic_constraints.height.Max();
    // Exact constraints override max constraints if both are specified.
    // Specifying both in the same structure is meaningless.
    if (basic_constraints.width.HasExact())
      *desired_width = basic_constraints.width.Exact();
    if (basic_constraints.height.HasExact())
      *desired_height = basic_constraints.height.Exact();
    return;
  }

  for (const auto& constraint_set : constraints.Advanced()) {
    if (constraint_set.width.HasMax())
      *desired_width = constraint_set.width.Max();
    if (constraint_set.height.HasMax())
      *desired_height = constraint_set.height.Max();
    if (constraint_set.width.HasExact())
      *desired_width = constraint_set.width.Exact();
    if (constraint_set.height.HasExact())
      *desired_height = constraint_set.height.Exact();
  }
}

// Retrieve the desired max and min aspect ratio from |constraints|. If not set,
// the |min_aspect_ratio| is set to 0 and |max_aspect_ratio| is set to
// std::numeric_limits<double>::max();
// If either min or max aspect ratio is set as a mandatory constraint, the
// optional constraints are not checked.
void GetDesiredMinAndMaxAspectRatio(
    const blink::WebMediaConstraints& constraints,
    double* min_aspect_ratio,
    double* max_aspect_ratio) {
  *min_aspect_ratio = 0;
  *max_aspect_ratio = std::numeric_limits<double>::max();

  if (constraints.Basic().aspect_ratio.HasMin() ||
      constraints.Basic().aspect_ratio.HasMax()) {
    if (constraints.Basic().aspect_ratio.HasMin())
      *min_aspect_ratio = constraints.Basic().aspect_ratio.Min();
    if (constraints.Basic().aspect_ratio.HasMax())
      *max_aspect_ratio = constraints.Basic().aspect_ratio.Max();
    return;
    // Note - the code will ignore attempts at successive refinement
    // of the aspect ratio with advanced constraint. This may be wrong.
  }
  // Note - the code below will potentially pick min and max from different
  // constraint sets, some of which might have been ignored.
  for (const auto& constraint_set : constraints.Advanced()) {
    if (constraint_set.aspect_ratio.HasMin()) {
      *min_aspect_ratio = constraint_set.aspect_ratio.Min();
      break;
    }
  }
  for (const auto& constraint_set : constraints.Advanced()) {
    // Advanced constraint sets with max aspect ratio 0 are unsatisfiable and
    // must be ignored.
    if (constraint_set.aspect_ratio.HasMax() &&
        constraint_set.aspect_ratio.Max() > 0) {
      *max_aspect_ratio = constraint_set.aspect_ratio.Max();
      break;
    }
  }
}

// Returns true if |constraints| are fulfilled. |format| can be changed by a
// constraint, e.g. the frame rate can be changed by setting maxFrameRate.
bool UpdateFormatForConstraints(
    const blink::WebMediaTrackConstraintSet& constraints,
    media::VideoCaptureFormat* format,
    std::string* failing_constraint_name) {
  DCHECK(format != NULL);

  if (!format->IsValid())
    return false;

  // The width and height are matched based on cropping occuring later:
  // min width/height has to be >= the size of the frame (no upscale).
  // max width/height just has to be > 0 (we can crop anything too large).
  if ((constraints.width.HasMin() &&
       constraints.width.Min() > format->frame_size.width()) ||
      (constraints.width.HasMax() && constraints.width.Max() <= 0) ||
      (constraints.width.HasExact() &&
       constraints.width.Exact() > format->frame_size.width())) {
    *failing_constraint_name = constraints.width.GetName();
  } else if ((constraints.height.HasMin() &&
              constraints.height.Min() > format->frame_size.height()) ||
             (constraints.height.HasMax() && constraints.height.Max() <= 0) ||
             (constraints.height.HasExact() &&
              constraints.height.Exact() > format->frame_size.height())) {
    *failing_constraint_name = constraints.height.GetName();
  } else if (constraints.video_kind.HasExact() &&
             !constraints.video_kind.Matches(GetVideoKindForFormat(*format))) {
    *failing_constraint_name = constraints.video_kind.GetName();
  } else if (!constraints.frame_rate.Matches(format->frame_rate)) {
    if (constraints.frame_rate.HasMax()) {
      const double value = constraints.frame_rate.Max();
      // TODO(hta): Check if handling of max = 0.0 is relevant.
      // (old handling was to set rate to 1.0 if 0.0 was specified)
      if (constraints.frame_rate.Matches(value)) {
        format->frame_rate =
            (format->frame_rate > value) ? value : format->frame_rate;
        return true;
      }
    }
    *failing_constraint_name = constraints.frame_rate.GetName();
  } else {
    return true;
  }

  DCHECK(!failing_constraint_name->empty());
  return false;
}

// Removes media::VideoCaptureFormats from |formats| that don't meet
// |constraints|.
void FilterFormatsByConstraints(
    const blink::WebMediaTrackConstraintSet& constraints,
    media::VideoCaptureFormats* formats,
    std::string* failing_constraint_name) {
  media::VideoCaptureFormats::iterator format_it = formats->begin();
  while (format_it != formats->end()) {
    // Modify |format_it| to fulfill the constraint if possible.
    // Delete it otherwise.
    if (!UpdateFormatForConstraints(constraints, &(*format_it),
                                    failing_constraint_name)) {
      DVLOG(2) << "Format filter: Discarding format "
               << format_it->frame_size.width() << "x"
               << format_it->frame_size.height() << "@"
               << format_it->frame_rate;
      format_it = formats->erase(format_it);
    } else {
      ++format_it;
    }
  }
}

// Returns the media::VideoCaptureFormats that matches |constraints|.
// If the return value is empty, and the reason is a specific constraint,
// |unsatisfied_constraint| returns the name of the constraint.
media::VideoCaptureFormats FilterFormats(
    const blink::WebMediaConstraints& constraints,
    const media::VideoCaptureFormats& supported_formats,
    std::string* unsatisfied_constraint) {
  if (constraints.IsNull())
    return supported_formats;

  const auto& basic = constraints.Basic();

  // Do some checks that won't be done when filtering candidates.

  if (basic.width.HasMin() && basic.width.HasMax() &&
      basic.width.Min() > basic.width.Max()) {
    *unsatisfied_constraint = basic.width.GetName();
    return media::VideoCaptureFormats();
  }

  if (basic.height.HasMin() && basic.height.HasMax() &&
      basic.height.Min() > basic.height.Max()) {
    *unsatisfied_constraint = basic.height.GetName();
    return media::VideoCaptureFormats();
  }

  double max_aspect_ratio;
  double min_aspect_ratio;
  GetDesiredMinAndMaxAspectRatio(constraints,
                                 &min_aspect_ratio,
                                 &max_aspect_ratio);

  if (min_aspect_ratio > max_aspect_ratio || max_aspect_ratio < 0.05f) {
    DLOG(WARNING) << "Wrong requested aspect ratio: min " << min_aspect_ratio
                  << " max " << max_aspect_ratio;
    *unsatisfied_constraint = basic.aspect_ratio.GetName();
    return media::VideoCaptureFormats();
  }

  std::vector<std::string> temp(
      &kLegalVideoConstraints[0],
      &kLegalVideoConstraints[sizeof(kLegalVideoConstraints) /
                              sizeof(kLegalVideoConstraints[0])]);
  std::string failing_name;
  if (basic.HasMandatoryOutsideSet(temp, failing_name)) {
    *unsatisfied_constraint = failing_name;
    return media::VideoCaptureFormats();
  }

  media::VideoCaptureFormats candidates = supported_formats;
  FilterFormatsByConstraints(basic, &candidates, unsatisfied_constraint);

  if (candidates.empty())
    return candidates;

  // Ok - all mandatory checked and we still have candidates.
  // Let's try filtering using the advanced constraints. The advanced
  // constraints must be filtered in the order they occur in |advanced|.
  // But if a constraint produce zero candidates, the constraint is ignored and
  // the next constraint is tested.
  // http://w3c.github.io/mediacapture-main/getusermedia.html#dfn-selectsettings
  for (const auto& constraint_set : constraints.Advanced()) {
    media::VideoCaptureFormats current_candidates = candidates;
    std::string unsatisfied_constraint;
    FilterFormatsByConstraints(constraint_set, &current_candidates,
                               &unsatisfied_constraint);
    if (!current_candidates.empty())
      candidates = current_candidates;
  }

  // We have done as good as we can to filter the supported resolutions.
  return candidates;
}

media::VideoCaptureFormat GetBestFormatBasedOnArea(
    const media::VideoCaptureFormats& formats,
    int area) {
  DCHECK(!formats.empty());
  const media::VideoCaptureFormat* best_format = nullptr;
  int best_diff = std::numeric_limits<int>::max();
  for (const auto& format : formats) {
    const int diff = abs(area - format.frame_size.GetArea());
    if (diff < best_diff) {
      best_diff = diff;
      best_format = &format;
    }
  }
  DVLOG(3) << "GetBestFormatBasedOnArea chose format "
           << media::VideoCaptureFormat::ToString(*best_format);
  return *best_format;
}

// Find the format that best matches the default video size.
// This algorithm is chosen since a resolution must be picked even if no
// constraints are provided. We don't just select the maximum supported
// resolution since higher resolutions cost more in terms of complexity and
// many cameras have lower frame rate and have more noise in the image at
// their maximum supported resolution.
media::VideoCaptureFormat GetBestCaptureFormat(
    const media::VideoCaptureFormats& formats,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(!formats.empty());

  int max_width;
  int max_height;
  GetDesiredMaxWidthAndHeight(constraints, &max_width, &max_height);
  const int area =
      std::min(max_width,
               static_cast<int>(MediaStreamVideoSource::kDefaultWidth)) *
      std::min(max_height,
               static_cast<int>(MediaStreamVideoSource::kDefaultHeight));

  return GetBestFormatBasedOnArea(formats, area);
}

}  // anonymous namespace

bool IsOldVideoConstraints() {
  return base::FeatureList::IsEnabled(
      features::kMediaStreamOldVideoConstraints);
}

// static
MediaStreamVideoSource* MediaStreamVideoSource::GetVideoSource(
    const blink::WebMediaStreamSource& source) {
  if (source.IsNull() ||
      source.GetType() != blink::WebMediaStreamSource::kTypeVideo) {
    return nullptr;
  }
  return static_cast<MediaStreamVideoSource*>(source.GetExtraData());
}

MediaStreamVideoSource::MediaStreamVideoSource()
    : state_(NEW),
      track_adapter_(
          new VideoTrackAdapter(ChildProcess::current()->io_task_runner())),
      weak_factory_(this) {}

MediaStreamVideoSource::~MediaStreamVideoSource() {
  DCHECK(CalledOnValidThread());
}

void MediaStreamVideoSource::AddTrackLegacy(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsOldVideoConstraints());
  DCHECK(!constraints.IsNull());
  DCHECK(std::find(tracks_.begin(), tracks_.end(), track) == tracks_.end());
  tracks_.push_back(track);
  secure_tracker_.Add(track, true);

  track_descriptors_.push_back(
      TrackDescriptor(track, frame_callback, constraints, callback));

  switch (state_) {
    case NEW: {
      // Tab capture and Screen capture needs the maximum requested height
      // and width to decide on the resolution.
      // NOTE: Optional constraints are deliberately ignored.
      int max_requested_width = 0;
      if (constraints.Basic().width.HasMax())
        max_requested_width = constraints.Basic().width.Max();

      int max_requested_height = 0;
      if (constraints.Basic().height.HasMax())
        max_requested_height = constraints.Basic().height.Max();

      double max_requested_frame_rate = kDefaultFrameRate;
      if (constraints.Basic().frame_rate.HasMax())
        max_requested_frame_rate = constraints.Basic().frame_rate.Max();

      state_ = RETRIEVING_CAPABILITIES;
      GetCurrentSupportedFormats(
          max_requested_width,
          max_requested_height,
          max_requested_frame_rate,
          base::Bind(&MediaStreamVideoSource::OnSupportedFormats,
                     weak_factory_.GetWeakPtr()));

      break;
    }
    case STARTING:
    case RETRIEVING_CAPABILITIES: {
      // The |callback| will be triggered once the source has started or
      // the capabilities have been retrieved.
      break;
    }
    case ENDED:
    case STARTED: {
      // Currently, reconfiguring the source is not supported.
      FinalizeAddTrackLegacy();
    }
  }
}

void MediaStreamVideoSource::AddTrack(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& track_adapter_settings,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const ConstraintsCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(std::find(tracks_.begin(), tracks_.end(), track) == tracks_.end());
  tracks_.push_back(track);
  secure_tracker_.Add(track, true);

  track_descriptors_.push_back(TrackDescriptor(
      track, frame_callback,
      base::MakeUnique<VideoTrackAdapterSettings>(track_adapter_settings),
      callback));

  switch (state_) {
    case NEW: {
      state_ = STARTING;
      blink::WebMediaConstraints ignored_constraints;
      StartSourceImpl(
          media::VideoCaptureFormat() /* ignored */, ignored_constraints,
          base::Bind(&VideoTrackAdapter::DeliverFrameOnIO, track_adapter_));
      break;
    }
    case STARTING: {
      break;
    }
    case RETRIEVING_CAPABILITIES: {
      NOTREACHED();
      break;
    }
    case ENDED:
    case STARTED: {
      // Currently, reconfiguring the source is not supported.
      FinalizeAddTrack();
    }
  }
}

void MediaStreamVideoSource::RemoveTrack(MediaStreamVideoTrack* video_track) {
  DCHECK(CalledOnValidThread());
  std::vector<MediaStreamVideoTrack*>::iterator it =
      std::find(tracks_.begin(), tracks_.end(), video_track);
  DCHECK(it != tracks_.end());
  tracks_.erase(it);
  secure_tracker_.Remove(video_track);

  for (std::vector<TrackDescriptor>::iterator it = track_descriptors_.begin();
       it != track_descriptors_.end(); ++it) {
    if (it->track == video_track) {
      track_descriptors_.erase(it);
      break;
    }
  }

  // Call |frame_adapter_->RemoveTrack| here even if adding the track has
  // failed and |frame_adapter_->AddCallback| has not been called.
  track_adapter_->RemoveTrack(video_track);

  if (tracks_.empty())
    StopSource();
}

void MediaStreamVideoSource::UpdateHasConsumers(MediaStreamVideoTrack* track,
                                                bool has_consumers) {
  DCHECK(CalledOnValidThread());
  const auto it =
      std::find(suspended_tracks_.begin(), suspended_tracks_.end(), track);
  if (has_consumers) {
    if (it != suspended_tracks_.end())
      suspended_tracks_.erase(it);
  } else {
    if (it == suspended_tracks_.end())
      suspended_tracks_.push_back(track);
  }
  OnHasConsumers(suspended_tracks_.size() < tracks_.size());
}

void MediaStreamVideoSource::UpdateCapturingLinkSecure(
    MediaStreamVideoTrack* track, bool is_secure) {
  DCHECK(CalledOnValidThread());
  secure_tracker_.Update(track, is_secure);
  OnCapturingLinkSecured(secure_tracker_.is_capturing_secure());
}

base::SingleThreadTaskRunner* MediaStreamVideoSource::io_task_runner() const {
  DCHECK(CalledOnValidThread());
  return track_adapter_->io_task_runner();
}

base::Optional<media::VideoCaptureFormat>
MediaStreamVideoSource::GetCurrentFormat() const {
  DCHECK(CalledOnValidThread());
  if (IsOldVideoConstraints()) {
    if (state_ == STARTING || state_ == STARTED)
      return current_format_;
    return base::Optional<media::VideoCaptureFormat>();
  } else {
    return GetCurrentFormatImpl();
  }
}

base::Optional<media::VideoCaptureFormat>
MediaStreamVideoSource::GetCurrentFormatImpl() const {
  return base::Optional<media::VideoCaptureFormat>();
}

void MediaStreamVideoSource::DoStopSource() {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "DoStopSource()";
  if (state_ == ENDED)
    return;
  track_adapter_->StopFrameMonitoring();
  StopSourceImpl();
  state_ = ENDED;
  SetReadyState(blink::WebMediaStreamSource::kReadyStateEnded);
}

void MediaStreamVideoSource::OnSupportedFormats(
    const media::VideoCaptureFormats& formats) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsOldVideoConstraints());
  DCHECK_EQ(RETRIEVING_CAPABILITIES, state_);

  supported_formats_ = formats;
  blink::WebMediaConstraints fulfilled_constraints;
  if (!FindBestFormatWithConstraints(supported_formats_,
                                     &current_format_,
                                     &fulfilled_constraints)) {
    SetReadyState(blink::WebMediaStreamSource::kReadyStateEnded);
    DVLOG(3) << "OnSupportedFormats failed to find an usable format";
    // This object can be deleted after calling FinalizeAddTrack. See comment
    // in the header file.
    FinalizeAddTrackLegacy();
    return;
  }

  state_ = STARTING;
  DVLOG(3) << "Starting the capturer with "
           << media::VideoCaptureFormat::ToString(current_format_);

  StartSourceImpl(
      current_format_,
      fulfilled_constraints,
      base::Bind(&VideoTrackAdapter::DeliverFrameOnIO, track_adapter_));
}

bool MediaStreamVideoSource::FindBestFormatWithConstraints(
    const media::VideoCaptureFormats& formats,
    media::VideoCaptureFormat* best_format,
    blink::WebMediaConstraints* fulfilled_constraints) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "MediaStreamVideoSource::FindBestFormatWithConstraints "
           << "with " << formats.size() << " formats";
  // Find the first track descriptor that can fulfil the constraints.
  for (const auto& track : track_descriptors_) {
    const blink::WebMediaConstraints& track_constraints = track.constraints;

    // If the source doesn't support capability enumeration it is still ok if
    // no mandatory constraints have been specified. That just means that
    // we will start with whatever format is native to the source.
    if (formats.empty() && !HasMandatoryConstraints(track_constraints)) {
      DVLOG(3) << "No mandatory constraints and no formats";
      *fulfilled_constraints = track_constraints;
      *best_format = media::VideoCaptureFormat();
      return true;
    }
    std::string unsatisfied_constraint;
    const media::VideoCaptureFormats filtered_formats =
        FilterFormats(track_constraints, formats, &unsatisfied_constraint);
    if (filtered_formats.empty())
      continue;

    // A request with constraints that can be fulfilled.
    *fulfilled_constraints = track_constraints;
    media::VideoCaptureFormat best_format_candidate =
        GetBestCaptureFormat(filtered_formats, track_constraints);
    if (!best_format_candidate.IsValid())
      continue;

    *best_format = best_format_candidate;
    DVLOG(3) << "Found a track that matches the constraints";
    return true;
  }
  DVLOG(3) << "No usable format found";
  return false;
}

void MediaStreamVideoSource::OnStartDone(MediaStreamRequestResult result) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnStartDone({result =" << result << "})";
  if (result == MEDIA_DEVICE_OK) {
    DCHECK_EQ(STARTING, state_);
    state_ = STARTED;
    SetReadyState(blink::WebMediaStreamSource::kReadyStateLive);
    double frame_rate =
        GetCurrentFormat() ? GetCurrentFormat()->frame_rate : 0.0;
    track_adapter_->StartFrameMonitoring(
        frame_rate, base::Bind(&MediaStreamVideoSource::SetMutedState,
                               weak_factory_.GetWeakPtr()));
  } else {
    StopSource();
  }

  // This object can be deleted after calling FinalizeAddTrack. See comment in
  // the header file.
  if (IsOldVideoConstraints())
    FinalizeAddTrackLegacy();
  else
    FinalizeAddTrack();
}

void MediaStreamVideoSource::FinalizeAddTrackLegacy() {
  DCHECK(CalledOnValidThread());
  DCHECK(IsOldVideoConstraints());
  const media::VideoCaptureFormats formats(1, current_format_);

  std::vector<TrackDescriptor> track_descriptors;
  track_descriptors.swap(track_descriptors_);
  for (const auto& track : track_descriptors) {
    MediaStreamRequestResult result = MEDIA_DEVICE_OK;
    std::string unsatisfied_constraint;

    if (HasMandatoryConstraints(track.constraints) &&
        FilterFormats(track.constraints, formats, &unsatisfied_constraint)
            .empty()) {
      result = MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      DVLOG(3) << "FinalizeAddTrackLegacy() ignoring device on constraint "
               << unsatisfied_constraint;
    }

    if (state_ != STARTED && result == MEDIA_DEVICE_OK)
      result = MEDIA_DEVICE_TRACK_START_FAILURE;

    if (result == MEDIA_DEVICE_OK) {
      int max_width;
      int max_height;
      GetDesiredMaxWidthAndHeight(track.constraints, &max_width, &max_height);
      double max_aspect_ratio;
      double min_aspect_ratio;
      GetDesiredMinAndMaxAspectRatio(track.constraints,
                                     &min_aspect_ratio,
                                     &max_aspect_ratio);
      double max_frame_rate = 0.0f;
      // Note: Optional and ideal constraints are ignored; this is
      // purely a hard max limit.
      if (track.constraints.Basic().frame_rate.HasMax())
        max_frame_rate = track.constraints.Basic().frame_rate.Max();

      track_adapter_->AddTrack(
          track.track, track.frame_callback,
          VideoTrackAdapterSettings(max_width, max_height, min_aspect_ratio,
                                    max_aspect_ratio, max_frame_rate));
      // Calculate resulting frame size if the source delivers frames
      // according to the current format. Note: Format may change later.
      gfx::Size desired_size;
      VideoTrackAdapter::CalculateTargetSize(
          current_format_.frame_size, gfx::Size(max_width, max_height),
          min_aspect_ratio, max_aspect_ratio, &desired_size);
      track.track->SetTargetSizeAndFrameRate(
          desired_size.width(), desired_size.height(), max_frame_rate);
    }

    DVLOG(3) << "FinalizeAddTrackLegacy() result " << result;

    if (!track.callback.is_null())
      track.callback.Run(this, result,
                         blink::WebString::FromUTF8(unsatisfied_constraint));
  }
}

void MediaStreamVideoSource::FinalizeAddTrack() {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsOldVideoConstraints());
  std::vector<TrackDescriptor> track_descriptors;
  track_descriptors.swap(track_descriptors_);
  for (const auto& track : track_descriptors) {
    MediaStreamRequestResult result = MEDIA_DEVICE_OK;
    if (state_ != STARTED)
      result = MEDIA_DEVICE_TRACK_START_FAILURE;

    if (result == MEDIA_DEVICE_OK) {
      track_adapter_->AddTrack(track.track, track.frame_callback,
                               *track.adapter_settings);

      // Calculate resulting frame size if the source delivers frames
      // according to the current format. Note: Format may change later.
      gfx::Size desired_size;
      VideoTrackAdapter::CalculateTargetSize(
          GetCurrentFormat() ? GetCurrentFormat()->frame_size
                             : gfx::Size(track.adapter_settings->max_width,
                                         track.adapter_settings->max_height),
          gfx::Size(track.adapter_settings->max_width,
                    track.adapter_settings->max_height),
          track.adapter_settings->min_aspect_ratio,
          track.adapter_settings->max_aspect_ratio, &desired_size);
      track.track->SetTargetSizeAndFrameRate(
          desired_size.width(), desired_size.height(),
          track.adapter_settings->max_frame_rate);
    }

    if (!track.callback.is_null())
      track.callback.Run(this, result, blink::WebString());
  }
}

void MediaStreamVideoSource::SetReadyState(
    blink::WebMediaStreamSource::ReadyState state) {
  DVLOG(3) << "MediaStreamVideoSource::SetReadyState state " << state;
  DCHECK(CalledOnValidThread());
  if (!Owner().IsNull())
    Owner().SetReadyState(state);
  for (auto* track : tracks_)
    track->OnReadyStateChanged(state);
}

void MediaStreamVideoSource::SetMutedState(bool muted_state) {
  DVLOG(3) << "MediaStreamVideoSource::SetMutedState state=" << muted_state;
  DCHECK(CalledOnValidThread());
  if (!Owner().IsNull()) {
    Owner().SetReadyState(muted_state
                              ? blink::WebMediaStreamSource::kReadyStateMuted
                              : blink::WebMediaStreamSource::kReadyStateLive);
  }
}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback)
    : track(track),
      frame_callback(frame_callback),
      constraints(constraints),
      callback(callback) {
  DCHECK(IsOldVideoConstraints());
}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    std::unique_ptr<VideoTrackAdapterSettings> adapter_settings,
    const ConstraintsCallback& callback)
    : track(track),
      frame_callback(frame_callback),
      adapter_settings(std::move(adapter_settings)),
      callback(callback) {
  DCHECK(!IsOldVideoConstraints());
}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    TrackDescriptor&& other) = default;
MediaStreamVideoSource::TrackDescriptor&
MediaStreamVideoSource::TrackDescriptor::operator=(
    MediaStreamVideoSource::TrackDescriptor&& other) = default;

MediaStreamVideoSource::TrackDescriptor::~TrackDescriptor() {
}

}  // namespace content
