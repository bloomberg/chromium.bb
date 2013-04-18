// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_delegate_impl.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace input_method {

InputMethodDelegateImpl::InputMethodDelegateImpl() {}

std::string InputMethodDelegateImpl::GetHardwareKeyboardLayout() const {
  if (g_browser_process) {
    PrefService* local_state = g_browser_process->local_state();
    if (local_state)
      return local_state->GetString(prefs::kHardwareKeyboardLayout);
  }
  // This shouldn't happen but just in case.
  DVLOG(1) << "Local state is not yet ready.";
  return std::string();
}

string16 InputMethodDelegateImpl::GetLocalizedString(int resource_id) const {
  return l10n_util::GetStringUTF16(resource_id);
}

string16 InputMethodDelegateImpl::GetDisplayLanguageName(
    const std::string& language_code) const {
  DCHECK(g_browser_process);
  return l10n_util::GetDisplayNameForLocale(
      language_code,
      g_browser_process->GetApplicationLocale(),
      true);
}

}  // namespace input_method
}  // namespace chromeos
