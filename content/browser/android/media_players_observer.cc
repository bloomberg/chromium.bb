// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/media_players_observer.h"

#include <climits>

#include "base/logging.h"
#include "content/public/browser/web_contents.h"

namespace content {

MediaPlayersObserver::MediaPlayersObserver(WebContents* web_contents)
    : AudioStateProvider(web_contents) {
}

MediaPlayersObserver::~MediaPlayersObserver() {}

bool MediaPlayersObserver::IsAudioStateAvailable() const {
  return true;
}

// This audio state provider does not have a monitor
AudioStreamMonitor* MediaPlayersObserver::audio_stream_monitor() {
  return nullptr;
}

void MediaPlayersObserver::OnAudibleStateChanged(RenderFrameHost* rfh,
                                                int player_id,
                                                bool is_audible) {
  audio_status_map_[Key(rfh, player_id)] = is_audible;
  UpdateStatusAndNotify();
}

void MediaPlayersObserver::RemovePlayer(RenderFrameHost* rfh, int player_id) {
  audio_status_map_.erase(Key(rfh, player_id));
  UpdateStatusAndNotify();
}

void MediaPlayersObserver::RenderFrameDeleted(RenderFrameHost* rfh) {
  StatusMap::iterator begin = audio_status_map_.lower_bound(Key(rfh, 0));
  StatusMap::iterator end = audio_status_map_.upper_bound(Key(rfh, INT_MAX));
  audio_status_map_.erase(begin, end);
  UpdateStatusAndNotify();
}

void MediaPlayersObserver::UpdateStatusAndNotify() {
  for (const auto& player_status : audio_status_map_) {
    if (player_status.second) {
      Notify(true);  // at least one player is making noise
      return;
    }
  }

  Notify(false);
}

} // namespace content
