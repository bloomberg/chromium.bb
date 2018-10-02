// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_publisher_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if DCHECK_IS_ON()
bool IsTransitionValid(ScreenPublisher::State from, ScreenPublisher::State to) {
  using State = ScreenPublisher::State;
  switch (from) {
    case State::kStopped:
      return to == State::kStarting || to == State::kStopping;
    case State::kStarting:
      return to == State::kRunning || to == State::kStopping ||
             to == State::kSuspended;
    case State::kRunning:
      return to == State::kSuspended || to == State::kStopping;
    case State::kStopping:
      return to == State::kStopped;
    case State::kSuspended:
      return to == State::kRunning || to == State::kStopping;
    default:
      DCHECK(false) << "unknown State value: " << static_cast<int>(from);
      break;
  }
  return false;
}
#endif

}  // namespace

ScreenPublisherImpl::Delegate::Delegate() = default;
ScreenPublisherImpl::Delegate::~Delegate() = default;

void ScreenPublisherImpl::Delegate::SetPublisherImpl(
    ScreenPublisherImpl* publisher) {
  DCHECK(!publisher_);
  publisher_ = publisher;
}

ScreenPublisherImpl::ScreenPublisherImpl(Observer* observer, Delegate* delegate)
    : ScreenPublisher(observer), delegate_(delegate) {
  delegate_->SetPublisherImpl(this);
}

ScreenPublisherImpl::~ScreenPublisherImpl() = default;

bool ScreenPublisherImpl::Start() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartPublisher();
  return true;
}
bool ScreenPublisherImpl::StartAndSuspend() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartAndSuspendPublisher();
  return true;
}
bool ScreenPublisherImpl::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping)
    return false;

  state_ = State::kStopping;
  delegate_->StopPublisher();
  return true;
}
bool ScreenPublisherImpl::Suspend() {
  if (state_ != State::kRunning && state_ != State::kStarting)
    return false;

  delegate_->SuspendPublisher();
  return true;
}
bool ScreenPublisherImpl::Resume() {
  if (state_ != State::kSuspended)
    return false;

  delegate_->ResumePublisher();
  return true;
}
void ScreenPublisherImpl::UpdateFriendlyName(const std::string& friendly_name) {
  delegate_->UpdateFriendlyName(friendly_name);
}

void ScreenPublisherImpl::SetState(State state) {
  DCHECK(IsTransitionValid(state_, state));
  state_ = state;
  if (observer_)
    MaybeNotifyObserver();
}

void ScreenPublisherImpl::MaybeNotifyObserver() {
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
    default:
      break;
  }
}

}  // namespace openscreen
