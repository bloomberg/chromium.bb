// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_helper.h"

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

// The Cocoa implementation of the TestToolbarActionsBarHelper, which creates
// (and owns) a BrowserActionsController and BrowserActionsContainerView for
// testing purposes.
class TestToolbarActionsBarHelperCocoa : public TestToolbarActionsBarHelper {
 public:
  TestToolbarActionsBarHelperCocoa(Browser* browser,
                                   TestToolbarActionsBarHelperCocoa* mainBar);
  ~TestToolbarActionsBarHelperCocoa() override;

 private:
  // TestToolbarActionsBarHelper:
  ToolbarActionsBar* GetToolbarActionsBar() override;

  // The owned BrowserActionsContainerView and BrowserActionsController; the
  // mac implementation of the ToolbarActionsBar delegate and view.
  base::scoped_nsobject<BrowserActionsContainerView> containerView_;
  base::scoped_nsobject<BrowserActionsController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionsBarHelperCocoa);
};

TestToolbarActionsBarHelperCocoa::TestToolbarActionsBarHelperCocoa(
    Browser* browser,
    TestToolbarActionsBarHelperCocoa* mainBar) {
  // Make sure that Cocoa has been bootstrapped.
  CocoaTest::BootstrapCocoa();

  containerView_.reset([[BrowserActionsContainerView alloc]
      initWithFrame:NSMakeRect(0, 0, 0, 15)]);
  BrowserActionsController* mainController =
      mainBar ? mainBar->controller_.get() : nil;
  controller_.reset([[BrowserActionsController alloc]
      initWithBrowser:browser
        containerView:containerView_.get()
       mainController:mainController]);
}

TestToolbarActionsBarHelperCocoa::~TestToolbarActionsBarHelperCocoa() {}

ToolbarActionsBar* TestToolbarActionsBarHelperCocoa::GetToolbarActionsBar() {
  return [controller_ toolbarActionsBar];
}

}  // namespace

scoped_ptr<TestToolbarActionsBarHelper> TestToolbarActionsBarHelper::Create(
    Browser* browser,
    TestToolbarActionsBarHelper* main_bar) {
  return make_scoped_ptr(new TestToolbarActionsBarHelperCocoa(
      browser,
      static_cast<TestToolbarActionsBarHelperCocoa*>(main_bar)));
}
