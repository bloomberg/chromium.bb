// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"

namespace media {

MediaPlayerAndroid::MediaPlayerAndroid(
    int player_id,
    MediaPlayerManager* manager,
    const RequestMediaResourcesCB& request_media_resources_cb,
    const GURL& frame_url)
    : request_media_resources_cb_(request_media_resources_cb),
      player_id_(player_id),
      manager_(manager),
      frame_url_(frame_url),
      weak_factory_(this) {
  listener_.reset(new MediaPlayerListener(base::MessageLoopProxy::current(),
                                          weak_factory_.GetWeakPtr()));
}

MediaPlayerAndroid::~MediaPlayerAndroid() {}

GURL MediaPlayerAndroid::GetUrl() {
  return GURL();
}

GURL MediaPlayerAndroid::GetFirstPartyForCookies() {
  return GURL();
}

void MediaPlayerAndroid::SetCdm(BrowserCdm* /* cdm */) {
  // Players that support EME should override this.
  NOTREACHED() << "EME not supported on base MediaPlayerAndroid class.";
  return;
}

void MediaPlayerAndroid::OnVideoSizeChanged(int width, int height) {
  manager_->OnVideoSizeChanged(player_id(), width, height);
}

void MediaPlayerAndroid::OnMediaError(int error_type) {
  manager_->OnError(player_id(), error_type);
}

void MediaPlayerAndroid::OnBufferingUpdate(int percent) {
  manager_->OnBufferingUpdate(player_id(), percent);
}

void MediaPlayerAndroid::OnPlaybackComplete() {
  manager_->OnPlaybackComplete(player_id());
}

void MediaPlayerAndroid::OnMediaInterrupted() {
  manager_->OnMediaInterrupted(player_id());
}

void MediaPlayerAndroid::OnSeekComplete() {
  manager_->OnSeekComplete(player_id(), GetCurrentTime());
}

void MediaPlayerAndroid::OnMediaPrepared() {}

void MediaPlayerAndroid::AttachListener(jobject j_media_player) {
  jobject j_context = base::android::GetApplicationContext();
  DCHECK(j_context);

  listener_->CreateMediaPlayerListener(j_context, j_media_player);
}

void MediaPlayerAndroid::DetachListener() {
  listener_->ReleaseMediaPlayerListenerResources();
}


}  // namespace media
