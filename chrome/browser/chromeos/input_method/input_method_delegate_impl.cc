// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_delegate_impl.h"

#include "base/prefs/public/pref_service_base.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

namespace chromeos {
namespace input_method {

InputMethodDelegateImpl::InputMethodDelegateImpl() {}

void InputMethodDelegateImpl::SetSystemInputMethod(
    const std::string& input_method) {
  if (g_browser_process) {
    PrefServiceBase* local_state = g_browser_process->local_state();
    if (local_state) {
      local_state->SetString(language_prefs::kPreferredKeyboardLayout,
                             input_method);
      return;
    }
  }

  NOTREACHED();
}

void InputMethodDelegateImpl::SetUserInputMethod(
    const std::string& input_method) {
  PrefServiceBase* user_prefs = NULL;
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

std::string InputMethodDelegateImpl::GetHardwareKeyboardLayout() const {
  if (g_browser_process) {
    PrefServiceBase* local_state = g_browser_process->local_state();
    if (local_state)
      return local_state->GetString(prefs::kHardwareKeyboardLayout);
  }
  // This shouldn't happen but just in case.
  DVLOG(1) << "Local state is not yet ready.";
  return std::string();
}

std::string InputMethodDelegateImpl::GetActiveLocale() const {
  if (g_browser_process)
    return g_browser_process->GetApplicationLocale();

  NOTREACHED();
  return std::string();
}

}  // namespace input_method
}  // namespace chromeos
