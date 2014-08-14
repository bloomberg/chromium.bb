// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_util.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
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


void SimulateTouchScreenInChromeVoxForTest(content::BrowserContext* profile) {
  // ChromeVox looks at whether 'ontouchstart' exists to know whether or not it
  // should respond to hover events. Fake it so that touch exploration events
  // get spoken.
  extensions::ExtensionHost* host =
      extensions::ExtensionSystem::Get(profile)
          ->process_manager()
          ->GetBackgroundHostForExtension(
              extension_misc::kChromeVoxExtensionId);
  CHECK(content::ExecuteScript(
      host->host_contents(),
      "if (!('ontouchstart' in window)) window.ontouchstart = function() {};"));
}

}  // namespace accessibility
}  // namespace chromeos
