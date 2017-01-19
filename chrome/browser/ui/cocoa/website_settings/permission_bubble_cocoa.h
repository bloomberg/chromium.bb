// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_PERMISSION_BUBBLE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_PERMISSION_BUBBLE_COCOA_H_

#import <Foundation/Foundation.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/website_settings/permission_prompt.h"
#include "content/public/browser/web_contents.h"

class Browser;
@class PermissionBubbleController;

class PermissionBubbleCocoa : public PermissionPrompt {
 public:
  explicit PermissionBubbleCocoa(Browser* browser);
  ~PermissionBubbleCocoa() override;

  // PermissionPrompt:
  void Show(const std::vector<PermissionRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  void Hide() override;
  bool IsVisible() override;
  void SetDelegate(Delegate* delegate) override;
  bool CanAcceptRequestUpdate() override;
  void UpdateAnchorPosition() override;
  gfx::NativeWindow GetNativeWindow() override;

  // Called when |bubbleController_| is closing.
  void OnBubbleClosing();

 private:
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleBrowserTest,
                           HasLocationBarByDefault);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleBrowserTest,
                           BrowserFullscreenHasLocationBar);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleBrowserTest,
                           TabFullscreenHasLocationBar);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleBrowserTest, AppHasNoLocationBar);
  FRIEND_TEST_ALL_PREFIXES(PermissionBubbleKioskBrowserTest,
                           KioskHasNoLocationBar);

  Browser* browser_;    // Weak.
  Delegate* delegate_;  // Weak.

  // Cocoa-side UI controller for the bubble.  Weak, as it will close itself.
  PermissionBubbleController* bubbleController_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_WEBSITE_SETTINGS_PERMISSION_BUBBLE_COCOA_H_
