// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_manager.h"

#include "content/browser/media/session/audio_focus_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

using media_session::mojom::AudioFocusType;

namespace {

media_session::mojom::MediaSessionPtr GetSessionMojoPtr(
    MediaSessionImpl* session) {
  media_session::mojom::MediaSessionPtr media_session_ptr;
  session->BindToMojoRequest(mojo::MakeRequest(&media_session_ptr));
  return media_session_ptr;
}

}  // namespace

// static
AudioFocusManager* AudioFocusManager::GetInstance() {
  return base::Singleton<AudioFocusManager>::get();
}

void AudioFocusManager::RequestAudioFocus(MediaSessionImpl* media_session,
                                          AudioFocusType type) {
  if (!audio_focus_stack_.empty() &&
      audio_focus_stack_.back().media_session == media_session &&
      audio_focus_stack_.back().audio_focus_type == type &&
      audio_focus_stack_.back().media_session->IsActive()) {
    // Early returning if |media_session| is already on top (has focus) and is
    // active.
    return;
  }

  MaybeRemoveFocusEntry(media_session);

  // TODO(zqzhang): It seems like MediaSessionImpl is exposed to
  // AudioFocusManager
  // too much. Maybe it's better to do some abstraction and refactoring to clean
  // up the relation between AudioFocusManager and MediaSessionImpl.
  // See https://crbug.com/651069
  if (type == AudioFocusType::kGainTransientMayDuck) {
    for (auto& old_session : audio_focus_stack_) {
      old_session.media_session->StartDucking();
    }
  } else {
    for (auto& old_session : audio_focus_stack_) {
      if (old_session.media_session->IsActive()) {
        if (old_session.media_session->HasPepper())
          old_session.media_session->StartDucking();
        else
          old_session.media_session->Suspend(
              MediaSessionImpl::SuspendType::kSystem);
      }
    }
  }

  // Store the MediaSession and requested focus type.
  audio_focus_stack_.emplace_back(media_session, type);
  audio_focus_stack_.back().media_session->StopDucking();

  // Notify observers that we were gained audio focus.
  observers_.ForAllPtrs(
      [media_session,
       type](media_session::mojom::AudioFocusObserver* observer) {
        observer->OnFocusGained(GetSessionMojoPtr(media_session), type);
      });
}

void AudioFocusManager::AbandonAudioFocus(MediaSessionImpl* media_session) {
  if (audio_focus_stack_.empty())
    return;

  if (audio_focus_stack_.back().media_session != media_session) {
    MaybeRemoveFocusEntry(media_session);
    return;
  }

  audio_focus_stack_.pop_back();
  if (audio_focus_stack_.empty()) {
    // Notify observers that we lost audio focus.
    observers_.ForAllPtrs(
        [media_session](media_session::mojom::AudioFocusObserver* observer) {
          observer->OnFocusLost(GetSessionMojoPtr(media_session));
        });
    return;
  }

  // Allow the top-most MediaSessionImpl having Pepper to unduck pepper even if
  // it's
  // not active.
  for (auto iter = audio_focus_stack_.rbegin();
       iter != audio_focus_stack_.rend(); ++iter) {
    if (!iter->media_session->HasPepper())
      continue;

    MediaSessionImpl* pepper_session = iter->media_session;
    AudioFocusType focus_type = iter->audio_focus_type;
    pepper_session->StopDucking();
    MaybeRemoveFocusEntry(pepper_session);
    audio_focus_stack_.emplace_back(pepper_session, focus_type);
    return;
  }
  // Only try to unduck the new MediaSessionImpl on top. The session might be
  // still
  // inactive but it will not be resumed (so it doesn't surprise the user).
  audio_focus_stack_.back().media_session->StopDucking();

  // Notify observers that we lost audio focus.
  observers_.ForAllPtrs(
      [media_session](media_session::mojom::AudioFocusObserver* observer) {
        observer->OnFocusLost(GetSessionMojoPtr(media_session));
      });
}

mojo::InterfacePtrSetElementId AudioFocusManager::AddObserver(
    media_session::mojom::AudioFocusObserverPtr observer) {
  return observers_.AddPtr(std::move(observer));
}

AudioFocusType AudioFocusManager::GetFocusTypeForSession(
    MediaSessionImpl* media_session) const {
  for (auto row : audio_focus_stack_) {
    if (row.media_session == media_session)
      return row.audio_focus_type;
  }

  NOTREACHED();
  return AudioFocusType::kGain;
}

void AudioFocusManager::RemoveObserver(mojo::InterfacePtrSetElementId id) {
  observers_.RemovePtr(id);
}

void AudioFocusManager::ResetForTesting() {
  audio_focus_stack_.clear();
  observers_.CloseAll();
}

void AudioFocusManager::FlushForTesting() {
  observers_.FlushForTesting();
}

AudioFocusManager::AudioFocusManager() {
  // Make sure we start AudioFocusManager on the browser UI thread. This is to
  // ensure thread consistency for mojo::InterfaceSetPtr.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AudioFocusManager::~AudioFocusManager() = default;

void AudioFocusManager::MaybeRemoveFocusEntry(MediaSessionImpl* media_session) {
  for (auto iter = audio_focus_stack_.begin(); iter != audio_focus_stack_.end();
       ++iter) {
    if (iter->media_session == media_session) {
      audio_focus_stack_.erase(iter);
      break;
    }
  }
}

}  // namespace content
