// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/web_app/mock_web_kiosk_app_launcher.h>

namespace chromeos {

MockWebKioskAppLauncher::MockWebKioskAppLauncher()
    : WebKioskAppLauncher(nullptr, nullptr) {}

MockWebKioskAppLauncher::~MockWebKioskAppLauncher() = default;

}  // namespace chromeos
