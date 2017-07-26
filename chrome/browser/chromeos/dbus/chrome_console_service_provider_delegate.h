// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CHROME_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CHROME_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_

#include "ash/public/interfaces/ash_display_controller.mojom.h"
#include "base/macros.h"
#include "chromeos/dbus/services/console_service_provider.h"

namespace service_manager {
class Connector;
}

namespace chromeos {

// Chrome's implementation of ConsoleServiceProvider::Delegate
class ChromeConsoleServiceProviderDelegate
    : public ConsoleServiceProvider::Delegate {
 public:
  ChromeConsoleServiceProviderDelegate();
  ~ChromeConsoleServiceProviderDelegate() override;

  void Connect(service_manager::Connector* connector);

  // ConsoleServiceProvider::Delegate overrides:
  void TakeDisplayOwnership(const UpdateOwnershipCallback& callback) override;
  void ReleaseDisplayOwnership(
      const UpdateOwnershipCallback& callback) override;

 private:
  ash::mojom::AshDisplayControllerPtr ash_display_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeConsoleServiceProviderDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CHROME_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_
