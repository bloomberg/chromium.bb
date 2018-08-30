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
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

class MediaSessionImpl;

class CONTENT_EXPORT AudioFocusManager {
 public:
  // Returns Chromium's internal AudioFocusManager.
  static AudioFocusManager* GetInstance();

  void RequestAudioFocus(MediaSessionImpl* media_session,
                         media_session::mojom::AudioFocusType type);

  void AbandonAudioFocus(MediaSessionImpl* media_session);

  media_session::mojom::AudioFocusType GetFocusTypeForSession(
      MediaSessionImpl* media_session) const;

  // Adds/removes audio focus observers.
  mojo::InterfacePtrSetElementId AddObserver(
      media_session::mojom::AudioFocusObserverPtr);
  void RemoveObserver(mojo::InterfacePtrSetElementId);

 private:
  friend struct base::DefaultSingletonTraits<AudioFocusManager>;
  friend class AudioFocusManagerTest;

  // Media internals UI needs access to internal state.
  friend class MediaInternalsAudioFocusTest;
  friend class MediaInternals;

  // Flush for testing will flush any pending messages to the observers.
  void FlushForTesting();

  // Reset for testing will clear any built up internal state.
  void ResetForTesting();

  AudioFocusManager();
  ~AudioFocusManager();

  void MaybeRemoveFocusEntry(MediaSessionImpl* media_session);

  // Weak reference of managed observers. Observers are expected to remove
  // themselves before being destroyed.
  mojo::InterfacePtrSet<media_session::mojom::AudioFocusObserver> observers_;

  // Weak reference of managed MediaSessions and their requested focus type.
  // A MediaSession must abandon audio focus before its destruction.
  struct StackRow {
    StackRow(MediaSessionImpl* media_session,
             media_session::mojom::AudioFocusType audio_focus_type)
        : media_session(media_session), audio_focus_type(audio_focus_type) {}
    MediaSessionImpl* media_session;
    media_session::mojom::AudioFocusType audio_focus_type;
  };
  std::list<StackRow> audio_focus_stack_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
