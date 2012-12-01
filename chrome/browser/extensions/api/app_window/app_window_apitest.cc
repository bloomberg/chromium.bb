// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "ui/gfx/rect.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiBounds) {
  ExtensionTestMessageListener background_listener("background_ok", false);
  ExtensionTestMessageListener ready_listener("ready", true /* will_reply */);
  ExtensionTestMessageListener success_listener("success", false);

  LoadAndLaunchPlatformApp("windows_api_bounds");
  ASSERT_TRUE(background_listener.WaitUntilSatisfied());
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ShellWindow* window = GetFirstShellWindow();

  gfx::Rect new_bounds(100, 200, 300, 400);
  window->GetBaseWindow()->SetBounds(new_bounds);

  // TODO(jeremya/asargent) figure out why in GTK the window doesn't end up
  // with exactly the bounds we set. Is it a bug in our shell window
  // implementation?  crbug.com/160252
#ifdef TOOLKIT_GTK
  int slop = 50;
#else
  int slop = 0;
#endif  // !TOOLKIT_GTK

  ready_listener.Reply(base::IntToString(slop));
  ASSERT_TRUE(success_listener.WaitUntilSatisfied());

  CloseShellWindowsAndWaitForAppToExit();
}

// TODO(asargent) - Fix onMinimzed event on OSX (crbug.com/162793) and figure
// out what to do about the fact that minimize events don't work under ubuntu
// unity (crbug.com/162794 and https://bugs.launchpad.net/unity/+bug/998073).
#if defined(TOOLKIT_VIEWS)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiProperties) {
  EXPECT_TRUE(
      RunPlatformAppTest("platform_apps/windows_api_properties")) << message_;
}

#endif  // defined(TOOLKIT_VIEWS)

}  // namespace extensions
