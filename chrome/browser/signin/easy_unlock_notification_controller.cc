// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_notification_controller.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/easy_unlock_notification_controller_chromeos.h"
#endif

namespace {

// Stub implementation of EasyUnlockNotificationController for non-ChromeOS
// platforms.
class EasyUnlockNotificationControllerStub
    : public EasyUnlockNotificationController {
 public:
  EasyUnlockNotificationControllerStub() {}
  ~EasyUnlockNotificationControllerStub() override {}

  // EasyUnlockNotificationController:
  void ShowChromebookAddedNotification() override {}
  void ShowPairingChangeNotification() override {}
  void ShowPairingChangeAppliedNotification(
      const std::string& phone_name) override {}
  void ShowPromotionNotification() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockNotificationControllerStub);
};

}  // namespace

std::unique_ptr<EasyUnlockNotificationController>
EasyUnlockNotificationController::Create(Profile* profile) {
#if defined(OS_CHROMEOS)
  return base::MakeUnique<EasyUnlockNotificationControllerChromeOS>(profile);
#else
  return base::MakeUnique<EasyUnlockNotificationControllerStub>();
#endif
}
