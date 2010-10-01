// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "views/accelerator.h"
#include "views/view.h"

namespace chromeos {

scoped_ptr<views::Accelerator> WizardAccessibilityHelper::accelerator_;

// static
views::Accelerator WizardAccessibilityHelper::GetAccelerator() {
  if (!WizardAccessibilityHelper::accelerator_.get())
    WizardAccessibilityHelper::accelerator_.reset(
        new views::Accelerator(app::VKEY_Z, false, true, true));
  return *(WizardAccessibilityHelper::accelerator_.get());
}

// static
WizardAccessibilityHelper* WizardAccessibilityHelper::GetInstance() {
  return Singleton<WizardAccessibilityHelper>::get();
}

WizardAccessibilityHelper::WizardAccessibilityHelper() {
  accessibility_handler_.reset(new WizardAccessibilityHandler());
  profile_ = ProfileManager::GetDefaultProfile();

  // http://crosbug.com/7238
  // TODO(dmazzoni), TODO(chaitanyag)
  // Commented out to disable trapping handling and responding to
  // accessibility notifications until the root cause of this bug has
  // been determined.
  //
  //registrar_.Add(accessibility_handler_.get(),
  //               NotificationType::ACCESSIBILITY_CONTROL_FOCUSED,
  //               NotificationService::AllSources());
  //registrar_.Add(accessibility_handler_.get(),
  //               NotificationType::ACCESSIBILITY_CONTROL_ACTION,
  //               NotificationService::AllSources());
  //registrar_.Add(accessibility_handler_.get(),
  //               NotificationType::ACCESSIBILITY_TEXT_CHANGED,
  //               NotificationService::AllSources());
  //registrar_.Add(accessibility_handler_.get(),
  //               NotificationType::ACCESSIBILITY_MENU_OPENED,
  //               NotificationService::AllSources());
  //registrar_.Add(accessibility_handler_.get(),
  //               NotificationType::ACCESSIBILITY_MENU_CLOSED,
  //               NotificationService::AllSources());
}

void WizardAccessibilityHelper::MaybeEnableAccessibility(
    views::View* view_tree) {
  if (g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAccessibilityEnabled)) {
      EnableAccessibility(view_tree);
  } else {
    AddViewToBuffer(view_tree);
  }
}

void WizardAccessibilityHelper::MaybeSpeak(const char* str, bool queue,
    bool interruptible) {
  if (g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAccessibilityEnabled)) {
    accessibility_handler_->Speak(str, queue, interruptible);
  }
}

void WizardAccessibilityHelper::EnableAccessibility(views::View* view_tree) {
  LOG(INFO) << "Enabling accessibility.";
  if (g_browser_process) {
    PrefService* prefService = g_browser_process->local_state();
    if (!prefService->GetBoolean(prefs::kAccessibilityEnabled)) {
      prefService->SetBoolean(prefs::kAccessibilityEnabled, true);
      prefService->ScheduleSavePersistentPrefs();
    }
  }
  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(true);
  AddViewToBuffer(view_tree);
  // If accessibility pref is set, enable accessibility for all views in
  // the buffer for which access is not yet enabled.
  for (std::map<views::View*, bool>::iterator iter =
      views_buffer_.begin();
      iter != views_buffer_.end(); ++iter) {
    if (!(*iter).second) {
      AccessibleViewHelper *helper = new AccessibleViewHelper((*iter).first,
          profile_);
      accessible_view_helpers_.push_back(helper);
      (*iter).second = true;
    }
  }
}

void WizardAccessibilityHelper::AddViewToBuffer(views::View* view_tree) {
  if (!view_tree->GetWidget())
    return;
  bool view_exists = false;
  // Check if the view is already queued for enabling accessibility.
  // Prevent adding the same view in the buffer twice.
  for (std::map<views::View*, bool>::iterator iter = views_buffer_.begin();
      iter != views_buffer_.end(); ++iter) {
    if ((*iter).first == view_tree) {
      view_exists = true;
      break;
    }
  }
  if (!view_exists)
    views_buffer_[view_tree] = false;
}

}  // namespace chromeos
