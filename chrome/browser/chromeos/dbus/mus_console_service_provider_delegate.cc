// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/mus_console_service_provider_delegate.h"

#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

MusConsoleServiceProviderDelegate::MusConsoleServiceProviderDelegate() {}

MusConsoleServiceProviderDelegate::~MusConsoleServiceProviderDelegate() {}

void MusConsoleServiceProviderDelegate::TakeDisplayOwnership(
    const UpdateOwnershipCallback& callback) {
  ConnectDisplayControllerIfNecessary();
  display_controller_->TakeDisplayControl(callback);
}

void MusConsoleServiceProviderDelegate::ReleaseDisplayOwnership(
    const UpdateOwnershipCallback& callback) {
  ConnectDisplayControllerIfNecessary();
  display_controller_->RelinquishDisplayControl(callback);
}

void MusConsoleServiceProviderDelegate::ConnectDisplayControllerIfNecessary() {
  if (display_controller_)
    return;

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface("ui", &display_controller_);
}

}  // namespace chromeos
