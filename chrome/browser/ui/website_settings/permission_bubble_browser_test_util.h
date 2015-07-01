// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"

namespace base {
class CommandLine;
}
class PermissionBubbleRequest;
class Browser;

class TestPermissionBubbleViewDelegate : public PermissionBubbleView::Delegate {
 public:
  TestPermissionBubbleViewDelegate();

  void ToggleAccept(int, bool) override {}
  void Accept() override {}
  void Deny() override {}
  void Closing() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPermissionBubbleViewDelegate);
};

// Use this class to test on a default window.
class PermissionBubbleBrowserTest : public virtual InProcessBrowserTest {
 public:
  PermissionBubbleBrowserTest();
  ~PermissionBubbleBrowserTest() override;

  void SetUpOnMainThread() override;

  std::vector<PermissionBubbleRequest*> requests() { return requests_.get(); }
  std::vector<bool> accept_states() { return accept_states_; }
  PermissionBubbleView::Delegate* test_delegate() { return &test_delegate_; }

 private:
  TestPermissionBubbleViewDelegate test_delegate_;
  ScopedVector<PermissionBubbleRequest> requests_;
  std::vector<bool> accept_states_;
};

// Use this class to test on an app window.
class PermissionBubbleAppBrowserTest : public PermissionBubbleBrowserTest,
                                       public ExtensionBrowserTest {
 public:
  PermissionBubbleAppBrowserTest();
  ~PermissionBubbleAppBrowserTest() override;

  void SetUpOnMainThread() override;

  // Override from ExtensionBrowserTest to avoid "inheritance via dominance".
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  Browser* app_browser() { return app_browser_; }

 private:
  Browser* app_browser_;

  Browser* OpenExtensionAppWindow(const extensions::Extension* extension);
};

// Use this class to test on a kiosk window.
class PermissionBubbleKioskBrowserTest : public PermissionBubbleBrowserTest {
 public:
  PermissionBubbleKioskBrowserTest();
  ~PermissionBubbleKioskBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
