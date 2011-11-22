// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/accelerator.h"
#include "views/view.h"

namespace chromeos {

// static
ui::Accelerator WizardAccessibilityHelper::GetAccelerator() {
  return ui::Accelerator(ui::VKEY_Z, false, true, true);
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

WizardAccessibilityHelper::~WizardAccessibilityHelper() {}

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
                 chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
                 content::NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_ACTION,
                 content::NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 chrome::NOTIFICATION_ACCESSIBILITY_TEXT_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 chrome::NOTIFICATION_ACCESSIBILITY_MENU_OPENED,
                 content::NotificationService::AllSources());
  registrar_.Add(accessibility_handler_.get(),
                 chrome::NOTIFICATION_ACCESSIBILITY_MENU_CLOSED,
                 content::NotificationService::AllSources());
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
    // Note: queue and interruptible are no longer supported, but
    // that shouldn't matter because the whole views-based wizard
    // is obsolete and should be deleted soon.
    accessibility::Speak(str);
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
  accessibility::EnableAccessibility(enabled, NULL);
}

}  // namespace chromeos
