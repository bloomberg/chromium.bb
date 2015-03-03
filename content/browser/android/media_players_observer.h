// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_MEDIA_PLAYERS_OBSERVER_H_
#define CONTENT_BROWSER_ANDROID_MEDIA_PLAYERS_OBSERVER_H_

#include <map>

#include "base/macros.h"
#include "content/browser/media/audio_state_provider.h"

namespace content {

class RenderFrameHost;

// On Android the MediaPlayerAndroid objects report
// the audible state to us.
class MediaPlayersObserver : public AudioStateProvider {
 public:
  explicit MediaPlayersObserver(WebContents* web_contents);
  ~MediaPlayersObserver() override;

  bool IsAudioStateAvailable() const override;

  // This audio state provider does not have a monitor,
  // the method returns nullptr.
  AudioStreamMonitor* audio_stream_monitor() override;

  // These methods constitute the observer pattern, should
  // be called when corresponding event happens. They will notify
  // WebContents whenever its audible state as a whole changes.
  void OnAudibleStateChanged(RenderFrameHost* rfh, int player_id,
                             bool is_audible);
  void RemovePlayer(RenderFrameHost* rfh, int player_id);
  void RenderFrameDeleted(RenderFrameHost* rfh);

 private:
  void UpdateStatusAndNotify();

  // Audible status per player ID and frame
  typedef std::pair<RenderFrameHost*, int> Key;
  typedef std::map<Key, bool> StatusMap;
  StatusMap audio_status_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayersObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_MEDIA_PLAYERS_OBSERVER_H_
