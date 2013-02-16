// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_persistence.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

namespace chromeos {
namespace input_method {
namespace {

void PersistSystemInputMethod(const std::string& input_method) {
  if (!g_browser_process || !g_browser_process->local_state())
    return;

  g_browser_process->local_state()->SetString(
        language_prefs::kPreferredKeyboardLayout, input_method);
}

void PersistUserInputMethod(const std::string& input_method) {
  PrefService* user_prefs = NULL;
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile)
    user_prefs = profile->GetPrefs();
  if (!user_prefs)
    return;

  const std::string current_input_method_on_pref =
      user_prefs->GetString(prefs::kLanguageCurrentInputMethod);
  if (current_input_method_on_pref == input_method)
    return;

  user_prefs->SetString(prefs::kLanguagePreviousInputMethod,
                        current_input_method_on_pref);
  user_prefs->SetString(prefs::kLanguageCurrentInputMethod,
                        input_method);
}

}  // namespace

InputMethodPersistence::InputMethodPersistence(
    InputMethodManager* input_method_manager)
    : input_method_manager_(input_method_manager),
      state_(InputMethodManager::STATE_LOGIN_SCREEN) {
  input_method_manager_->AddObserver(this);
}

InputMethodPersistence::~InputMethodPersistence() {
  input_method_manager_->RemoveObserver(this);
}

void InputMethodPersistence::InputMethodChanged(
    InputMethodManager* manager, bool show_message) {
  DCHECK_EQ(input_method_manager_, manager);
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
      PersistSystemInputMethod(current_input_method);
      return;
    case InputMethodManager::STATE_BROWSER_SCREEN:
      PersistUserInputMethod(current_input_method);
      return;
    case InputMethodManager::STATE_LOCK_SCREEN:
      // We use a special set of input methods on the screen. Do not update.
      return;
    case InputMethodManager::STATE_TERMINATING:
      return;
  }
  NOTREACHED();
}

void InputMethodPersistence::InputMethodPropertyChanged(
    InputMethodManager* manager) {}

void InputMethodPersistence::OnSessionStateChange(
    InputMethodManager::State new_state) {
  state_ = new_state;
}

}  // namespace input_method
}  // namespace chromeos
