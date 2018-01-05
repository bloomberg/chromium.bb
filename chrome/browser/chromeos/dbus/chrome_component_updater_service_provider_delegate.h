// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CHROME_COMPONENT_UPDATER_SERVICE_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CHROME_COMPONENT_UPDATER_SERVICE_PROVIDER_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/dbus/services/component_updater_service_provider.h"

namespace chromeos {

// Chrome's implementation of ChromeComponentUpdaterServiceProvider::Delegate.
class ChromeComponentUpdaterServiceProviderDelegate
    : public ComponentUpdaterServiceProvider::Delegate {
 public:
  ChromeComponentUpdaterServiceProviderDelegate();
  ~ChromeComponentUpdaterServiceProviderDelegate() override;

  // ComponentUpdaterServiceProvider::Delegate:
  void LoadComponent(
      const std::string& name,
      base::OnceCallback<void(const base::FilePath&)> load_callback) override;
  bool UnloadComponent(const std::string& name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeComponentUpdaterServiceProviderDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CHROME_COMPONENT_UPDATER_SERVICE_PROVIDER_DELEGATE_H_
