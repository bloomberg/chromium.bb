// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/fake_android_sms_app_manager.h"

#include "base/macros.h"

namespace chromeos {

namespace android_sms {

FakeAndroidSmsAppManager::FakeAndroidSmsAppManager() = default;

FakeAndroidSmsAppManager::~FakeAndroidSmsAppManager() = default;

void FakeAndroidSmsAppManager::SetInstalledAppUrl(
    const base::Optional<GURL>& url) {
  if (url == url_)
    return;

  url_ = url;
  NotifyInstalledAppUrlChanged();
}

base::Optional<GURL> FakeAndroidSmsAppManager::GetCurrentAppUrl() {
  return url_;
}

}  // namespace android_sms

}  // namespace chromeos
