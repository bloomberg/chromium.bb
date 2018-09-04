// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_listener_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if DCHECK_IS_ON()
bool IsTransitionValid(ScreenListenerState from, ScreenListenerState to) {
  switch (from) {
    case ScreenListenerState::kStopped:
      return to == ScreenListenerState::kStarting ||
             to == ScreenListenerState::kStopping;
    case ScreenListenerState::kStarting:
      return to == ScreenListenerState::kRunning ||
             to == ScreenListenerState::kStopping ||
             to == ScreenListenerState::kSuspended;
    case ScreenListenerState::kRunning:
      return to == ScreenListenerState::kSuspended ||
             to == ScreenListenerState::kSearching ||
             to == ScreenListenerState::kStopping;
    case ScreenListenerState::kStopping:
      return to == ScreenListenerState::kStopped;
    case ScreenListenerState::kSearching:
      return to == ScreenListenerState::kRunning ||
             to == ScreenListenerState::kSuspended ||
             to == ScreenListenerState::kStopping;
    case ScreenListenerState::kSuspended:
      return to == ScreenListenerState::kRunning ||
             to == ScreenListenerState::kSearching ||
             to == ScreenListenerState::kStopping;
    default:
      DCHECK(false) << "unknown ScreenListenerState value: "
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

ScreenListenerImpl::ScreenListenerImpl(ScreenListenerObserver* observer,
                                       Delegate* delegate)
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

void ScreenListenerImpl::OnError(ScreenListenerErrorInfo error) {
  last_error_ = error;
  if (observer_)
    observer_->OnError(error);
}

bool ScreenListenerImpl::Start() {
  if (state_ != ScreenListenerState::kStopped)
    return false;
  state_ = ScreenListenerState::kStarting;
  delegate_->StartListener();
  return true;
}

bool ScreenListenerImpl::StartAndSuspend() {
  if (state_ != ScreenListenerState::kStopped)
    return false;
  state_ = ScreenListenerState::kStarting;
  delegate_->StartAndSuspendListener();
  return true;
}

bool ScreenListenerImpl::Stop() {
  if (state_ == ScreenListenerState::kStopped ||
      state_ == ScreenListenerState::kStopping) {
    return false;
  }
  state_ = ScreenListenerState::kStopping;
  delegate_->StopListener();
  return true;
}

bool ScreenListenerImpl::Suspend() {
  if (state_ != ScreenListenerState::kRunning &&
      state_ != ScreenListenerState::kSearching &&
      state_ != ScreenListenerState::kStarting) {
    return false;
  }
  delegate_->SuspendListener();
  return true;
}

bool ScreenListenerImpl::Resume() {
  if (state_ != ScreenListenerState::kSuspended &&
      state_ != ScreenListenerState::kSearching) {
    return false;
  }
  delegate_->ResumeListener();
  return true;
}

bool ScreenListenerImpl::SearchNow() {
  if (state_ != ScreenListenerState::kRunning &&
      state_ != ScreenListenerState::kSuspended) {
    return false;
  }
  delegate_->SearchNow(state_);
  return true;
}

const std::vector<ScreenInfo>& ScreenListenerImpl::GetScreens() const {
  return screen_list_.screens();
}

void ScreenListenerImpl::SetState(ScreenListenerState state) {
  DCHECK(IsTransitionValid(state_, state));
  const auto from = state_;
  state_ = state;
  if (observer_)
    MaybeNotifyObserver(from);
}

void ScreenListenerImpl::MaybeNotifyObserver(ScreenListenerState from) {
  DCHECK(observer_);
  switch (state_) {
    case ScreenListenerState::kRunning:
      if (from == ScreenListenerState::kStarting ||
          from == ScreenListenerState::kSuspended ||
          from == ScreenListenerState::kSearching) {
        observer_->OnStarted();
      }
      break;
    case ScreenListenerState::kStopped:
      observer_->OnStopped();
      break;
    case ScreenListenerState::kSuspended:
      if (from == ScreenListenerState::kRunning ||
          from == ScreenListenerState::kSearching) {
        observer_->OnSuspended();
      }
      break;
    case ScreenListenerState::kSearching:
      observer_->OnSearching();
      break;
    default:
      break;
  }
}

}  // namespace openscreen
