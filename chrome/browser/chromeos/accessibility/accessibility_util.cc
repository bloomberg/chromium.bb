// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

// TODO(yoshiki): move the following method to accessibility_manager.cc and
// remove this file.

namespace chromeos {
namespace accessibility {

void EnableVirtualKeyboard(bool enabled) {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool IsVirtualKeyboardEnabled() {
  if (!g_browser_process) {
    return false;
  }
  PrefService* prefs = g_browser_process->local_state();
  bool virtual_keyboard_enabled =
      prefs && prefs->GetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled);
  return virtual_keyboard_enabled;
}

void ShowAccessibilityHelp(Browser* browser) {
  chrome::ShowSingletonTab(browser, GURL(chrome::kChromeAccessibilityHelpURL));
}

}  // namespace accessibility
}  // namespace chromeos
