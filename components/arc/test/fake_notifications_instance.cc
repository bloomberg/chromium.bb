// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_notifications_instance.h"

namespace arc {

FakeNotificationsInstance::FakeNotificationsInstance(
    mojo::InterfaceRequest<NotificationsInstance> request)
    : binding_(this, std::move(request)) {}

FakeNotificationsInstance::~FakeNotificationsInstance() {}

void FakeNotificationsInstance::SendNotificationEventToAndroid(
    const mojo::String& key,
    ArcNotificationEvent event) {
  events_.emplace_back(key, event);
}

void FakeNotificationsInstance::Init(NotificationsHostPtr host_ptr) {}

const std::vector<std::pair<mojo::String, ArcNotificationEvent>>&
FakeNotificationsInstance::events() const {
  return events_;
}

void FakeNotificationsInstance::WaitForIncomingMethodCall() {
  binding_.WaitForIncomingMethodCall();
}

}  // namespace arc
