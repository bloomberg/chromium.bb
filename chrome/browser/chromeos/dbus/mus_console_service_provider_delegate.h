// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MUS_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MUS_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/dbus/services/console_service_provider.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"

namespace chromeos {

// An implementation of ConsoleServiceProvider::Delegate that communicates with
// mus-ws via IPC.
class MusConsoleServiceProviderDelegate
    : public ConsoleServiceProvider::Delegate {
 public:
  MusConsoleServiceProviderDelegate();
  ~MusConsoleServiceProviderDelegate() override;

  // ConsoleServiceProvider::Delegate overrides:
  void TakeDisplayOwnership(const UpdateOwnershipCallback& callback) override;
  void ReleaseDisplayOwnership(
      const UpdateOwnershipCallback& callback) override;

 private:
  void ConnectDisplayControllerIfNecessary();

  // TODO(kylechar): Can we have one DisplayController for Chrome somewhere?
  display::mojom::DisplayControllerPtr display_controller_;

  DISALLOW_COPY_AND_ASSIGN(MusConsoleServiceProviderDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MUS_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_
