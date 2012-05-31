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
      pref_service_(NULL) {
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

  manager_->SetState(state_);
  manager_->AddObserver(this);
}

BrowserStateMonitor::~BrowserStateMonitor() {
  manager_->RemoveObserver(this);
}

void BrowserStateMonitor::SetPrefServiceForTesting(PrefService* pref_service) {
  pref_service_ = pref_service;
}

void BrowserStateMonitor::UpdateLocalState(
    const std::string& current_input_method) {
  if (!g_browser_process || !g_browser_process->local_state())
    return;
  g_browser_process->local_state()->SetString(
      language_prefs::kPreferredKeyboardLayout,
      current_input_method);
}

void BrowserStateMonitor::UpdateUserPreferences(
    const std::string& current_input_method) {
  PrefService* pref_service = pref_service_ ? pref_service_ : GetPrefService();
  DCHECK(pref_service);

  // Even though we're DCHECK'ing to catch this on debug builds, we don't
  // want to crash a release build in case the pref service is no longer
  // available.
  if (!pref_service)
    return;

  const std::string current_input_method_on_pref =
      pref_service->GetString(prefs::kLanguageCurrentInputMethod);
  if (current_input_method_on_pref == current_input_method)
    return;

  pref_service->SetString(prefs::kLanguagePreviousInputMethod,
                          current_input_method_on_pref);
  pref_service->SetString(prefs::kLanguageCurrentInputMethod,
                          current_input_method);
}

void BrowserStateMonitor::InputMethodChanged(InputMethodManager* manager,
                                             bool show_message) {
  DCHECK_EQ(manager_, manager);
  const std::string current_input_method =
      manager->GetCurrentInputMethod().id();
  // Save the new input method id depending on the current browser state.
  switch (state_) {
    case InputMethodManager::STATE_LOGIN_SCREEN:
      if (!InputMethodUtil::IsKeyboardLayout(current_input_method)) {
        DVLOG(1) << "Only keyboard layouts are supported: "
                 << current_input_method;
        return;
      }
      UpdateLocalState(current_input_method);
      return;
    case InputMethodManager::STATE_BROWSER_SCREEN:
      UpdateUserPreferences(current_input_method);
      return;
    case InputMethodManager::STATE_LOCK_SCREEN:
      // We use a special set of input methods on the screen. Do not update.
      return;
    case InputMethodManager::STATE_TERMINATING:
      return;
  }
  NOTREACHED();
}

void BrowserStateMonitor::InputMethodPropertyChanged(
    InputMethodManager* manager) {}

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
      // Note that the notification is also sent when Chrome crashes/restarts
      // as of writing, but it might be changed in the future (therefore we need
      // to listen to NOTIFICATION_SESSION_STARTED as well.)
      DVLOG(1) << "Received chrome::NOTIFICATION_LOGIN_USER_CHANGED";
      SetState(InputMethodManager::STATE_BROWSER_SCREEN);
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED: {
      // The user logged in, and the browser window for user session is ready.
      // An initial input method has already been set.
      // We should NOT call InitializePrefMembers() here since the notification
      // is sent in the PreProfileInit phase in case when Chrome crashes and
      // restarts.
      DVLOG(1) << "Received chrome::NOTIFICATION_SESSION_STARTED";
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
  // Note: browser notifications are sent in the following order.
  //
  // Normal login:
  // 1. chrome::NOTIFICATION_LOGIN_USER_CHANGED is sent.
  // 2. Preferences::NotifyPrefChanged() is called. preload_engines (which
  //    might change the current input method) and current/previous input method
  //    are sent to the manager.
  // 3. chrome::NOTIFICATION_SESSION_STARTED is sent.
  //
  // Chrome crash/restart (after logging in):
  // 1. chrome::NOTIFICATION_LOGIN_USER_CHANGED might be sent.
  // 2. chrome::NOTIFICATION_SESSION_STARTED is sent.
  // 3. Preferences::NotifyPrefChanged() is called. The same things as above
  //    happen.
  //
  // We have to be careful not to overwrite both local and user prefs when
  // preloaded engine is set. Note that it does not work to do nothing in
  // InputMethodChanged() between chrome::NOTIFICATION_LOGIN_USER_CHANGED and
  // chrome::NOTIFICATION_SESSION_STARTED because SESSION_STARTED is sent very
  // early on Chrome crash/restart.
}

void BrowserStateMonitor::SetState(InputMethodManager::State new_state) {
  const InputMethodManager::State old_state = state_;
  state_ = new_state;
  if (old_state != state_)
    manager_->SetState(state_);
}

}  // namespace input_method
}  // namespace chromeos
