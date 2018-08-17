// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_

#include <list>
#include <unordered_map>

#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

enum class AudioFocusType;
class AudioFocusObserver;
class MediaSessionImpl;

class CONTENT_EXPORT AudioFocusManager {
 public:
  // Returns Chromium's internal AudioFocusManager.
  static AudioFocusManager* GetInstance();

  void RequestAudioFocus(MediaSessionImpl* media_session, AudioFocusType type);

  void AbandonAudioFocus(MediaSessionImpl* media_session);

  // Adds/removes audio focus observers.
  void AddObserver(AudioFocusObserver*);
  void RemoveObserver(AudioFocusObserver*);

 private:
  friend struct base::DefaultSingletonTraits<AudioFocusManager>;
  friend class AudioFocusManagerTest;

  AudioFocusManager();
  ~AudioFocusManager();

  void MaybeRemoveFocusEntry(MediaSessionImpl* media_session);

  // Weak reference of managed observers. Observers are expected to remove
  // themselves before being destroyed.
  std::list<AudioFocusObserver*> audio_focus_observers_;

  // Weak reference of managed MediaSessions. A MediaSession must abandon audio
  // focus before its destruction.
  std::list<MediaSessionImpl*> audio_focus_stack_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
