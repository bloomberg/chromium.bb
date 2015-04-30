// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"

#include "base/logging.h"

class TestToolbarActionsBarBubbleDelegate::DelegateImpl
    : public ToolbarActionsBarBubbleDelegate {
 public:
  explicit DelegateImpl(TestToolbarActionsBarBubbleDelegate* parent)
      : parent_(parent) {}
  ~DelegateImpl() override {}

 private:
  base::string16 GetHeadingText() override { return parent_->heading_; }
  base::string16 GetBodyText() override { return parent_->body_; }
  base::string16 GetItemListText() override { return parent_->item_list_; }
  base::string16 GetActionButtonText() override { return parent_->action_; }
  base::string16 GetDismissButtonText() override { return parent_->dismiss_; }
  base::string16 GetLearnMoreButtonText() override {
    return parent_->learn_more_;
  }
  void OnBubbleShown() override {
    CHECK(!parent_->shown_);
    parent_->shown_ = true;
  }
  void OnBubbleClosed(CloseAction action) override {
    CHECK(!parent_->close_action_);
    parent_->close_action_.reset(new CloseAction(action));
  }

  TestToolbarActionsBarBubbleDelegate* parent_;

  DISALLOW_COPY_AND_ASSIGN(DelegateImpl);
};

TestToolbarActionsBarBubbleDelegate::TestToolbarActionsBarBubbleDelegate(
    const base::string16& heading,
    const base::string16& body,
    const base::string16& action)
    : shown_(false),
      heading_(heading),
      body_(body),
      action_(action) {
}

TestToolbarActionsBarBubbleDelegate::~TestToolbarActionsBarBubbleDelegate() {
  // If the bubble didn't close, it means that it still owns the DelegateImpl,
  // which has a weak ptr to this object. Make sure that this class always
  // outlives the bubble.
  CHECK(close_action_);
}

scoped_ptr<ToolbarActionsBarBubbleDelegate>
TestToolbarActionsBarBubbleDelegate::GetDelegate() {
  return scoped_ptr<ToolbarActionsBarBubbleDelegate>(new DelegateImpl(this));
}
