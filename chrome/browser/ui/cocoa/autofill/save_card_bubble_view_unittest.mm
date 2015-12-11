// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#import "chrome/browser/ui/cocoa/autofill/save_card_bubble_view_bridge.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestSaveCardBubbleController : public SaveCardBubbleController {
 public:
  static scoped_ptr<TestSaveCardBubbleController> CreateForTesting() {
    return scoped_ptr<TestSaveCardBubbleController>(
        new TestSaveCardBubbleController());
  }

  // SaveCardBubbleController:
  base::string16 GetWindowTitle() const override { return base::string16(); }

  base::string16 GetExplanatoryMessage() const override {
    return base::string16();
  }

  void OnSaveButton() override {}
  void OnCancelButton() override {}
  void OnLearnMoreClicked() override {}
  void OnLegalMessageLinkClicked(const GURL& url) override {}
  void OnBubbleClosed() override {}

  const LegalMessageLines& GetLegalMessageLines() const override {
    return *lines_;
  }

 private:
  TestSaveCardBubbleController() { lines_.reset(new LegalMessageLines()); }

  scoped_ptr<LegalMessageLines> lines_;
};

class SaveCardBubbleViewTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    browser_window_controller_ =
        [[BrowserWindowController alloc] initWithBrowser:browser()
                                           takeOwnership:NO];
  }

  void TearDown() override {
    [browser_window_controller_ close];
    CocoaProfileTest::TearDown();
  }

 protected:
  BrowserWindowController* browser_window_controller_;
};

}  // namespace

// Test that SaveCardBubbleViewBridge and SaveCardBubbleViewCocoa can be
// created, shown, and hidden without crashing.
TEST_F(SaveCardBubbleViewTest, ShowHide) {
  scoped_ptr<TestSaveCardBubbleController> bubble_controller =
      TestSaveCardBubbleController::CreateForTesting();

  // This will show the SaveCardBubbleViewCocoa.
  SaveCardBubbleViewBridge* bridge = new SaveCardBubbleViewBridge(
      bubble_controller.get(), browser_window_controller_);

  // This sends -close to the SaveCardBubbleViewCocoa, and causes bridge to
  // delete itself.
  bridge->Hide();
}

}  // namespace autofill
