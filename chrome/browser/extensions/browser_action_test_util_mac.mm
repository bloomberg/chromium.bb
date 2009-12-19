// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_action_test_util.h"

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/cocoa/toolbar_controller.h"

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
  return [GetController(browser_) popup] != nil;
}

gfx::Rect BrowserActionTestUtil::GetPopupBounds() {
  NSRect bounds = [[[GetController(browser_) popup] view] bounds];
  return gfx::Rect(NSRectToCGRect(bounds));
}

bool BrowserActionTestUtil::HidePopup() {
  [GetController(browser_) hidePopup];
  return !HasPopup();
}

gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController minPopupSize]));
}

gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController maxPopupSize]));
}
