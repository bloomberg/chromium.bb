// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_action_test_util.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

BrowserActionsController* GetController(Browser* browser) {
  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser->window());

  return [[window->cocoa_controller() toolbarController]
           browserActionsController];
}

NSButton* GetButton(Browser* browser, int index) {
  return [GetController(browser) buttonWithIndex:index];
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return [GetController(browser_) buttonCount];
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return [GetButton(browser_, index) image] != nil;
}

void BrowserActionTestUtil::Press(int index) {
  NSButton* button = GetButton(browser_, index);
  [button performClick:nil];
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  NSString* tooltip = [GetButton(browser_, index) toolTip];
  return base::SysNSStringToUTF8(tooltip);
}

bool BrowserActionTestUtil::HasPopup() {
  return [ExtensionPopupController popup] != nil;
}

gfx::Rect BrowserActionTestUtil::GetPopupBounds() {
  NSRect bounds = [[[ExtensionPopupController popup] view] bounds];
  return gfx::Rect(NSRectToCGRect(bounds));
}

bool BrowserActionTestUtil::HidePopup() {
  ExtensionPopupController* controller = [ExtensionPopupController popup];
  // The window must be gone or we'll fail a unit test with windows left open.
  [static_cast<InfoBubbleWindow*>([controller window]) setDelayOnClose:NO];
  [controller close];
  return !HasPopup();
}

gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController minPopupSize]));
}

gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController maxPopupSize]));
}
