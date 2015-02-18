// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_

#include <string>

#include "ash/system/tray/system_tray_delegate.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// Common base class for platform-specific implementations.
class SystemTrayDelegateCommon : public ash::SystemTrayDelegate,
                                 public content::NotificationObserver {
 public:
  SystemTrayDelegateCommon();

  ~SystemTrayDelegateCommon() override;

  // Overridden from ash::SystemTrayDelegate:
  void Initialize() override;
  void Shutdown() override;
  bool GetTrayVisibilityOnStartup() override;
  ash::user::LoginStatus GetUserLoginStatus() const override;
  bool IsUserSupervised() const override;
  bool IsUserChild() const override;
  void GetSystemUpdateInfo(ash::UpdateInfo* info) const override;
  base::HourClockType GetHourClockType() const override;
  void ShowChromeSlow() override;
  void ShowHelp() override;
  void RequestRestartForUpdate() override;
  int GetSystemTrayMenuWidth() override;

 private:
  void UpdateClockType();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  scoped_ptr<content::NotificationRegistrar> registrar_;
  base::HourClockType clock_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateCommon);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_COMMON_H_
