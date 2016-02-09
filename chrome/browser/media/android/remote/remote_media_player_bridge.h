// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_REMOTE_REMOTE_MEDIA_PLAYER_BRIDGE_H_
#define CHROME_BROWSER_MEDIA_ANDROID_REMOTE_REMOTE_MEDIA_PLAYER_BRIDGE_H_

#include <jni.h>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_player_bridge.h"

class SkBitmap;

// This is the version of MediaPlayerBridge that handles the remote media
// playback.

namespace remote_media {

class RemoteMediaPlayerManager;

class RemoteMediaPlayerBridge : public media::MediaPlayerAndroid {
 public:
  RemoteMediaPlayerBridge(
      MediaPlayerAndroid* local_player,
      const std::string& user_agent,
      bool hide_url_log,
      RemoteMediaPlayerManager* manager);
  ~RemoteMediaPlayerBridge() override;

  static bool RegisterRemoteMediaPlayerBridge(JNIEnv* env);

  // Initialize this object.
  virtual void Initialize();

  // MediaPlayerAndroid implementation.
  void SetVideoSurface(gfx::ScopedJavaSurface surface) override;
  void Start() override;
  void Pause(bool is_media_related_action) override;
  void SeekTo(base::TimeDelta timestamp) override;
  void Release() override;
  bool HasVideo() const override;
  bool HasAudio() const override;
  int GetVideoWidth() override;
  int GetVideoHeight() override;
  base::TimeDelta GetCurrentTime() override;
  base::TimeDelta GetDuration() override;
  bool IsPlaying() override;
  bool CanPause() override;
  bool CanSeekForward() override;
  bool CanSeekBackward() override;
  bool IsPlayerReady() override;
  GURL GetUrl() override;
  GURL GetFirstPartyForCookies() override;

  // JNI functions
  void OnPlaying(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnPaused(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnRouteUnselected(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void OnPlaybackFinished(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  void OnRouteAvailabilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean available);
  base::android::ScopedJavaLocalRef<jstring> GetTitle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void PauseLocal(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jint GetLocalPosition(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void OnCastStarting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& casting_message);
  void OnCastStopping(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnSeekCompleted(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);
  void OnError(JNIEnv *env,
               const base::android::JavaParamRef<jobject>& obj);

  // Wrappers for calls to Java used by the remote media player manager
  void RequestRemotePlayback();
  void RequestRemotePlaybackControl();
  void SetNativePlayer();
  void OnPlayerCreated();
  void OnPlayerDestroyed();

  // Gets the message to display on the embedded player while casting.
  std::string GetCastingMessage();

  // Tell the java side about the poster image for a given media.
  void SetPosterBitmap(const std::vector<SkBitmap>& bitmaps);

 protected:
  // MediaPlayerAndroid implementation.
  void OnVideoSizeChanged(int width, int height) override;
  void OnPlaybackComplete() override;
  void OnMediaInterrupted() override;

 private:
  // MediaPlayerAndroid implementation
  void UpdateEffectiveVolumeInternal(double effective_volume) override;

  // Functions that implements media player control.
  void StartInternal();
  void PauseInternal();

  // Called when |time_update_timer_| fires.
  void OnTimeUpdateTimerFired();

  // Callback function passed to |resource_getter_|. Called when the cookies
  // are retrieved.
  void OnCookiesRetrieved(const std::string& cookies);

  MediaPlayerAndroid* local_player_;
  int width_;
  int height_;
  base::RepeatingTimer time_update_timer_;
  base::TimeDelta duration_;

  // Hide url log from media player.
  bool hide_url_log_;

  // Url for playback.
  GURL url_;

  // First party url for cookies.
  GURL first_party_for_cookies_;

  // Cookies for |url_|.
  std::string cookies_;

  // User agent string to be used for media player.
  const std::string user_agent_;

  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  scoped_ptr<std::string> casting_message_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RemoteMediaPlayerBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaPlayerBridge);
};

} // namespace remote_media

#endif  // CHROME_BROWSER_MEDIA_ANDROID_REMOTE_REMOTE_MEDIA_PLAYER_BRIDGE_H_
