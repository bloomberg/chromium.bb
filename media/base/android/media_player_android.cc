// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_android.h"

#include "base/logging.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"

namespace media {

MediaPlayerAndroid::MediaPlayerAndroid(
    int player_id,
    MediaPlayerManager* manager)
    : player_id_(player_id),
      manager_(manager) {
}

MediaPlayerAndroid::~MediaPlayerAndroid() {}

void MediaPlayerAndroid::OnMediaError(int error_type) {
  manager_->OnError(player_id_, error_type);
}

void MediaPlayerAndroid::OnVideoSizeChanged(int width, int height) {
  manager_->OnVideoSizeChanged(player_id_, width, height);
}

void MediaPlayerAndroid::OnBufferingUpdate(int percent) {
  manager_->OnBufferingUpdate(player_id_, percent);
}

void MediaPlayerAndroid::OnPlaybackComplete() {
  manager_->OnPlaybackComplete(player_id_);
}

void MediaPlayerAndroid::OnMediaInterrupted() {
  manager_->OnMediaInterrupted(player_id_);
}

void MediaPlayerAndroid::OnSeekComplete() {
  manager_->OnSeekComplete(player_id_, GetCurrentTime());
}

void MediaPlayerAndroid::OnTimeUpdated() {
  manager_->OnTimeUpdate(player_id_, GetCurrentTime());
}

void MediaPlayerAndroid::OnMediaMetadataChanged(
    base::TimeDelta duration, int width, int height, bool success) {
  manager_->OnMediaMetadataChanged(
      player_id_, duration, width, height, success);
}

void MediaPlayerAndroid::RequestMediaResourcesFromManager() {
  if (manager_)
    manager_->RequestMediaResources(player_id_);
}

void MediaPlayerAndroid::ReleaseMediaResourcesFromManager() {
  if (manager_)
    manager_->ReleaseMediaResources(player_id_);
}

void MediaPlayerAndroid::DemuxerReady(
    const MediaPlayerHostMsg_DemuxerReady_Params& params) {
  NOTREACHED() << "Unexpected ipc received";
}

void MediaPlayerAndroid::ReadFromDemuxerAck(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  NOTREACHED() << "Unexpected ipc received";
}

void MediaPlayerAndroid::OnSeekRequestAck(unsigned seek_request_id) {
  NOTREACHED() << "Unexpected ipc received";
}

void MediaPlayerAndroid::DurationChanged(const base::TimeDelta& duration) {
  NOTREACHED() << "Unexpected ipc received";
}

GURL MediaPlayerAndroid::GetUrl() {
  return GURL();
}

GURL MediaPlayerAndroid::GetFirstPartyForCookies() {
  return GURL();
}

void MediaPlayerAndroid::SetDrmBridge(MediaDrmBridge* drm_bridge) {
  // Not all players support DrmBridge. Do nothing by default.
  return;
}

}  // namespace media
