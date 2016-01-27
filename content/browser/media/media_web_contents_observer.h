// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class PowerSaveBlocker;

// This class manages all RenderFrame based media related managers at the
// browser side. It receives IPC messages from media RenderFrameObservers and
// forwards them to the corresponding managers. The managers are responsible
// for sending IPCs back to the RenderFrameObservers at the render side.
class CONTENT_EXPORT MediaWebContentsObserver : public WebContentsObserver {
 public:
  explicit MediaWebContentsObserver(WebContents* web_contents);
  ~MediaWebContentsObserver() override;

  // Called by WebContentsImpl when the audible state may have changed.
  void MaybeUpdateAudibleState();

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void WasShown() override;
  void WasHidden() override;

  bool has_audio_power_save_blocker_for_testing() const {
    return !!audio_power_save_blocker_;
  }

  bool has_video_power_save_blocker_for_testing() const {
    return !!video_power_save_blocker_;
  }

 private:
  void OnMediaDestroyed(RenderFrameHost* render_frame_host, int delegate_id);
  void OnMediaPaused(RenderFrameHost* render_frame_host,
                     int delegate_id,
                     bool reached_end_of_stream);
  void OnMediaPlaying(RenderFrameHost* render_frame_host,
                      int delegate_id,
                      bool has_video,
                      bool has_audio,
                      bool is_remote,
                      base::TimeDelta duration);

  // Clear |render_frame_host|'s tracking entry for its power save blockers.
  void ClearPowerSaveBlockers(RenderFrameHost* render_frame_host);

  // Creates an audio or video power save blocker respectively.
  void CreateAudioPowerSaveBlocker();
  void CreateVideoPowerSaveBlocker();

  // Releases the audio power save blockers if |active_audio_players_| is empty.
  // Likewise, releases the video power save blockers if |active_video_players_|
  // is empty.
  void MaybeReleasePowerSaveBlockers();

  // Helper methods for adding or removing player entries in |player_map|.
  using PlayerList = std::vector<int>;
  using ActiveMediaPlayerMap = std::map<RenderFrameHost*, PlayerList>;
  void AddMediaPlayerEntry(const MediaPlayerId& id,
                           ActiveMediaPlayerMap* player_map);
  // Returns true if an entry is actually removed.
  bool RemoveMediaPlayerEntry(const MediaPlayerId& id,
                              ActiveMediaPlayerMap* player_map);
  // Removes all entries from |player_map| for |render_frame_host|. Removed
  // entries are added to |removed_players|.
  void RemoveAllMediaPlayerEntries(RenderFrameHost* render_frame_host,
                                   ActiveMediaPlayerMap* player_map,
                                   std::set<MediaPlayerId>* removed_players);

  // Tracking variables and associated power save blockers for media playback.
  ActiveMediaPlayerMap active_audio_players_;
  ActiveMediaPlayerMap active_video_players_;
  scoped_ptr<PowerSaveBlocker> audio_power_save_blocker_;
  scoped_ptr<PowerSaveBlocker> video_power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
