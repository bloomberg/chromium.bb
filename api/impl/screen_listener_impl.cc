// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_listener_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if DCHECK_IS_ON()
bool IsTransitionValid(ScreenListener::State from, ScreenListener::State to) {
  switch (from) {
    case ScreenListener::State::kStopped:
      return to == ScreenListener::State::kStarting ||
             to == ScreenListener::State::kStopping;
    case ScreenListener::State::kStarting:
      return to == ScreenListener::State::kRunning ||
             to == ScreenListener::State::kStopping ||
             to == ScreenListener::State::kSuspended;
    case ScreenListener::State::kRunning:
      return to == ScreenListener::State::kSuspended ||
             to == ScreenListener::State::kSearching ||
             to == ScreenListener::State::kStopping;
    case ScreenListener::State::kStopping:
      return to == ScreenListener::State::kStopped;
    case ScreenListener::State::kSearching:
      return to == ScreenListener::State::kRunning ||
             to == ScreenListener::State::kSuspended ||
             to == ScreenListener::State::kStopping;
    case ScreenListener::State::kSuspended:
      return to == ScreenListener::State::kRunning ||
             to == ScreenListener::State::kSearching ||
             to == ScreenListener::State::kStopping;
    default:
      DCHECK(false) << "unknown ScreenListener::State value: "
                    << static_cast<int>(from);
      break;
  }
  return false;
}
#endif

}  // namespace

ScreenListenerImpl::Delegate::Delegate() = default;
ScreenListenerImpl::Delegate::~Delegate() = default;

void ScreenListenerImpl::Delegate::SetListenerImpl(
    ScreenListenerImpl* listener) {
  DCHECK(!listener_);
  listener_ = listener;
}

ScreenListenerImpl::ScreenListenerImpl(Observer* observer, Delegate* delegate)
    : ScreenListener(observer), delegate_(delegate) {
  delegate_->SetListenerImpl(this);
}

ScreenListenerImpl::~ScreenListenerImpl() = default;

void ScreenListenerImpl::OnScreenAdded(const ScreenInfo& info) {
  screen_list_.OnScreenAdded(info);
  if (observer_)
    observer_->OnScreenAdded(info);
}

void ScreenListenerImpl::OnScreenChanged(const ScreenInfo& info) {
  const auto any_changed = screen_list_.OnScreenChanged(info);
  if (any_changed && observer_)
    observer_->OnScreenChanged(info);
}

void ScreenListenerImpl::OnScreenRemoved(const ScreenInfo& info) {
  const auto any_removed = screen_list_.OnScreenRemoved(info);
  if (any_removed && observer_)
    observer_->OnScreenRemoved(info);
}

void ScreenListenerImpl::OnAllScreensRemoved() {
  const auto any_removed = screen_list_.OnAllScreensRemoved();
  if (any_removed && observer_)
    observer_->OnAllScreensRemoved();
}

void ScreenListenerImpl::OnError(ScreenListenerError error) {
  last_error_ = error;
  if (observer_)
    observer_->OnError(error);
}

bool ScreenListenerImpl::Start() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartListener();
  return true;
}

bool ScreenListenerImpl::StartAndSuspend() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartAndSuspendListener();
  return true;
}

bool ScreenListenerImpl::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping) {
    return false;
  }
  state_ = State::kStopping;
  delegate_->StopListener();
  return true;
}

bool ScreenListenerImpl::Suspend() {
  if (state_ != State::kRunning && state_ != State::kSearching &&
      state_ != State::kStarting) {
    return false;
  }
  delegate_->SuspendListener();
  return true;
}

bool ScreenListenerImpl::Resume() {
  if (state_ != State::kSuspended && state_ != State::kSearching) {
    return false;
  }
  delegate_->ResumeListener();
  return true;
}

bool ScreenListenerImpl::SearchNow() {
  if (state_ != State::kRunning && state_ != State::kSuspended) {
    return false;
  }
  delegate_->SearchNow(state_);
  return true;
}

const std::vector<ScreenInfo>& ScreenListenerImpl::GetScreens() const {
  return screen_list_.screens();
}

void ScreenListenerImpl::SetState(State state) {
  DCHECK(IsTransitionValid(state_, state));
  state_ = state;
  if (observer_)
    MaybeNotifyObserver();
}

void ScreenListenerImpl::MaybeNotifyObserver() {
  DCHECK(observer_);
  switch (state_) {
    case State::kRunning:
      observer_->OnStarted();
      break;
    case State::kStopped:
      observer_->OnStopped();
      break;
    case State::kSuspended:
      observer_->OnSuspended();
      break;
    case State::kSearching:
      observer_->OnSearching();
      break;
    default:
      break;
  }
}

}  // namespace openscreen
