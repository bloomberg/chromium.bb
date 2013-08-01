// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "content/public/renderer/render_view_observer.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_player_android.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace content {
class WebMediaPlayerAndroid;
class RendererMediaPlayerManager;

// This class manages IPC communication between WebMediaPlayerAndroid and the
// MediaPlayerManagerAndroid in the browser process.
class WebMediaPlayerProxyAndroid : public RenderViewObserver {
 public:
  // Construct a WebMediaPlayerProxyAndroid object for the |render_view|.
  // |manager| is passed to this class so that it can find the right
  // WebMediaPlayerAndroid using player IDs.
  WebMediaPlayerProxyAndroid(
      RenderView* render_view,
      RendererMediaPlayerManager* manager);
  virtual ~WebMediaPlayerProxyAndroid();

  // RenderViewObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Initializes a MediaPlayerAndroid object in browser process.
  void Initialize(int player_id,
                  const GURL& url,
                  media::MediaPlayerAndroid::SourceType source_type,
                  const GURL& first_party_for_cookies);

  // Starts the player.
  void Start(int player_id);

  // Pausees the player.
  void Pause(int player_id);

  // Performs seek on the player.
  void Seek(int player_id, base::TimeDelta time);

  // Set the player volume.
  void SetVolume(int player_id, double volume);

  // Release resources for the player.
  void ReleaseResources(int player_id);

  // Destroy the player in the browser process
  void DestroyPlayer(int player_id);

  // Request the player to enter fullscreen.
  void EnterFullscreen(int player_id);

  // Request the player to exit fullscreen.
  void ExitFullscreen(int player_id);

#if defined(GOOGLE_TV)
  // Request an external surface for out-of-band compositing.
  void RequestExternalSurface(int player_id, const gfx::RectF& geometry);

  // RenderViewObserver overrides.
  virtual void DidCommitCompositorFrame() OVERRIDE;
#endif

  // Media source related methods.
  void DemuxerReady(int player_id,
                    const media::MediaPlayerHostMsg_DemuxerReady_Params&);
  void ReadFromDemuxerAck(
      int player_id,
      const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params);
  void SeekRequestAck(int player_id, unsigned seek_request_id);
  void DurationChanged(int player_id, const base::TimeDelta& duration);

  // Encrypted media related methods.
  void InitializeCDM(int media_keys_id, const std::vector<uint8>& uuid);
  void GenerateKeyRequest(int media_keys_id,
                          const std::string& type,
                          const std::vector<uint8>& init_data);
  void AddKey(int media_keys_id,
              const std::vector<uint8>& key,
              const std::vector<uint8>& init_data,
              const std::string& session_id);
  void CancelKeyRequest(int media_keys_id, const std::string& session_id);

 private:
  WebMediaPlayerAndroid* GetWebMediaPlayer(int player_id);

  // Message handlers.
  void OnMediaMetadataChanged(int player_id,
                              base::TimeDelta duration,
                              int width,
                              int height,
                              bool success);
  void OnMediaPlaybackCompleted(int player_id);
  void OnMediaBufferingUpdate(int player_id, int percent);
  void OnMediaSeekCompleted(int player_id, base::TimeDelta current_time);
  void OnMediaError(int player_id, int error);
  void OnVideoSizeChanged(int player_id, int width, int height);
  void OnTimeUpdate(int player_id, base::TimeDelta current_time);
  void OnMediaPlayerReleased(int player_id);
  void OnDidExitFullscreen(int player_id);
  void OnDidEnterFullscreen(int player_id);
  void OnPlayerPlay(int player_id);
  void OnPlayerPause(int player_id);
  void OnReadFromDemuxer(int player_id,
                         media::DemuxerStream::Type type);
  void OnMediaSeekRequest(int player_id,
                          base::TimeDelta time_to_seek,
                          unsigned seek_request_id);
  void OnMediaConfigRequest(int player_id);
  void OnKeyAdded(int media_keys_id, const std::string& session_id);
  void OnKeyError(int media_keys_id,
                  const std::string& session_id,
                  media::MediaKeys::KeyError error_code,
                  int system_code);
  void OnKeyMessage(int media_keys_id,
                    const std::string& session_id,
                    const std::vector<uint8>& message,
                    const std::string& destination_url);

  RendererMediaPlayerManager* manager_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerProxyAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
