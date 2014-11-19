// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CHROME_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CHROME_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_

#include "chromeos/dbus/services/console_service_provider.h"

namespace chromeos {

// Chrome's implementation of ConsoleServiceProvider::Delegate
class ChromeConsoleServiceProviderDelegate
    : public ConsoleServiceProvider::Delegate {
 public:
  ChromeConsoleServiceProviderDelegate();
  ~ChromeConsoleServiceProviderDelegate() override;

  // ConsoleServiceProvider::Delegate overrides:
  void TakeDisplayOwnership() override;
  void ReleaseDisplayOwnership() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeConsoleServiceProviderDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CHROME_CONSOLE_SERVICE_PROVIDER_DELEGATE_H_
