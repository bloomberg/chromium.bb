// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace input_method {
namespace {

PrefService* GetPrefService() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile)
    return profile->GetPrefs();
  return NULL;
}

}  // namespace

BrowserStateMonitor::BrowserStateMonitor(InputMethodManager* manager)
    : manager_(manager),
      state_(InputMethodManager::STATE_LOGIN_SCREEN),
      initialized_(false) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_SESSION_STARTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                              content::NotificationService::AllSources());
  // We should not use APP_EXITING here since logout might be canceled by
  // JavaScript after APP_EXITING is sent (crosbug.com/11055).
  notification_registrar_.Add(this,
                              content::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  // TODO(yusukes): Tell the initial state to the manager.
  manager_->AddObserver(this);
}

BrowserStateMonitor::~BrowserStateMonitor() {
  manager_->RemoveObserver(this);
}

void BrowserStateMonitor::UpdateLocalState(
    const std::string& current_input_method) {
  if (!InputMethodUtil::IsKeyboardLayout(current_input_method)) {
    LOG(ERROR) << "Only keyboard layouts are supported: "
               << current_input_method;
    return;
  }

  if (!g_browser_process || !g_browser_process->local_state())
    return;

  g_browser_process->local_state()->SetString(
      language_prefs::kPreferredKeyboardLayout,
      current_input_method);
}

void BrowserStateMonitor::UpdateUserPreferences(
    const std::string& current_input_method) {
  InitializePrefMembers();
  const std::string current_input_method_on_pref =
      current_input_method_pref_.GetValue();
  if (current_input_method_on_pref == current_input_method)
    return;
  previous_input_method_pref_.SetValue(current_input_method_on_pref);
  current_input_method_pref_.SetValue(current_input_method);
}

void BrowserStateMonitor::InputMethodChanged(
    InputMethodManager* manager,
    const InputMethodDescriptor& current_input_method,
    size_t num_active_input_methods) {
  DCHECK_EQ(manager_, manager);
  // Save the new input method id depending on the current browser state.
  switch (state_) {
    case InputMethodManager::STATE_LOGIN_SCREEN:
      UpdateLocalState(current_input_method.id());
      return;
    case InputMethodManager::STATE_BROWSER_SCREEN:
      UpdateUserPreferences(current_input_method.id());
      return;
    case InputMethodManager::STATE_LOGGING_IN:
      // Do not update the prefs since Preferences::NotifyPrefChanged() will
      // notify the current/previous input method IDs to the manager.
      return;
    case InputMethodManager::STATE_LOCK_SCREEN:
      // We use a special set of input methods on the screen. Do not update.
      return;
    case InputMethodManager::STATE_TERMINATING:
      return;
  }
  NOTREACHED();
}

void BrowserStateMonitor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_APP_TERMINATING: {
      SetState(InputMethodManager::STATE_TERMINATING);
      break;
    }
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      // The user logged in, but the browser window for user session is not yet
      // ready. An initial input method hasn't been set to the manager.
      // Note that the notification is also sent when Chrome crashes/restarts.
      SetState(InputMethodManager::STATE_LOGGING_IN);
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED: {
      // The user logged in, and the browser window for user session is ready.
      // An initial input method has already been set.
      // We should NOT call InitializePrefMembers() here since the notification
      // is sent in the PreProfileInit phase in case when Chrome crashes and
      // restarts.
      SetState(InputMethodManager::STATE_BROWSER_SCREEN);
      break;
    }
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED: {
      const bool is_screen_locked = *content::Details<bool>(details).ptr();
      SetState(is_screen_locked ? InputMethodManager::STATE_LOCK_SCREEN :
               InputMethodManager::STATE_BROWSER_SCREEN);
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      break;  // just ignore the notification.
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  // TODO(yusukes): On R20, we should handle Chrome crash/restart correctly.
  // Currently, a wrong input method could be selected when Chrome crashes and
  // restarts.
  //
  // Normal login sequence:
  // 1. chrome::NOTIFICATION_LOGIN_USER_CHANGED is sent.
  // 2. Preferences::NotifyPrefChanged() is called. preload_engines (which
  //    might change the current input method) and current/previous input method
  //    are sent to the manager.
  // 3. chrome::NOTIFICATION_SESSION_STARTED is sent.
  //
  // Chrome crash/restart (after logging in) sequence:
  // 1. chrome::NOTIFICATION_LOGIN_USER_CHANGED is sent.
  // 2. chrome::NOTIFICATION_SESSION_STARTED is sent.
  // 3. Preferences::NotifyPrefChanged() is called. preload_engines (which
  //    might change the current input method) and current/previous input method
  //    are sent to the manager. When preload_engines are sent to the manager,
  //    UpdateUserPreferences() might be accidentally called.
}

void BrowserStateMonitor::SetState(InputMethodManager::State new_state) {
  const InputMethodManager::State old_state = state_;
  state_ = new_state;
  if (old_state != state_) {
    // TODO(yusukes): Tell the new state to the manager.
  }
}

void BrowserStateMonitor::InitializePrefMembers() {
  if (initialized_)
    return;

  initialized_ = true;
  PrefService* pref_service = GetPrefService();
  DCHECK(pref_service);
  DCHECK_EQ(InputMethodManager::STATE_BROWSER_SCREEN, state_);
  previous_input_method_pref_.Init(
      prefs::kLanguagePreviousInputMethod, pref_service, this);
  current_input_method_pref_.Init(
      prefs::kLanguageCurrentInputMethod, pref_service, this);
}

}  // namespace input_method
}  // namespace chromeos
