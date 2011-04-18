// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/accelerator.h"
#include "views/view.h"

namespace chromeos {

scoped_ptr<views::Accelerator> WizardAccessibilityHelper::accelerator_;

// static
views::Accelerator WizardAccessibilityHelper::GetAccelerator() {
  if (!WizardAccessibilityHelper::accelerator_.get())
    WizardAccessibilityHelper::accelerator_.reset(
        new views::Accelerator(ui::VKEY_Z, false, true, true));
  return *(WizardAccessibilityHelper::accelerator_.get());
}

// static
WizardAccessibilityHelper* WizardAccessibilityHelper::GetInstance() {
  return Singleton<WizardAccessibilityHelper>::get();
}

WizardAccessibilityHelper::WizardAccessibilityHelper() {
  accessibility_handler_.reset(new WizardAccessibilityHandler());
  profile_ = ProfileManager::GetDefaultProfile();
  registered_notifications_ = false;
}

void WizardAccessibilityHelper::Init() {
  if (IsAccessibilityEnabled()) {
    if (!registered_notifications_)
      RegisterNotifications();
    // SetAccessibilityEnabled(true) fully enables accessibility. Init() is
    // necessary during startup when the global accessibility pref is set but
    // the notifications are not registered.
    SetAccessibilityEnabled(true);
  }
}

void WizardAccessibilityHelper::RegisterNotifications() {
  registrar_.Add(accessibility_handler_.get(),
                 NotificationType::ACCESSIBILITY_CONTROL_FOCUSED,
                 NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 NotificationType::ACCESSIBILITY_CONTROL_ACTION,
                 NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 NotificationType::ACCESSIBILITY_TEXT_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 NotificationType::ACCESSIBILITY_MENU_OPENED,
                 NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 NotificationType::ACCESSIBILITY_MENU_CLOSED,
                 NotificationService::AllSources());
  registered_notifications_ = true;
}

void WizardAccessibilityHelper::UnregisterNotifications() {
  if (!registered_notifications_)
    return;
  registrar_.RemoveAll();
  registered_notifications_ = false;
}

bool WizardAccessibilityHelper::IsAccessibilityEnabled() {
  return g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
      prefs::kAccessibilityEnabled);
}

void WizardAccessibilityHelper::MaybeSpeak(const char* str, bool queue,
    bool interruptible) {
  if (IsAccessibilityEnabled()) {
    accessibility_handler_->Speak(str, queue, interruptible);
  }
}

void WizardAccessibilityHelper::ToggleAccessibility() {
  if (!IsAccessibilityEnabled()) {
    VLOG(1) << "Enabling accessibility.";
    if (!registered_notifications_)
      RegisterNotifications();
    SetAccessibilityEnabled(true);
  } else {
    SetAccessibilityEnabled(false);
    if (registered_notifications_)
      UnregisterNotifications();
  }
}

void WizardAccessibilityHelper::SetAccessibilityEnabled(bool enabled) {
  bool doSpeak = (IsAccessibilityEnabled() != enabled);
  if (g_browser_process) {
    PrefService* prefService = g_browser_process->local_state();
    prefService->SetBoolean(prefs::kAccessibilityEnabled, enabled);
    prefService->ScheduleSavePersistentPrefs();
  }
  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(enabled);
  if (doSpeak) {
    accessibility_handler_->Speak(enabled ?
        l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_ACCESS_ENABLED).c_str() :
        l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_ACCESS_DISABLED).c_str(),
        false, true);
  }
}

}  // namespace chromeos
