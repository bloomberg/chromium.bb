// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/base_window.h"

namespace extensions {

typedef ExtensionApiTest GlobalCommandsApiTest;

#if defined(OS_WIN)
// The feature is only fully implemented on Windows, other platforms coming.
#define MAYBE_GlobalCommand GlobalCommand
#else
#define MAYBE_GlobalCommand DISABLED_GlobalCommand
#endif

// Test the basics of global commands and make sure they work when Chrome
// doesn't have focus. Also test that non-global commands are not treated as
// global and that keys beyond Ctrl+Shift+[0..9] cannot be auto-assigned by an
// extension.
IN_PROC_BROWSER_TEST_F(GlobalCommandsApiTest, MAYBE_GlobalCommand) {
  FeatureSwitch::ScopedOverride enable_global_commands(
      FeatureSwitch::global_commands(), true);

  // Our infrastructure for sending keys expects a browser to send them to, but
  // to properly test global shortcuts you need to send them to another target.
  // So, create an incognito browser to use as a target to send the shortcuts
  // to. It will ignore all of them and allow us test whether the global
  // shortcut really is global in nature and also that the non-global shortcut
  // is non-global.
  Browser* incognito_browser = CreateIncognitoBrowser();

  // Load the extension in the non-incognito browser.
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/global")) << message_;
  ASSERT_TRUE(catcher.GetNextResult());

  // Try to activate the non-global shortcut (Ctrl+Shift+1) and the
  // non-assignable shortcut (Ctrl+Shift+A) by sending the keystrokes to the
  // incognito browser. Both shortcuts should have no effect (extension is not
  // loaded there).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_1, true, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_A, true, true, false, false));

  // Activate the shortcut (Ctrl+Shift+9). This should have an effect.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_9, true, true, false, false));

  // If this fails, it might be because the global shortcut failed to work,
  // but it might also be because the non-global shortcuts unexpectedly
  // worked.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
