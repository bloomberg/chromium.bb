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

bool MediaPlayerAndroid::IsRemote() const {
  return false;
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

void MediaPlayerAndroid::OnKeyAdded() {
  // Not all players care about the decryption key. Do nothing by default.
  return;
}

}  // namespace media
