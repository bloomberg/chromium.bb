// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_manager.h"

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/browser/media/session/audio_focus_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaSessionPtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

namespace {

// Generate a unique audio focus request ID for the audio focus request. The IDs
// are only handed out by the audio focus manager.
int GenerateAudioFocusRequestId() {
  static base::AtomicSequenceNumber request_id;
  return request_id.GetNext();
}

}  // namespace

// static
AudioFocusManager* AudioFocusManager::GetInstance() {
  return base::Singleton<AudioFocusManager>::get();
}

AudioFocusManager::RequestResponse AudioFocusManager::RequestAudioFocus(
    MediaSessionPtr media_session,
    MediaSessionInfoPtr session_info,
    AudioFocusType type,
    base::Optional<RequestId> previous_id) {
  DCHECK(!session_info.is_null());
  DCHECK(media_session.is_bound());

  if (!audio_focus_stack_.empty() &&
      previous_id == audio_focus_stack_.back()->id() &&
      audio_focus_stack_.back()->audio_focus_type() == type &&
      audio_focus_stack_.back()->IsActive()) {
    // Early returning if |media_session| is already on top (has focus) and is
    // active.
    return AudioFocusManager::RequestResponse(previous_id.value(), true);
  }

  // If we have a previous ID then we should remove it from the stack.
  if (previous_id.has_value())
    RemoveFocusEntryIfPresent(previous_id.value());

  if (type == AudioFocusType::kGainTransientMayDuck) {
    for (auto& old_session : audio_focus_stack_)
      old_session->session()->StartDucking();
  } else {
    for (auto& old_session : audio_focus_stack_) {
      // If the session has the force duck bit set then we should duck the
      // session instead of suspending it.
      if (old_session->info()->force_duck) {
        old_session->session()->StartDucking();
      } else {
        old_session->session()->Suspend(MediaSession::SuspendType::kSystem);
      }
    }
  }

  audio_focus_stack_.push_back(std::make_unique<AudioFocusManager::StackRow>(
      std::move(media_session), std::move(session_info), type,
      previous_id ? *previous_id : GenerateAudioFocusRequestId()));

  AudioFocusManager::StackRow* row = audio_focus_stack_.back().get();
  row->session()->StopDucking();

  // Notify observers that we were gained audio focus.
  observers_.ForAllPtrs(
      [&row, type](media_session::mojom::AudioFocusObserver* observer) {
        observer->OnFocusGained(row->info().Clone(), type);
      });

  // We always grant the audio focus request but this may not always be the case
  // in the future.
  return AudioFocusManager::RequestResponse(row->id(), true);
}

void AudioFocusManager::AbandonAudioFocus(RequestId id) {
  if (audio_focus_stack_.empty())
    return;

  if (audio_focus_stack_.back()->id() != id) {
    RemoveFocusEntryIfPresent(id);
    return;
  }

  auto row = std::move(audio_focus_stack_.back());
  audio_focus_stack_.pop_back();

  if (audio_focus_stack_.empty()) {
    // Notify observers that we lost audio focus.
    observers_.ForAllPtrs(
        [&row](media_session::mojom::AudioFocusObserver* observer) {
          observer->OnFocusLost(row->info().Clone());
        });
    return;
  }

  // Allow the top-most MediaSession having force duck to unduck even if
  // it is not active.
  for (auto iter = audio_focus_stack_.rbegin();
       iter != audio_focus_stack_.rend(); ++iter) {
    if (!(*iter)->info()->force_duck)
      continue;

    auto duck_row = std::move(*iter);
    duck_row->session()->StopDucking();
    audio_focus_stack_.erase(std::next(iter).base());
    audio_focus_stack_.push_back(std::move(duck_row));
    return;
  }

  // Only try to unduck the new MediaSession on top. The session might be
  // still inactive but it will not be resumed (so it doesn't surprise the
  // user).
  audio_focus_stack_.back()->session()->StopDucking();

  // Notify observers that we lost audio focus.
  observers_.ForAllPtrs(
      [&row](media_session::mojom::AudioFocusObserver* observer) {
        observer->OnFocusLost(row->info().Clone());
      });
}

mojo::InterfacePtrSetElementId AudioFocusManager::AddObserver(
    media_session::mojom::AudioFocusObserverPtr observer) {
  return observers_.AddPtr(std::move(observer));
}

AudioFocusType AudioFocusManager::GetFocusTypeForSession(RequestId id) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() == id)
      return row->audio_focus_type();
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

  for (auto& session : audio_focus_stack_)
    session->FlushForTesting();
}

AudioFocusManager::AudioFocusManager() {
  // Make sure we start AudioFocusManager on the browser UI thread. This is to
  // ensure thread consistency for mojo::InterfaceSetPtr.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AudioFocusManager::~AudioFocusManager() = default;

void AudioFocusManager::RemoveFocusEntryIfPresent(
    AudioFocusManager::RequestId id) {
  for (auto iter = audio_focus_stack_.begin(); iter != audio_focus_stack_.end();
       ++iter) {
    if ((*iter)->id() == id) {
      audio_focus_stack_.erase(iter);
      break;
    }
  }
}

AudioFocusManager::StackRow::StackRow(MediaSessionPtr session,
                                      MediaSessionInfoPtr current_info,
                                      AudioFocusType audio_focus_type,
                                      AudioFocusManager::RequestId id)
    : id_(id),
      session_(std::move(session)),
      current_info_(std::move(current_info)),
      audio_focus_type_(audio_focus_type),
      binding_(this) {
  media_session::mojom::MediaSessionObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));

  // Listen for mojo errors.
  binding_.set_connection_error_handler(base::BindOnce(
      &AudioFocusManager::StackRow::OnConnectionError, base::Unretained(this)));
  session_.set_connection_error_handler(base::BindOnce(
      &AudioFocusManager::StackRow::OnConnectionError, base::Unretained(this)));

  // Listen to info changes on the MediaSession.
  session_->AddObserver(std::move(observer));
};

AudioFocusManager::StackRow::~StackRow() = default;

void AudioFocusManager::StackRow::MediaSessionInfoChanged(
    MediaSessionInfoPtr info) {
  current_info_ = std::move(info);
}

media_session::mojom::MediaSession* AudioFocusManager::StackRow::session() {
  return session_.get();
}

const media_session::mojom::MediaSessionInfoPtr&
AudioFocusManager::StackRow::info() const {
  return current_info_;
}

bool AudioFocusManager::StackRow::IsActive() const {
  return current_info_->state == MediaSessionInfo::SessionState::kActive;
}

void AudioFocusManager::StackRow::FlushForTesting() {
  session_.FlushForTesting();
  binding_.FlushForTesting();
}

void AudioFocusManager::StackRow::OnConnectionError() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AudioFocusManager::AbandonAudioFocus,
                     base::Unretained(AudioFocusManager::GetInstance()), id()));
}

}  // namespace content
