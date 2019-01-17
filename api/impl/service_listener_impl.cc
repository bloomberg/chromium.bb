// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/service_listener_impl.h"

#include "base/error.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace {

bool IsTransitionValid(ServiceListener::State from, ServiceListener::State to) {
  switch (from) {
    case ServiceListener::State::kStopped:
      return to == ServiceListener::State::kStarting ||
             to == ServiceListener::State::kStopping;
    case ServiceListener::State::kStarting:
      return to == ServiceListener::State::kRunning ||
             to == ServiceListener::State::kStopping ||
             to == ServiceListener::State::kSuspended;
    case ServiceListener::State::kRunning:
      return to == ServiceListener::State::kSuspended ||
             to == ServiceListener::State::kSearching ||
             to == ServiceListener::State::kStopping;
    case ServiceListener::State::kStopping:
      return to == ServiceListener::State::kStopped;
    case ServiceListener::State::kSearching:
      return to == ServiceListener::State::kRunning ||
             to == ServiceListener::State::kSuspended ||
             to == ServiceListener::State::kStopping;
    case ServiceListener::State::kSuspended:
      return to == ServiceListener::State::kRunning ||
             to == ServiceListener::State::kSearching ||
             to == ServiceListener::State::kStopping;
    default:
      OSP_DCHECK(false) << "unknown ServiceListener::State value: "
                        << static_cast<int>(from);
      break;
  }
  return false;
}

}  // namespace

ServiceListenerImpl::Delegate::Delegate() = default;
ServiceListenerImpl::Delegate::~Delegate() = default;

void ServiceListenerImpl::Delegate::SetListenerImpl(
    ServiceListenerImpl* listener) {
  OSP_DCHECK(!listener_);
  listener_ = listener;
}

ServiceListenerImpl::ServiceListenerImpl(Observer* observer, Delegate* delegate)
    : ServiceListener(observer), delegate_(delegate) {
  delegate_->SetListenerImpl(this);
}

ServiceListenerImpl::~ServiceListenerImpl() = default;

void ServiceListenerImpl::OnReceiverAdded(const ServiceInfo& info) {
  receiver_list_.OnReceiverAdded(info);
  if (observer_)
    observer_->OnReceiverAdded(info);
}

void ServiceListenerImpl::OnReceiverChanged(const ServiceInfo& info) {
  const Error changed_error = receiver_list_.OnReceiverChanged(info);
  if (changed_error.ok() && observer_)
    observer_->OnReceiverChanged(info);
}

void ServiceListenerImpl::OnReceiverRemoved(const ServiceInfo& info) {
  const Error removed_error = receiver_list_.OnReceiverRemoved(info);
  if (removed_error.ok() && observer_)
    observer_->OnReceiverRemoved(info);
}

void ServiceListenerImpl::OnAllReceiversRemoved() {
  const Error removed_all_error = receiver_list_.OnAllReceiversRemoved();
  if (removed_all_error.ok() && observer_)
    observer_->OnAllReceiversRemoved();
}

void ServiceListenerImpl::OnError(ServiceListenerError error) {
  last_error_ = error;
  if (observer_)
    observer_->OnError(error);
}

bool ServiceListenerImpl::Start() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartListener();
  return true;
}

bool ServiceListenerImpl::StartAndSuspend() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartAndSuspendListener();
  return true;
}

bool ServiceListenerImpl::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping)
    return false;

  state_ = State::kStopping;
  delegate_->StopListener();
  return true;
}

bool ServiceListenerImpl::Suspend() {
  if (state_ != State::kRunning && state_ != State::kSearching &&
      state_ != State::kStarting) {
    return false;
  }
  delegate_->SuspendListener();
  return true;
}

bool ServiceListenerImpl::Resume() {
  if (state_ != State::kSuspended && state_ != State::kSearching)
    return false;

  delegate_->ResumeListener();
  return true;
}

bool ServiceListenerImpl::SearchNow() {
  if (state_ != State::kRunning && state_ != State::kSuspended)
    return false;

  delegate_->SearchNow(state_);
  return true;
}

const std::vector<ServiceInfo>& ServiceListenerImpl::GetReceivers() const {
  return receiver_list_.receivers();
}

void ServiceListenerImpl::SetState(State state) {
  OSP_DCHECK(IsTransitionValid(state_, state));
  state_ = state;
  if (observer_)
    MaybeNotifyObserver();
}

void ServiceListenerImpl::MaybeNotifyObserver() {
  OSP_DCHECK(observer_);
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
