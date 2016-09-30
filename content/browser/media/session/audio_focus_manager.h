// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_

#include <unordered_map>

#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class MediaSession;
class WebContents;

class CONTENT_EXPORT AudioFocusManager {
 public:
  enum class AudioFocusType {
    Gain,
    GainTransientMayDuck,
  };

  // Returns Chromium's internal AudioFocusManager.
  static AudioFocusManager* GetInstance();

  void RequestAudioFocus(MediaSession* media_session, AudioFocusType type);

  void AbandonAudioFocus(MediaSession* media_session);

 private:
  friend struct base::DefaultSingletonTraits<AudioFocusManager>;
  friend class AudioFocusManagerTest;

  // TODO(mlamouri): in order to allow multiple MediaSession per WebContents, we
  // will have to keep track of MediaSession's. Though, we can easily keep track
  // of WebContents' life time right now but not MediaSession's.
  class AudioFocusEntry : public WebContentsObserver {
   public:
    AudioFocusEntry(WebContents* web_contents,
                    AudioFocusManager* audio_focus_manager,
                    AudioFocusType type);

    AudioFocusType type() const;

   private:
    // WebContentsObserver implementation.
    void WebContentsDestroyed() override;

    AudioFocusManager* audio_focus_manager_;  // Owns |this|.
    AudioFocusType type_;

    DISALLOW_COPY_AND_ASSIGN(AudioFocusEntry);
  };

  AudioFocusManager();
  ~AudioFocusManager();

  void OnWebContentsDestroyed(WebContents* web_contents);

  // Internal usage of AbandonAudioFocus using WebContents.
  void AbandonAudioFocusInternal(WebContents* web_contents);

  // This method is meant to be called when a new session is of type
  // GainTransientMayDuck. If it is the first one, other clients will be asked
  // to duck.
  void MaybeStartDucking() const;

  // This method is meant to be called when a session is no longer of type
  // GainTransientMayDuck. If it was the last one, other clients will be asked
  // to no longer duck.
  void MaybeStopDucking() const;

  // Returns how many sessions require current audio focused session to duck.
  int TransientMayDuckEntriesCount() const;

  // Internal method to request audio focus of type AudioFocusType::Gain.
  void RequestAudioFocusGain(WebContents* web_contents);

  // Removes the entry associated with |web_contents| from the
  // |transient_entries_| if there is one.
  void MaybeRemoveTransientEntry(WebContents* web_contents);

  // Removes the focused session if it is associated with |web_contents|.
  void MaybeRemoveFocusEntry(WebContents* web_contents);

  std::unordered_map<WebContents*, std::unique_ptr<AudioFocusEntry>>
      transient_entries_;
  std::unique_ptr<AudioFocusEntry> focus_entry_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
