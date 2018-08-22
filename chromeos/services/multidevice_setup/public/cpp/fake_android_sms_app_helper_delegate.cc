// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"

#include "url/gurl.h"

namespace chromeos {

namespace multidevice_setup {

FakeAndroidSmsAppHelperDelegate::FakeAndroidSmsAppHelperDelegate() {}

FakeAndroidSmsAppHelperDelegate::~FakeAndroidSmsAppHelperDelegate() = default;

void FakeAndroidSmsAppHelperDelegate::InstallAndroidSmsApp() {
  // TODO(jlklein): Add error simulation when error codes are added to the API.
  has_installed_ = true;
}

bool FakeAndroidSmsAppHelperDelegate::HasInstalledApp() {
  return has_installed_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
