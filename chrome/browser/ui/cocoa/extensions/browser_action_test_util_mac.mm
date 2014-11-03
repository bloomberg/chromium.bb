// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
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

BrowserActionButton* GetButton(Browser* browser, int index) {
  return [GetController(browser) buttonWithIndex:index];
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return [GetController(browser_) buttonCount];
}

int BrowserActionTestUtil::VisibleBrowserActions() {
  return [GetController(browser_) visibleButtonCount];
}

void BrowserActionTestUtil::InspectPopup(int index) {
  NOTREACHED();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return [GetButton(browser_, index) image] != nil;
}

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  NSImage* ns_image = [GetButton(browser_, index) image];
  // gfx::Image takes ownership of the |ns_image| reference. We have to increase
  // the ref count so |ns_image| stays around when the image object is
  // destroyed.
  base::mac::NSObjectRetain(ns_image);
  return gfx::Image(ns_image);
}

void BrowserActionTestUtil::Press(int index) {
  NSButton* button = GetButton(browser_, index);
  [button performClick:nil];
}

std::string BrowserActionTestUtil::GetExtensionId(int index) {
  return [GetButton(browser_, index) viewController]->GetId();
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  NSString* tooltip = [GetButton(browser_, index) toolTip];
  return base::SysNSStringToUTF8(tooltip);
}

gfx::NativeView BrowserActionTestUtil::GetPopupNativeView() {
  return [[ExtensionPopupController popup] view];
}

bool BrowserActionTestUtil::HasPopup() {
  return [ExtensionPopupController popup] != nil;
}

gfx::Size BrowserActionTestUtil::GetPopupSize() {
  NSRect bounds = [[[ExtensionPopupController popup] view] bounds];
  return gfx::Size(NSSizeToCGSize(bounds.size));
}

bool BrowserActionTestUtil::HidePopup() {
  ExtensionPopupController* controller = [ExtensionPopupController popup];
  // The window must be gone or we'll fail a unit test with windows left open.
  [static_cast<InfoBubbleWindow*>([controller window])
      setAllowedAnimations:info_bubble::kAnimateNone];
  [controller close];
  return !HasPopup();
}

// static
void BrowserActionTestUtil::DisableAnimations() {
}

// static
void BrowserActionTestUtil::EnableAnimations() {
}

// static
gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController minPopupSize]));
}

// static
gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController maxPopupSize]));
}
