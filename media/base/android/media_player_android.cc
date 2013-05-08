// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_android.h"

#include "media/base/android/media_player_manager.h"

namespace media {

MediaPlayerAndroid::MediaPlayerAndroid(
    int player_id,
    MediaPlayerManager* manager,
    const MediaErrorCB& media_error_cb,
    const VideoSizeChangedCB& video_size_changed_cb,
    const BufferingUpdateCB& buffering_update_cb,
    const MediaMetadataChangedCB& media_metadata_changed_cb,
    const PlaybackCompleteCB& playback_complete_cb,
    const SeekCompleteCB& seek_complete_cb,
    const TimeUpdateCB& time_update_cb,
    const MediaInterruptedCB& media_interrupted_cb)
    : player_id_(player_id),
      manager_(manager),
      media_error_cb_(media_error_cb),
      video_size_changed_cb_(video_size_changed_cb),
      buffering_update_cb_(buffering_update_cb),
      media_metadata_changed_cb_(media_metadata_changed_cb),
      playback_complete_cb_(playback_complete_cb),
      seek_complete_cb_(seek_complete_cb),
      media_interrupted_cb_(media_interrupted_cb),
      time_update_cb_(time_update_cb) {
}

MediaPlayerAndroid::~MediaPlayerAndroid() {}

void MediaPlayerAndroid::OnMediaError(int error_type) {
  media_error_cb_.Run(player_id_, error_type);
}

void MediaPlayerAndroid::OnVideoSizeChanged(int width, int height) {
  video_size_changed_cb_.Run(player_id_, width, height);
}

void MediaPlayerAndroid::OnBufferingUpdate(int percent) {
  buffering_update_cb_.Run(player_id_, percent);
}

void MediaPlayerAndroid::OnPlaybackComplete() {
  playback_complete_cb_.Run(player_id_);
}

void MediaPlayerAndroid::OnMediaInterrupted() {
  media_interrupted_cb_.Run(player_id_);
}

void MediaPlayerAndroid::OnSeekComplete() {
  seek_complete_cb_.Run(player_id_, GetCurrentTime());
}

void MediaPlayerAndroid::OnTimeUpdated() {
  time_update_cb_.Run(player_id_, GetCurrentTime());
}

void MediaPlayerAndroid::OnMediaMetadataChanged(
    base::TimeDelta duration, int width, int height, bool success) {
  media_metadata_changed_cb_.Run(player_id_, duration, width, height, success);
}

void MediaPlayerAndroid::RequestMediaResourcesFromManager() {
  if (manager_)
    manager_->RequestMediaResources(this);
}

void MediaPlayerAndroid::ReleaseMediaResourcesFromManager() {
  if (manager_)
    manager_->ReleaseMediaResources(this);
}

#if defined(GOOGLE_TV)
void MediaPlayerAndroid::DemuxerReady(
    const MediaPlayerHostMsg_DemuxerReady_Params& params) {
  NOTREACHED() << "Unexpected ipc received";
}

void MediaPlayerAndroid::ReadFromDemuxerAck(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  NOTREACHED() << "Unexpected ipc received";
}
#endif

}  // namespace media
