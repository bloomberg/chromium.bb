// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"

// static
WizardAccessibilityHelper* WizardAccessibilityHelper::GetInstance() {
  return Singleton<WizardAccessibilityHelper>::get();
}

WizardAccessibilityHelper::WizardAccessibilityHelper() {
  accessibility_handler_.reset(new WizardAccessibilityHandler());
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
}

void WizardAccessibilityHelper::MaybeEnableAccessibility(
    views::View* view_tree, Profile* profile) {
  if (g_browser_process != NULL &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAccessibilityEnabled)) {
      EnableAccessibility(view_tree, profile);
  }
}

void WizardAccessibilityHelper::EnableAccessibility(
    views::View* view_tree, Profile* profile) {
  LOG(INFO) << "Enabling accessibility.";
  if (g_browser_process != NULL) {
    PrefService* prefService = g_browser_process->local_state();
    if (!prefService->GetBoolean(prefs::kAccessibilityEnabled)) {
      prefService->SetBoolean(prefs::kAccessibilityEnabled, true);
      prefService->ScheduleSavePersistentPrefs();
    }
  }
  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(true);
  accessible_view_helper_.reset(new AccessibleViewHelper(
      view_tree, profile));
}
