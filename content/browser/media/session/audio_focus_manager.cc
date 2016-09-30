// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_manager.h"

#include "base/memory/ptr_util.h"
#include "content/browser/media/session/media_session.h"
#include "content/public/browser/web_contents.h"

namespace content {

// static
AudioFocusManager* AudioFocusManager::GetInstance() {
  return base::Singleton<AudioFocusManager>::get();
}

void AudioFocusManager::RequestAudioFocus(MediaSession* media_session,
                                          AudioFocusType type) {
  if (!audio_focus_stack_.empty() &&
      audio_focus_stack_.back() == media_session &&
      audio_focus_stack_.back()->audio_focus_type() == type &&
      audio_focus_stack_.back()->IsActive()) {
    // Early returning if |media_session| is already on top (has focus) and is
    // active.
    return;
  }

  MaybeRemoveFocusEntry(media_session);

  // TODO(zqzhang): It seems like MediaSession is exposed to AudioFocusManager
  // too much. Maybe it's better to do some abstraction and refactoring to clean
  // up the relation between AudioFocusManager and MediaSession.
  // See https://crbug.com/651069
  if (type == AudioFocusType::GainTransientMayDuck) {
    for (const auto old_session : audio_focus_stack_) {
      old_session->StartDucking();
    }
  } else {
    for (const auto old_session : audio_focus_stack_) {
      if (old_session->IsActive()) {
        if (old_session->HasPepper())
          old_session->StartDucking();
        else
          old_session->Suspend(MediaSession::SuspendType::SYSTEM);
      }
    }
  }

  audio_focus_stack_.push_back(media_session);
  audio_focus_stack_.back()->StopDucking();
}

void AudioFocusManager::AbandonAudioFocus(MediaSession* media_session) {
  if (audio_focus_stack_.empty())
    return;

  if (audio_focus_stack_.back() != media_session) {
    MaybeRemoveFocusEntry(media_session);
    return;
  }

  audio_focus_stack_.pop_back();
  if (audio_focus_stack_.empty())
    return;

  // Allow the top-most MediaSession having Pepper to unduck pepper even if it's
  // not active.
  for (auto iter = audio_focus_stack_.rbegin();
       iter != audio_focus_stack_.rend(); ++iter) {
    if (!(*iter)->HasPepper())
      continue;

    MediaSession* pepper_session = *iter;
    pepper_session->StopDucking();
    MaybeRemoveFocusEntry(pepper_session);
    audio_focus_stack_.push_back(pepper_session);
    return;
  }
  // Only try to unduck the new MediaSession on top. The session might be still
  // inactive but it will not be resumed (so it doesn't surprise the user).
  audio_focus_stack_.back()->StopDucking();
}

AudioFocusManager::AudioFocusManager() = default;

AudioFocusManager::~AudioFocusManager() = default;

void AudioFocusManager::MaybeRemoveFocusEntry(MediaSession* media_session) {
  audio_focus_stack_.remove(media_session);
}

}  // namespace content
