// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_

#include <map>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class BrowserCdmManager;
class BrowserMediaPlayerManager;

// This class manages all RenderFrame based media related managers at the
// browser side. It receives IPC messages from media RenderFrameObservers and
// forwards them to the corresponding managers. The managers are responsible
// for sending IPCs back to the RenderFrameObservers at the render side.
class CONTENT_EXPORT MediaWebContentsObserver : public WebContentsObserver {
 public:
  explicit MediaWebContentsObserver(WebContents* web_contents);
  ~MediaWebContentsObserver() override;

  // Called when the audible state has changed.  If inaudible any audio power
  // save blockers are released.
  void MaybeUpdateAudibleState(bool recently_audible);

  // WebContentsObserver implementation.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void WasShown() override;
  void WasHidden() override;

#if defined(OS_ANDROID)
  // Gets the media player manager associated with |render_frame_host|. Creates
  // a new one if it doesn't exist. The caller doesn't own the returned pointer.
  BrowserMediaPlayerManager* GetMediaPlayerManager(
      RenderFrameHost* render_frame_host);

#if defined(VIDEO_HOLE)
  void OnFrameInfoUpdated();
#endif  // defined(VIDEO_HOLE)
#endif  // defined(OS_ANDROID)

  bool has_audio_power_save_blocker_for_testing() const {
    return audio_power_save_blocker_;
  }

  bool has_video_power_save_blocker_for_testing() const {
    return video_power_save_blocker_;
  }

 private:
  bool OnMediaPlayerDelegateMessageReceived(const IPC::Message& message,
                                            RenderFrameHost* render_frame_host);

  void OnMediaPlayingNotification(RenderFrameHost* render_frame_host,
                                  int64_t player_cookie,
                                  bool has_video,
                                  bool has_audio,
                                  bool is_remote);
  void OnMediaPausedNotification(RenderFrameHost* render_frame_host,
                                 int64_t player_cookie);

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
  using PlayerList = std::vector<int64_t>;
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

#if defined(OS_ANDROID)
  // Helper functions to handle media player IPC messages. Returns whether the
  // |message| is handled in the function.
  bool OnMediaPlayerMessageReceived(const IPC::Message& message,
                                    RenderFrameHost* render_frame_host);

  bool OnMediaPlayerSetCdmMessageReceived(const IPC::Message& message,
                                          RenderFrameHost* render_frame_host);

  void OnSetCdm(RenderFrameHost* render_frame_host, int player_id, int cdm_id);

  // Map from RenderFrameHost* to BrowserMediaPlayerManager.
  using MediaPlayerManagerMap =
      base::ScopedPtrHashMap<RenderFrameHost*,
                             scoped_ptr<BrowserMediaPlayerManager>>;
  MediaPlayerManagerMap media_player_managers_;
#endif  // defined(OS_ANDROID)

  // Tracking variables and associated power save blockers for media playback.
  ActiveMediaPlayerMap active_audio_players_;
  ActiveMediaPlayerMap active_video_players_;
  scoped_ptr<PowerSaveBlocker> audio_power_save_blocker_;
  scoped_ptr<PowerSaveBlocker> video_power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
