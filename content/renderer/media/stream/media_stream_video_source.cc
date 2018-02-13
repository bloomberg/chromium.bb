// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_video_source.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_process.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/stream/media_stream_constraints_util_video_device.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/stream/video_track_adapter.h"

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

  pending_tracks_.push_back(PendingTrackInfo(
      track, frame_callback,
      std::make_unique<VideoTrackAdapterSettings>(track_adapter_settings),
      callback));

  switch (state_) {
    case NEW: {
      state_ = STARTING;
      StartSourceImpl(
          base::Bind(&VideoTrackAdapter::DeliverFrameOnIO, track_adapter_));
      break;
    }
    case STARTING:
    case STOPPING_FOR_RESTART:
    case STOPPED_FOR_RESTART:
    case RESTARTING: {
      // These cases are handled by OnStartDone(), OnStoppedForRestartDone()
      // and OnRestartDone().
      break;
    }
    case ENDED:
    case STARTED: {
      FinalizeAddPendingTracks();
      break;
    }
  }
}

void MediaStreamVideoSource::RemoveTrack(MediaStreamVideoTrack* video_track,
                                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    std::vector<MediaStreamVideoTrack*>::iterator it =
        std::find(tracks_.begin(), tracks_.end(), video_track);
    DCHECK(it != tracks_.end());
    tracks_.erase(it);
  }
  secure_tracker_.Remove(video_track);

  {
    auto it = std::find(suspended_tracks_.begin(), suspended_tracks_.end(),
                        video_track);
    if (it != suspended_tracks_.end())
      suspended_tracks_.erase(it);
  }

  for (auto it = pending_tracks_.begin(); it != pending_tracks_.end(); ++it) {
    if (it->track == video_track) {
      pending_tracks_.erase(it);
      break;
    }
  }

  // Call |frame_adapter_->RemoveTrack| here even if adding the track has
  // failed and |frame_adapter_->AddCallback| has not been called.
  track_adapter_->RemoveTrack(video_track);

  if (tracks_.empty()) {
    if (callback) {
      // Use StopForRestart() in order to get a notification of when the
      // source is actually stopped (if supported). The source will not be
      // restarted.
      // The intent is to have the same effect as StopSource() (i.e., having
      // the readyState updated and invoking the source's stop callback on this
      // task), but getting a notification of when the source has actually
      // stopped so that clients have a mechanism to serialize the creation and
      // destruction of video sources. Without such serialization it is possible
      // that concurrent creation and destruction of sources that share the same
      // underlying implementation results in failed source creation since
      // stopping a source with StopSource() can have side effects that affect
      // sources created after that StopSource() call, but before the actual
      // stop takes place. See http://crbug.com/778039.
      StopForRestart(base::BindOnce(&MediaStreamVideoSource::DidRemoveLastTrack,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
      if (state_ == STOPPING_FOR_RESTART || state_ == STOPPED_FOR_RESTART) {
        // If the source supports restarting, it is necessary to call
        // FinalizeStopSource() to ensure the same behavior as StopSource(),
        // even if the underlying implementation takes longer to actually stop.
        // In particular, Tab capture and capture from element require the
        // source's stop callback to be invoked on this task in order to ensure
        // correct behavior.
        FinalizeStopSource();
      } else {
        // If the source does not support restarting, call StopSource()
        // to ensure stop on this task. DidRemoveLastTrack() will be called on
        // another task even if the source does not support restarting, as
        // StopForRestart() always posts a task to run its callback.
        StopSource();
      }
    } else {
      StopSource();
    }
  } else if (callback) {
    std::move(callback).Run();
  }
}

void MediaStreamVideoSource::DidRemoveLastTrack(base::OnceClosure callback,
                                                RestartResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  DCHECK(tracks_.empty());
  DCHECK_EQ(Owner().GetReadyState(),
            blink::WebMediaStreamSource::kReadyStateEnded);
  if (result == RestartResult::IS_STOPPED) {
    state_ = ENDED;
  }

  if (state_ != ENDED) {
    // This can happen if a source that supports StopForRestart() fails to
    // actually stop the source after trying to stop it. The contract for
    // StopForRestart() allows this, but it should not happen in practice.
    LOG(WARNING) << "Source unexpectedly failed to stop. Force stopping and "
                    "sending notification anyway";
    StopSource();
  }
  std::move(callback).Run();
}

void MediaStreamVideoSource::ReconfigureTrack(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& adapter_settings) {
  track_adapter_->ReconfigureTrack(track, adapter_settings);
  // It's OK to reconfigure settings even if ReconfigureTrack fails, provided
  // |track| is not connected to a different source, which is a precondition
  // for calling this method.
  UpdateTrackSettings(track, adapter_settings);
}

void MediaStreamVideoSource::StopForRestart(RestartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != STARTED) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RestartResult::INVALID_STATE));
    return;
  }

  DCHECK(!restart_callback_);
  track_adapter_->StopFrameMonitoring();
  state_ = STOPPING_FOR_RESTART;
  restart_callback_ = std::move(callback);
  StopSourceForRestartImpl();
}

void MediaStreamVideoSource::StopSourceForRestartImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, STOPPING_FOR_RESTART);
  OnStopForRestartDone(false);
}

void MediaStreamVideoSource::OnStopForRestartDone(bool did_stop_for_restart) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == ENDED) {
    return;
  }

  DCHECK_EQ(state_, STOPPING_FOR_RESTART);
  if (did_stop_for_restart) {
    state_ = STOPPED_FOR_RESTART;
  } else {
    state_ = STARTED;
    StartFrameMonitoring();
    FinalizeAddPendingTracks();
  }
  DCHECK(restart_callback_);

  RestartResult result = did_stop_for_restart ? RestartResult::IS_STOPPED
                                              : RestartResult::IS_RUNNING;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(restart_callback_), result));
}

void MediaStreamVideoSource::Restart(
    const media::VideoCaptureFormat& new_format,
    RestartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != STOPPED_FOR_RESTART) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RestartResult::INVALID_STATE));
    return;
  }
  DCHECK(!restart_callback_);
  state_ = RESTARTING;
  restart_callback_ = std::move(callback);
  RestartSourceImpl(new_format);
}

void MediaStreamVideoSource::RestartSourceImpl(
    const media::VideoCaptureFormat& new_format) {
  NOTREACHED();
}

void MediaStreamVideoSource::OnRestartDone(bool did_restart) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == ENDED)
    return;

  DCHECK_EQ(state_, RESTARTING);
  if (did_restart) {
    state_ = STARTED;
    StartFrameMonitoring();
    FinalizeAddPendingTracks();
  } else {
    state_ = STOPPED_FOR_RESTART;
  }

  RestartResult result =
      did_restart ? RestartResult::IS_RUNNING : RestartResult::IS_STOPPED;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(restart_callback_), result));
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

void MediaStreamVideoSource::SetDeviceRotationDetection(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enable_device_rotation_detection_ = enabled;
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

base::Optional<media::VideoCaptureParams>
MediaStreamVideoSource::GetCurrentCaptureParams() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Optional<media::VideoCaptureParams>();
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
  if (state_ == ENDED)
    return;

  if (result == MEDIA_DEVICE_OK) {
    DCHECK_EQ(STARTING, state_);
    state_ = STARTED;
    SetReadyState(blink::WebMediaStreamSource::kReadyStateLive);
    StartFrameMonitoring();
  } else {
    StopSource();
  }

  // This object can be deleted after calling FinalizeAddPendingTracks. See
  // comment in the header file.
  FinalizeAddPendingTracks();
}

void MediaStreamVideoSource::FinalizeAddPendingTracks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PendingTrackInfo> pending_track_descriptors;
  pending_track_descriptors.swap(pending_tracks_);
  for (const auto& track_info : pending_track_descriptors) {
    MediaStreamRequestResult result = MEDIA_DEVICE_OK;
    if (state_ != STARTED)
      result = MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO;

    if (result == MEDIA_DEVICE_OK) {
      track_adapter_->AddTrack(track_info.track, track_info.frame_callback,
                               *track_info.adapter_settings);
      UpdateTrackSettings(track_info.track, *track_info.adapter_settings);
    }

    if (!track_info.callback.is_null())
      track_info.callback.Run(this, result, blink::WebString());
  }
}

void MediaStreamVideoSource::StartFrameMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Optional<media::VideoCaptureFormat> current_format = GetCurrentFormat();
  double frame_rate = current_format ? current_format->frame_rate : 0.0;
  if (current_format && enable_device_rotation_detection_) {
    track_adapter_->SetSourceFrameSize(current_format->frame_size);
  }
  track_adapter_->StartFrameMonitoring(
      frame_rate, base::Bind(&MediaStreamVideoSource::SetMutedState,
                             weak_factory_.GetWeakPtr()));
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

void MediaStreamVideoSource::UpdateTrackSettings(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& adapter_settings) {
  // If the source does not provide a format, do not set any target dimensions
  // or frame rate.
  if (!GetCurrentFormat())
    return;

  // Calculate resulting frame size if the source delivers frames
  // according to the current format. Note: Format may change later.
  gfx::Size desired_size;
  VideoTrackAdapter::CalculateTargetSize(false /* is_rotated */,
                                         GetCurrentFormat()->frame_size,
                                         adapter_settings, &desired_size);
  track->SetTargetSizeAndFrameRate(desired_size.width(), desired_size.height(),
                                   adapter_settings.max_frame_rate);
}

MediaStreamVideoSource::PendingTrackInfo::PendingTrackInfo(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    std::unique_ptr<VideoTrackAdapterSettings> adapter_settings,
    const ConstraintsCallback& callback)
    : track(track),
      frame_callback(frame_callback),
      adapter_settings(std::move(adapter_settings)),
      callback(callback) {}

MediaStreamVideoSource::PendingTrackInfo::PendingTrackInfo(
    PendingTrackInfo&& other) = default;
MediaStreamVideoSource::PendingTrackInfo&
MediaStreamVideoSource::PendingTrackInfo::operator=(
    MediaStreamVideoSource::PendingTrackInfo&& other) = default;

MediaStreamVideoSource::PendingTrackInfo::~PendingTrackInfo() {}

}  // namespace content
