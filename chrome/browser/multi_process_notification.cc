// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multi_process_notification.h"

#include "base/task.h"

namespace multi_process_notification {

Listener::Delegate::~Delegate() {
}

void Listener::Delegate::OnListenerStarted(
    const std::string& name, Domain domain, bool success) {
}

Listener::ListenerStartedTask::ListenerStartedTask(const std::string& name,
    Domain domain, Listener::Delegate* delegate, bool success)
    : name_(name), domain_(domain), delegate_(delegate), success_(success) {
}

Listener::ListenerStartedTask::~ListenerStartedTask() {
}

void Listener::ListenerStartedTask::Run() {
  delegate_->OnListenerStarted(name_, domain_, success_);
}

Listener::NotificationReceivedTask::NotificationReceivedTask(
    const std::string& name, Domain domain, Listener::Delegate* delegate)
    : name_(name), domain_(domain), delegate_(delegate) {
}

Listener::NotificationReceivedTask::~NotificationReceivedTask() {
}

void Listener::NotificationReceivedTask::Run() {
  delegate_->OnNotificationReceived(name_, domain_);
}

}  // namespace multi_process_notification
