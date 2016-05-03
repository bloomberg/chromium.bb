// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BROWSERTEST_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BROWSERTEST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"

class ExtensionMessageBubbleBrowserTest
    : public BrowserActionsBarBrowserTest {
 protected:
  enum AnchorPosition {
    ANCHOR_BROWSER_ACTION,
    ANCHOR_APP_MENU,
  };

  ExtensionMessageBubbleBrowserTest();
  ~ExtensionMessageBubbleBrowserTest() override;

  // BrowserActionsBarBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

  // Checks the position of the bubble present in the given |browser|, when the
  // bubble should be anchored at the given |anchor|.
  virtual void CheckBubble(Browser* browser, AnchorPosition anchor) = 0;

  // Closes the bubble present in the given |browser|.
  virtual void CloseBubble(Browser* browser) = 0;

  // Checks that there is no active bubble for the given |browser|.
  virtual void CheckBubbleIsNotPresent(Browser* browser) = 0;

  // The following are essentially the different tests, but we can't define the
  // tests in this file, since it relies on platform-specific implementation
  // (the above virtual methods).

  // Tests that an extension bubble will be anchored to an extension action when
  // there are extensions with actions.
  void TestBubbleAnchoredToExtensionAction();

  // Tests that an extension bubble will be anchored to the app menu when there
  // aren't any extensions with actions.
  // This also tests that the crashes in crbug.com/476426 are fixed.
  void TestBubbleAnchoredToAppMenu();

  // Tests that an extension bubble will be anchored to the app menu if there
  // are no highlighted extensions, even if there's a benevolent extension with
  // an action.
  // Regression test for crbug.com/485614.
  void TestBubbleAnchoredToAppMenuWithOtherAction();

  // Tests that uninstalling the extension between when the bubble is originally
  // slated to show and when it does show is handled gracefully.
  // Regression test for crbug.com/531648.
  void TestUninstallDangerousExtension();

  // Tests that the extension bubble will show on startup.
  void PreBubbleShowsOnStartup();
  void TestBubbleShowsOnStartup();

  // Tests that the developer mode warning bubble is only shown once per
  // profile.
  // Regression test for crbug.com/607099.
  void TestDevModeBubbleIsntShownTwice();

 private:
  std::unique_ptr<extensions::FeatureSwitch::ScopedOverride>
      dev_mode_bubble_override_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBrowserTest);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BROWSERTEST_H_
