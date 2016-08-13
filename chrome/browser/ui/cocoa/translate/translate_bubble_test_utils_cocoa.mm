// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"

// TODO(groby): Share with translate_bubble_controller_unittest.mm
@implementation BrowserWindowController (ForTesting)

- (TranslateBubbleController*)translateBubbleController {
  return translateBubbleController_;
}

@end

namespace translate {

namespace test_utils {

const TranslateBubbleModel* GetCurrentModel(Browser* browser) {
  DCHECK(browser);
  NSWindow* native_window = browser->window()->GetNativeWindow();
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForWindow:native_window];
  return [[controller translateBubbleController] model];
}

void PressTranslate(Browser* browser) {
  DCHECK(browser);
  NSWindow* native_window = browser->window()->GetNativeWindow();
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForWindow:native_window];
  [[controller translateBubbleController] handleTranslateButtonPressed:nil];
}

}  // namespace test_utils

}  // namespace translate
