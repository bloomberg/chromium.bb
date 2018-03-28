// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_console_service_provider_delegate.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/manager/chromeos/display_configurator.h"

namespace chromeos {

ChromeConsoleServiceProviderDelegate::ChromeConsoleServiceProviderDelegate() {}

ChromeConsoleServiceProviderDelegate::~ChromeConsoleServiceProviderDelegate() {}

void ChromeConsoleServiceProviderDelegate::Connect(
    service_manager::Connector* connector) {
  connector->BindInterface(ash::mojom::kServiceName, &ash_display_controller_);
}

void ChromeConsoleServiceProviderDelegate::TakeDisplayOwnership(
    const UpdateOwnershipCallback& callback) {
  if (!ash_display_controller_) {
    callback.Run(false);
    return;
  }
  ash_display_controller_->TakeDisplayControl(callback);
}

void ChromeConsoleServiceProviderDelegate::ReleaseDisplayOwnership(
    const UpdateOwnershipCallback& callback) {
  if (!ash_display_controller_) {
    callback.Run(false);
    return;
  }
  ash_display_controller_->RelinquishDisplayControl(callback);
}

}  // namespace chromeos
