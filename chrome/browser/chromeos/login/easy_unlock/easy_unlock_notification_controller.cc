// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller_chromeos.h"

namespace chromeos {

std::unique_ptr<EasyUnlockNotificationController>
EasyUnlockNotificationController::Create(Profile* profile) {
  return std::make_unique<EasyUnlockNotificationControllerChromeOS>(profile);
}

}  // namespace chromeos
