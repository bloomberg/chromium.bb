// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_manager.h"

#include "content/browser/media/session/media_session.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

const double kDuckingVolumeMultiplier = 0.2;
const double kDefaultVolumeMultiplier = 1.0;

}  // anonymous namespace

AudioFocusManager::AudioFocusEntry::AudioFocusEntry(
    WebContents* web_contents,
    AudioFocusManager* audio_focus_manager,
    AudioFocusType type)
    : WebContentsObserver(web_contents),
      audio_focus_manager_(audio_focus_manager) {}

AudioFocusManager::AudioFocusType
AudioFocusManager::AudioFocusEntry::type() const {
  return type_;
}

void AudioFocusManager::AudioFocusEntry::WebContentsDestroyed() {
  audio_focus_manager_->OnWebContentsDestroyed(web_contents());
  // |this| will be destroyed now.
}

// static
AudioFocusManager* AudioFocusManager::GetInstance() {
  return base::Singleton<AudioFocusManager>::get();
}

void AudioFocusManager::RequestAudioFocus(MediaSession* media_session,
                                          AudioFocusType type) {
  WebContents* web_contents = media_session->web_contents();

  if (type == AudioFocusType::GainTransientMayDuck) {
    MaybeRemoveFocusEntry(web_contents);
    transient_entries_[web_contents].reset(
        new AudioFocusEntry(web_contents, this, type));
    MaybeStartDucking();
    return;
  }

  DCHECK(type == AudioFocusType::Gain);
  RequestAudioFocusGain(web_contents);
}

void AudioFocusManager::AbandonAudioFocus(MediaSession* media_session) {
  AbandonAudioFocusInternal(media_session->web_contents());
}

AudioFocusManager::AudioFocusManager() = default;

AudioFocusManager::~AudioFocusManager() = default;

void AudioFocusManager::RequestAudioFocusGain(WebContents* web_contents) {
  MaybeRemoveTransientEntry(web_contents);

  if (focus_entry_) {
    if (focus_entry_->web_contents() == web_contents)
      return;

    MediaSession* other_session =
        MediaSession::Get(focus_entry_->web_contents());
    if (other_session->IsActive())
      other_session->Suspend(MediaSession::SuspendType::SYSTEM);
  }

  focus_entry_.reset(
      new AudioFocusEntry(web_contents, this, AudioFocusType::Gain));
  MaybeStartDucking();
}

void AudioFocusManager::OnWebContentsDestroyed(WebContents* web_contents) {
  AbandonAudioFocusInternal(web_contents);
}

void AudioFocusManager::AbandonAudioFocusInternal(WebContents* web_contents) {
  MaybeRemoveTransientEntry(web_contents);
  MaybeRemoveFocusEntry(web_contents);
}

void AudioFocusManager::MaybeStartDucking() const {
  if (TransientMayDuckEntriesCount() != 1 || !focus_entry_)
    return;

  // TODO(mlamouri): add StartDuck to MediaSession.
  MediaSession::Get(focus_entry_->web_contents())
      ->SetVolumeMultiplier(kDuckingVolumeMultiplier);
}

void AudioFocusManager::MaybeStopDucking() const {
  if (TransientMayDuckEntriesCount() != 0 || !focus_entry_)
    return;

  // TODO(mlamouri): add StopDuck to MediaSession.
  MediaSession::Get(focus_entry_->web_contents())
      ->SetVolumeMultiplier(kDefaultVolumeMultiplier);
}

int AudioFocusManager::TransientMayDuckEntriesCount() const {
  return transient_entries_.size();
}

void AudioFocusManager::MaybeRemoveTransientEntry(WebContents* web_contents) {
  transient_entries_.erase(web_contents);
  MaybeStopDucking();
}

void AudioFocusManager::MaybeRemoveFocusEntry(WebContents* web_contents) {
  if (focus_entry_ && focus_entry_->web_contents() == web_contents) {
    MediaSession::Get(focus_entry_->web_contents())
        ->SetVolumeMultiplier(kDefaultVolumeMultiplier);
    focus_entry_.reset();
  }
}

}  // namespace content
