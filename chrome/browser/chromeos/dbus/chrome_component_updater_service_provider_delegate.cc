// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_component_updater_service_provider_delegate.h"

#include "chrome/browser/component_updater/cros_component_installer.h"

namespace chromeos {

ChromeComponentUpdaterServiceProviderDelegate::
    ChromeComponentUpdaterServiceProviderDelegate() {}

ChromeComponentUpdaterServiceProviderDelegate::
    ~ChromeComponentUpdaterServiceProviderDelegate() {}

void ChromeComponentUpdaterServiceProviderDelegate::LoadComponent(
    const std::string& name,
    const base::Callback<void(const std::string&)>& load_callback) {
  component_updater::CrOSComponent::LoadComponent(name, load_callback);
}

}  // namespace chromeos
