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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaStreamVideoSource::AddTrack(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& track_adapter_settings,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const ConstraintsCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      StartSourceImpl(
          base::Bind(&VideoTrackAdapter::DeliverFrameOnIO, track_adapter_));
      break;
    }
    case STARTING: {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  secure_tracker_.Update(track, is_secure);
  OnCapturingLinkSecured(secure_tracker_.is_capturing_secure());
}

base::SingleThreadTaskRunner* MediaStreamVideoSource::io_task_runner() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return track_adapter_->io_task_runner();
}

base::Optional<media::VideoCaptureFormat>
MediaStreamVideoSource::GetCurrentFormat() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Optional<media::VideoCaptureFormat>();
}

void MediaStreamVideoSource::DoStopSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << "DoStopSource()";
  if (state_ == ENDED)
    return;
  track_adapter_->StopFrameMonitoring();
  StopSourceImpl();
  state_ = ENDED;
  SetReadyState(blink::WebMediaStreamSource::kReadyStateEnded);
}

void MediaStreamVideoSource::OnStartDone(MediaStreamRequestResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  FinalizeAddTrack();
}

void MediaStreamVideoSource::FinalizeAddTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
          false /* is_rotated */,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Owner().IsNull())
    Owner().SetReadyState(state);
  for (auto* track : tracks_)
    track->OnReadyStateChanged(state);
}

void MediaStreamVideoSource::SetMutedState(bool muted_state) {
  DVLOG(3) << "MediaStreamVideoSource::SetMutedState state=" << muted_state;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Owner().IsNull()) {
    Owner().SetReadyState(muted_state
                              ? blink::WebMediaStreamSource::kReadyStateMuted
                              : blink::WebMediaStreamSource::kReadyStateLive);
  }
}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    std::unique_ptr<VideoTrackAdapterSettings> adapter_settings,
    const ConstraintsCallback& callback)
    : track(track),
      frame_callback(frame_callback),
      adapter_settings(std::move(adapter_settings)),
      callback(callback) {}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    TrackDescriptor&& other) = default;
MediaStreamVideoSource::TrackDescriptor&
MediaStreamVideoSource::TrackDescriptor::operator=(
    MediaStreamVideoSource::TrackDescriptor&& other) = default;

MediaStreamVideoSource::TrackDescriptor::~TrackDescriptor() {
}

}  // namespace content
