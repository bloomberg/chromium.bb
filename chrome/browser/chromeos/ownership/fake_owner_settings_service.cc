// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"

namespace chromeos {

FakeOwnerSettingsService::FakeOwnerSettingsService(
    Profile* profile,
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util)
    : OwnerSettingsServiceChromeOS(nullptr, profile, owner_key_util),
      set_management_settings_result_(true) {
}

FakeOwnerSettingsService::~FakeOwnerSettingsService() {
}

void FakeOwnerSettingsService::SetManagementSettings(
    const ManagementSettings& settings,
    const OnManagementSettingsSetCallback& callback) {
  last_settings_ = settings;
  callback.Run(set_management_settings_result_);
}

}  // namespace chromeos
