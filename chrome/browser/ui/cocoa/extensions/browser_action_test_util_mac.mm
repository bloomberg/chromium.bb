// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/chrome_constants.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

// The Cocoa implementation of the TestToolbarActionsBarHelper, which creates
// (and owns) a BrowserActionsController and BrowserActionsContainerView for
// testing purposes.
class TestToolbarActionsBarHelperCocoa : public TestToolbarActionsBarHelper {
 public:
  TestToolbarActionsBarHelperCocoa(Browser* browser,
                                   BrowserActionsController* mainController);
  ~TestToolbarActionsBarHelperCocoa() override;

  BrowserActionsController* controller() { return controller_.get(); }

 private:
  // The owned BrowserActionsContainerView and BrowserActionsController; the
  // mac implementation of the ToolbarActionsBar delegate and view.
  base::scoped_nsobject<BrowserActionsContainerView> containerView_;
  base::scoped_nsobject<BrowserActionsController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionsBarHelperCocoa);
};

TestToolbarActionsBarHelperCocoa::TestToolbarActionsBarHelperCocoa(
    Browser* browser,
    BrowserActionsController* mainController) {
  // Make sure that Cocoa has been bootstrapped.
  if (!base::mac::FrameworkBundle()) {
    // Look in the framework bundle for resources.
    base::FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.Append(chrome::kFrameworkName);
    base::mac::SetOverrideFrameworkBundlePath(path);
  }

  containerView_.reset([[BrowserActionsContainerView alloc]
      initWithFrame:NSMakeRect(0, 0, 0, 15)]);
  controller_.reset(
      [[BrowserActionsController alloc] initWithBrowser:browser
                                          containerView:containerView_.get()
                                         mainController:mainController]);
}

TestToolbarActionsBarHelperCocoa::~TestToolbarActionsBarHelperCocoa() {
}

BrowserActionsController* GetController(Browser* browser,
                                        TestToolbarActionsBarHelper* helper) {
  if (helper) {
    return static_cast<TestToolbarActionsBarHelperCocoa*>(helper)->controller();
  }

  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser->window());
  return [[window->cocoa_controller() toolbarController]
           browserActionsController];
}

BrowserActionButton* GetButton(
    Browser* browser,
    TestToolbarActionsBarHelper* helper,
    int index) {
  return [GetController(browser, helper) buttonWithIndex:index];
}

}  // namespace

BrowserActionTestUtil::BrowserActionTestUtil(Browser* browser)
    : BrowserActionTestUtil(browser, true) {
}

BrowserActionTestUtil::BrowserActionTestUtil(Browser* browser,
                                             bool is_real_window)
    : browser_(browser) {
  if (!is_real_window)
    test_helper_.reset(new TestToolbarActionsBarHelperCocoa(browser, nullptr));
  // We disable animations on extension popups so that tests aren't waiting for
  // a popup to fade out.
  [ExtensionPopupController setAnimationsEnabledForTesting:NO];
}

BrowserActionTestUtil::~BrowserActionTestUtil() {}

int BrowserActionTestUtil::NumberOfBrowserActions() {
  return [GetController(browser_, test_helper_.get()) buttonCount];
}

int BrowserActionTestUtil::VisibleBrowserActions() {
  return [GetController(browser_, test_helper_.get()) visibleButtonCount];
}

bool BrowserActionTestUtil::IsChevronShowing() {
  BrowserActionsController* controller =
      GetController(browser_, test_helper_.get());
  // The magic "18" comes from kChevronWidth in browser_actions_controller.mm.
  return ![controller chevronIsHidden] &&
         NSWidth([[controller containerView] animationEndFrame]) >= 18;
}

void BrowserActionTestUtil::InspectPopup(int index) {
  NOTREACHED();
}

bool BrowserActionTestUtil::HasIcon(int index) {
  return [GetButton(browser_, test_helper_.get(), index) image] != nil;
}

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  NSImage* ns_image = [GetButton(browser_, test_helper_.get(), index) image];
  // gfx::Image takes ownership of the |ns_image| reference. We have to increase
  // the ref count so |ns_image| stays around when the image object is
  // destroyed.
  base::mac::NSObjectRetain(ns_image);
  return gfx::Image(ns_image);
}

void BrowserActionTestUtil::Press(int index) {
  NSButton* button = GetButton(browser_, test_helper_.get(), index);
  [button performClick:nil];
}

std::string BrowserActionTestUtil::GetExtensionId(int index) {
  return
      [GetButton(browser_, test_helper_.get(), index) viewController]->GetId();
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  NSString* tooltip = [GetButton(browser_, test_helper_.get(), index) toolTip];
  return base::SysNSStringToUTF8(tooltip);
}

gfx::NativeView BrowserActionTestUtil::GetPopupNativeView() {
  ToolbarActionViewController* popup_owner =
      GetToolbarActionsBar()->popup_owner();
  return popup_owner ? popup_owner->GetPopupNativeView() : nil;
}

bool BrowserActionTestUtil::HasPopup() {
  return GetPopupNativeView() != nil;
}

gfx::Size BrowserActionTestUtil::GetPopupSize() {
  NSRect bounds = [[[ExtensionPopupController popup] view] bounds];
  return gfx::Size(NSSizeToCGSize(bounds.size));
}

bool BrowserActionTestUtil::HidePopup() {
  ExtensionPopupController* controller = [ExtensionPopupController popup];
  [controller close];
  return !HasPopup();
}

bool BrowserActionTestUtil::ActionButtonWantsToRun(size_t index) {
  return [GetButton(browser_, test_helper_.get(), index) wantsToRunForTesting];
}

void BrowserActionTestUtil::SetWidth(int width) {
  BrowserActionsContainerView* containerView =
      [GetController(browser_, test_helper_.get()) containerView];
  NSRect frame = [containerView frame];
  frame.size.width = width;
  [containerView setFrame:frame];
}

bool BrowserActionTestUtil::IsHighlightingForSurfacingBubble() {
  BrowserActionsContainerView* containerView =
      [GetController(browser_, test_helper_.get()) containerView];
  return [containerView trackingEnabled] && [containerView isHighlighting];
}

ToolbarActionsBar* BrowserActionTestUtil::GetToolbarActionsBar() {
  return [GetController(browser_, test_helper_.get()) toolbarActionsBar];
}

scoped_ptr<BrowserActionTestUtil> BrowserActionTestUtil::CreateOverflowBar() {
  CHECK(!GetToolbarActionsBar()->in_overflow_mode())
      << "Only a main bar can create an overflow bar!";
  return make_scoped_ptr(new BrowserActionTestUtil(browser_, this));
}

// static
gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController minPopupSize]));
}

// static
gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(NSSizeToCGSize([ExtensionPopupController maxPopupSize]));
}

BrowserActionTestUtil::BrowserActionTestUtil(Browser* browser,
                                             BrowserActionTestUtil* main_bar)
    : browser_(browser),
      test_helper_(new TestToolbarActionsBarHelperCocoa(
          browser_, GetController(browser_, main_bar->test_helper_.get()))) {
}
