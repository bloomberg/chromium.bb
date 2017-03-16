// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_notification_presenter.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/message_center/message_center.h"

namespace chromeos {

namespace tether {

FakeNotificationPresenter::FakeNotificationPresenter()
    : NotificationPresenter(nullptr) {}

FakeNotificationPresenter::~FakeNotificationPresenter() {}

void FakeNotificationPresenter::NotifyConnectionToHostFailed(
    const std::string& host_device_name) {
  connection_failed_notification_device_name_ = host_device_name;
}

void FakeNotificationPresenter::RemoveConnectionToHostFailedNotification() {
  connection_failed_notification_device_name_ = "";
}

}  // namespace tether

}  // namespace chromeos
