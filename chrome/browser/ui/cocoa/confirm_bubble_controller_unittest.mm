// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/confirm_bubble_controller.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/confirm_bubble_cocoa.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#import "testing/gtest_mac.h"
#import "ui/gfx/geometry/point.h"

namespace {

// The model object used in this test. This model implements all methods and
// updates its status when ConfirmBubbleController calls its methods.
class TestConfirmBubbleModel : public ConfirmBubbleModel {
 public:
  TestConfirmBubbleModel(bool* model_deleted,
                         bool* accept_clicked,
                         bool* cancel_clicked,
                         bool* link_clicked);
  TestConfirmBubbleModel();
  ~TestConfirmBubbleModel() override;
  base::string16 GetTitle() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(BubbleButton button) const override;
  void Accept() override;
  void Cancel() override;
  base::string16 GetLinkText() const override;
  void LinkClicked() override;

 private:
  bool* model_deleted_;
  bool* accept_clicked_;
  bool* cancel_clicked_;
  bool* link_clicked_;
};

TestConfirmBubbleModel::TestConfirmBubbleModel(bool* model_deleted,
                                               bool* accept_clicked,
                                               bool* cancel_clicked,
                                               bool* link_clicked)
    : model_deleted_(model_deleted),
      accept_clicked_(accept_clicked),
      cancel_clicked_(cancel_clicked),
      link_clicked_(link_clicked) {
}

TestConfirmBubbleModel::~TestConfirmBubbleModel() {
  *model_deleted_ = true;
}

base::string16 TestConfirmBubbleModel::GetTitle() const {
  return base::ASCIIToUTF16("Test");
}

base::string16 TestConfirmBubbleModel::GetMessageText() const {
  return base::ASCIIToUTF16("Test Message");
}

int TestConfirmBubbleModel::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 TestConfirmBubbleModel::GetButtonLabel(
    BubbleButton button) const {
  return button == BUTTON_OK ? base::ASCIIToUTF16("OK")
                             : base::ASCIIToUTF16("Cancel");
}

void TestConfirmBubbleModel::Accept() {
  *accept_clicked_ = true;
}

void TestConfirmBubbleModel::Cancel() {
  *cancel_clicked_ = true;
}

base::string16 TestConfirmBubbleModel::GetLinkText() const {
  return base::ASCIIToUTF16("Link");
}

void TestConfirmBubbleModel::LinkClicked() {
  *link_clicked_ = true;
}

}  // namespace

class ConfirmBubbleControllerTest : public CocoaTest {
 public:
  ConfirmBubbleControllerTest()
      : model_deleted_(false),
        accept_clicked_(false),
        cancel_clicked_(false),
        link_clicked_(false) {
    NSView* view = [test_window() contentView];
    // This model is owned by the controller created below.
    model_.reset(new TestConfirmBubbleModel(&model_deleted_,
                                            &accept_clicked_,
                                            &cancel_clicked_,
                                            &link_clicked_));
    gfx::Point origin(0, 0);
    controller_ =
        [[ConfirmBubbleController alloc] initWithParent:view
                                                 origin:origin.ToCGPoint()
                                                  model:std::move(model_)];
    [view addSubview:[controller_ view]
          positioned:NSWindowAbove
          relativeTo:nil];
  }

  ConfirmBubbleCocoa* GetBubble() const {
    return (ConfirmBubbleCocoa*)[controller_ view];
  }

  bool model_deleted() const { return model_deleted_; }
  bool accept_clicked() const { return accept_clicked_; }
  bool cancel_clicked() const { return cancel_clicked_; }
  bool link_clicked() const { return link_clicked_; }

 private:
  ConfirmBubbleController* controller_;  // weak; owns self
  std::unique_ptr<TestConfirmBubbleModel> model_;
  bool model_deleted_;
  bool accept_clicked_;
  bool cancel_clicked_;
  bool link_clicked_;
};

// Verify clicking a button or a link removes the ConfirmBubbleCocoa object and
// calls an appropriate model method.
TEST_F(ConfirmBubbleControllerTest, ClickOk) {
  NSView* view = [test_window() contentView];
  ConfirmBubbleCocoa* bubble = GetBubble();
  bool contains_bubble_view = [[view subviews] containsObject:bubble];
  EXPECT_TRUE(contains_bubble_view);

  // Click its OK button and verify this view has been removed from the test
  // window. Also verify TestConfirmBubbleModel::Accept() has been called.
  [bubble clickOk];

  contains_bubble_view = [[view subviews] containsObject:bubble];
  EXPECT_FALSE(contains_bubble_view);
  EXPECT_TRUE(accept_clicked());
  EXPECT_FALSE(cancel_clicked());
  EXPECT_FALSE(link_clicked());
}

TEST_F(ConfirmBubbleControllerTest, ClickCancel) {
  NSView* view = [test_window() contentView];
  ConfirmBubbleCocoa* bubble = GetBubble();
  bool contains_bubble_view = [[view subviews] containsObject:bubble];
  EXPECT_TRUE(contains_bubble_view);

  // Click its cancel button and verify this view has been removed from the test
  // window. Also verify TestConfirmBubbleModel::Cancel() has been called.
  [bubble clickCancel];

  contains_bubble_view = [[view subviews] containsObject:bubble];
  EXPECT_FALSE(contains_bubble_view);
  EXPECT_FALSE(accept_clicked());
  EXPECT_TRUE(cancel_clicked());
  EXPECT_FALSE(link_clicked());
}

TEST_F(ConfirmBubbleControllerTest, ClickLink) {
  NSView* view = [test_window() contentView];
  ConfirmBubbleCocoa* bubble = GetBubble();
  bool contains_bubble_view = [[view subviews] containsObject:bubble];
  EXPECT_TRUE(contains_bubble_view);

  // Click its link and verify this view has been removed from the test window.
  // Also verify TestConfirmBubbleModel::LinkClicked() has been called.
  [bubble clickLink];

  contains_bubble_view = [[view subviews] containsObject:bubble];
  EXPECT_FALSE(contains_bubble_view);
  EXPECT_FALSE(accept_clicked());
  EXPECT_FALSE(cancel_clicked());
  EXPECT_TRUE(link_clicked());
}
