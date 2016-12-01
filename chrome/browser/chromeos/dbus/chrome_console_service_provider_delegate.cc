// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_console_service_provider_delegate.h"

#include "ash/shell.h"
#include "ui/display/manager/chromeos/display_configurator.h"

namespace chromeos {

ChromeConsoleServiceProviderDelegate::ChromeConsoleServiceProviderDelegate() {
}

ChromeConsoleServiceProviderDelegate::~ChromeConsoleServiceProviderDelegate() {
}

void ChromeConsoleServiceProviderDelegate::TakeDisplayOwnership(
    const UpdateOwnershipCallback& callback) {
  ash::Shell::GetInstance()->display_configurator()->TakeControl(callback);
}

void ChromeConsoleServiceProviderDelegate::ReleaseDisplayOwnership(
    const UpdateOwnershipCallback& callback) {
  ash::Shell::GetInstance()->display_configurator()->RelinquishControl(
      callback);
}

}  // namespace chromeos
