// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/confirm_bubble_controller.h"
#import "chrome/browser/ui/cocoa/confirm_bubble_view.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "grit/theme_resources.h"
#import "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/gfx/point.h"

namespace {

// The model object used in this test. This model implements all methods and
// updates its status when ConfirmBubbleController calls its methods.
class TestConfirmBubbleModel : public ConfirmBubbleModel {
 public:
  TestConfirmBubbleModel();
  virtual ~TestConfirmBubbleModel() OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(BubbleButton button) const OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual void LinkClicked() OVERRIDE;

  bool accept_clicked() const { return accept_clicked_; }
  bool cancel_clicked() const { return cancel_clicked_; }
  bool link_clicked() const { return link_clicked_; }

 private:
  bool accept_clicked_;
  bool cancel_clicked_;
  bool link_clicked_;
};

TestConfirmBubbleModel::TestConfirmBubbleModel() :
  accept_clicked_(false),
  cancel_clicked_(false),
  link_clicked_(false) {
}

TestConfirmBubbleModel::~TestConfirmBubbleModel() {
}

string16 TestConfirmBubbleModel::GetTitle() const {
  return ASCIIToUTF16("Test");
}

string16 TestConfirmBubbleModel::GetMessageText() const {
  return ASCIIToUTF16("Test Message");
}

gfx::Image* TestConfirmBubbleModel::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
}

int TestConfirmBubbleModel::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 TestConfirmBubbleModel::GetButtonLabel(BubbleButton button) const {
  return button == BUTTON_OK ? ASCIIToUTF16("OK") : ASCIIToUTF16("Cancel");
}

void TestConfirmBubbleModel::Accept() {
  accept_clicked_ = true;
}

void TestConfirmBubbleModel::Cancel() {
  cancel_clicked_ = true;
}

string16 TestConfirmBubbleModel::GetLinkText() const {
  return ASCIIToUTF16("Link");
}

void TestConfirmBubbleModel::LinkClicked() {
  link_clicked_ = true;
}

}  // namespace

class ConfirmBubbleControllerTest : public CocoaTest {
 public:
  ConfirmBubbleControllerTest() {
    NSView* view = [test_window() contentView];
    model_.reset(new TestConfirmBubbleModel);
    gfx::Point origin(0, 0);
    controller_ =
        [[ConfirmBubbleController alloc] initWithParent:view
                                                 origin:origin.ToCGPoint()
                                                  model:model_.get()];
    [view addSubview:[controller_ view]
          positioned:NSWindowAbove
          relativeTo:nil];
  }

  ConfirmBubbleView* GetBubbleView() const {
    return (ConfirmBubbleView*)[controller_ view];
  }

  TestConfirmBubbleModel* model() const {
    return model_.get();
  }

 private:
  ConfirmBubbleController* controller_;  // weak; owns self
  scoped_ptr<TestConfirmBubbleModel> model_;
};

// Verify clicking a button or a link removes the ConfirmBubbleView object and
// calls an appropriate model method.
TEST_F(ConfirmBubbleControllerTest, ClickOk) {
  NSView* view = [test_window() contentView];
  ConfirmBubbleView* bubble_view = GetBubbleView();
  bool contains_bubble_view = [[view subviews] containsObject:bubble_view];
  EXPECT_TRUE(contains_bubble_view);

  // Click its OK button and verify this view has been removed from the test
  // window. Also verify TestConfirmBubbleModel::Accept() has been called.
  [bubble_view clickOk];

  contains_bubble_view = [[view subviews] containsObject:bubble_view];
  EXPECT_FALSE(contains_bubble_view);
  EXPECT_TRUE(model()->accept_clicked());
  EXPECT_FALSE(model()->cancel_clicked());
  EXPECT_FALSE(model()->link_clicked());
}

TEST_F(ConfirmBubbleControllerTest, ClickCancel) {
  NSView* view = [test_window() contentView];
  ConfirmBubbleView* bubble_view = GetBubbleView();
  bool contains_bubble_view = [[view subviews] containsObject:bubble_view];
  EXPECT_TRUE(contains_bubble_view);

  // Click its cancel button and verify this view has been removed from the test
  // window. Also verify TestConfirmBubbleModel::Cancel() has been called.
  [bubble_view clickCancel];

  contains_bubble_view = [[view subviews] containsObject:bubble_view];
  EXPECT_FALSE(contains_bubble_view);
  EXPECT_FALSE(model()->accept_clicked());
  EXPECT_TRUE(model()->cancel_clicked());
  EXPECT_FALSE(model()->link_clicked());
}

TEST_F(ConfirmBubbleControllerTest, ClickLink) {
  NSView* view = [test_window() contentView];
  ConfirmBubbleView* bubble_view = GetBubbleView();
  bool contains_bubble_view = [[view subviews] containsObject:bubble_view];
  EXPECT_TRUE(contains_bubble_view);

  // Click its link and verify this view has been removed from the test window.
  // Also verify TestConfirmBubbleModel::LinkClicked() has been called.
  [bubble_view clickLink];

  contains_bubble_view = [[view subviews] containsObject:bubble_view];
  EXPECT_FALSE(contains_bubble_view);
  EXPECT_FALSE(model()->accept_clicked());
  EXPECT_FALSE(model()->cancel_clicked());
  EXPECT_TRUE(model()->link_clicked());
}
