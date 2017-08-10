// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include <memory>

#include "ash/accessibility_types.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace ash {
class SystemTrayNotifier;
}

namespace chromeos {

// DEPRECATED. Do not add new code here. This class is being removed as part of
// the transition to mustash. New code should be added to SystemTrayClient.
// Use system_tray.mojom methods if you need to send information to ash.
// Please contact jamescook@chromium.org if you have questions or need help.
class SystemTrayDelegateChromeOS : public ash::SystemTrayDelegate,
                                   public content::NotificationObserver {
 public:
  SystemTrayDelegateChromeOS();
  ~SystemTrayDelegateChromeOS() override;

  // Overridden from ash::SystemTrayDelegate:
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;
  void ActiveUserWasChanged() override;

 private:
  ash::SystemTrayNotifier* GetSystemTrayNotifier();

  void SetProfile(Profile* profile);

  bool UnsetProfile(Profile* profile);

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnAccessibilityModeChanged(
      ash::AccessibilityNotificationVisibility notify);

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  std::unique_ptr<content::NotificationRegistrar> registrar_;
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;
  Profile* user_profile_ = nullptr;

  std::unique_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;
  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
