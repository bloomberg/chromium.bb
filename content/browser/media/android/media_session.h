// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "content/browser/media/android/media_session_uma_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class MediaSessionBrowserTest;

namespace content {

class MediaSessionObserver;

// MediaSession manages the Android AudioFocus for a given WebContents. It is
// requesting the audio focus, pausing when requested by the system and dropping
// it on demand.
// The audio focus can be of two types: Transient or Content. A Transient audio
// focus will allow other players to duck instead of pausing and will be
// declared as temporary to the system. A Content audio focus will not be
// declared as temporary and will not allow other players to duck. If a given
// WebContents can only have one audio focus at a time, it will be Content in
// case of Transient and Content audio focus are both requested.
// Android system interaction occurs in the Java counterpart to this class.
class CONTENT_EXPORT MediaSession
    : public WebContentsObserver,
      protected WebContentsUserData<MediaSession> {
 public:
  enum class Type {
    Content,
    Transient
  };

  static bool RegisterMediaSession(JNIEnv* env);

  // Returns the MediaSession associated to this WebContents. Creates one if
  // none is currently available.
  static MediaSession* Get(WebContents* web_contents);

  ~MediaSession() override;

  // Adds the given player to the current media session. Returns whether the
  // player was successfully added. If it returns false, AddPlayer() should be
  // called again later.
  bool AddPlayer(MediaSessionObserver* observer, int player_id, Type type);

  // Removes the given player from the current media session. Abandons audio
  // focus if that was the last player in the session.
  void RemovePlayer(MediaSessionObserver* observer, int player_id);

  // Removes all the players associated with |observer|. Abandons audio focus if
  // these were the last players in the session.
  void RemovePlayers(MediaSessionObserver* observer);

  // Called when the Android system requests the MediaSession to be suspended.
  // Called by Java through JNI.
  void OnSuspend(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jboolean temporary);

  // Called when the Android system requests the MediaSession to be resumed.
  // Called by Java through JNI.
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called when a player is paused in the content.
  // If the paused player is the last player, we suspend the MediaSession.
  // Otherwise, the paused player will be removed from the MediaSession.
  void OnPlayerPaused(MediaSessionObserver* observer, int player_id);

  // Called when the user requests resuming the session. No-op if the session is
  // not controllable.
  void Resume();

  // Called when the user requests suspending the session. No-op if the session
  // is not controllable.
  void Suspend();

  // Called when the user requests stopping the session.
  void Stop();

  // Returns if the session can be controlled by Resume() and Suspend calls
  // above.
  bool IsControllable() const;

  // Returns if the session is currently suspended.
  bool IsSuspended() const;

 private:
  friend class content::WebContentsUserData<MediaSession>;
  friend class ::MediaSessionBrowserTest;

  // Resets the |j_media_session_| ref to prevent calling the Java backend
  // during content_browsertests.
  void ResetJavaRefForTest();
  bool IsActiveForTest() const;
  Type audio_focus_type_for_test() const;
  void RemoveAllPlayersForTest();
  MediaSessionUmaHelper* uma_helper_for_test();

  enum class State {
    ACTIVE,
    SUSPENDED,
    INACTIVE
  };

  enum class SuspendType {
    // Suspended by the system because a transient sound needs to be played.
    SYSTEM,
    // Suspended by the UI.
    UI,
    // Suspended by the page via script or user interaction.
    CONTENT,
  };

  // Representation of a player for the MediaSession.
  struct PlayerIdentifier {
    PlayerIdentifier(MediaSessionObserver* observer, int player_id);
    PlayerIdentifier(const PlayerIdentifier&) = default;

    void operator=(const PlayerIdentifier&) = delete;
    bool operator==(const PlayerIdentifier& player_identifier) const;

    // Hash operator for base::hash_map<>.
    struct Hash {
      size_t operator()(const PlayerIdentifier& player_identifier) const;
    };

    MediaSessionObserver* observer;
    int player_id;
  };
  using PlayersMap = base::hash_set<PlayerIdentifier, PlayerIdentifier::Hash>;

  explicit MediaSession(WebContents* web_contents);

  // Setup the JNI.
  void Initialize();

  void OnSuspendInternal(SuspendType type, State new_state);
  void OnResumeInternal(SuspendType type);

  // Requests audio focus to Android using |j_media_session_|.
  // Returns whether the request was granted. If |j_media_session_| is null, it
  // will always return true.
  bool RequestSystemAudioFocus(Type type);

  // To be called after a call to AbandonAudioFocus() in order to call the Java
  // MediaSession if the audio focus really need to be abandoned.
  void AbandonSystemAudioFocusIfNeeded();

  // Notifies WebContents about the state change of the media session.
  void UpdateWebContents();

  // Internal method that should be used instead of setting audio_focus_state_.
  // It sets audio_focus_state_ and notifies observers about the state change.
  void SetAudioFocusState(State audio_focus_state);

  base::android::ScopedJavaGlobalRef<jobject> j_media_session_;
  PlayersMap players_;

  State audio_focus_state_;
  SuspendType suspend_type_;
  Type audio_focus_type_;

  MediaSessionUmaHelper uma_helper_;

  DISALLOW_COPY_AND_ASSIGN(MediaSession);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_H_
