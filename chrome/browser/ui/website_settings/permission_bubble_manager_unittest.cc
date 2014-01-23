// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/permission_bubble_delegate.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDelegate : public PermissionBubbleDelegate {
 public:
  MockDelegate() : granted_(false), cancelled_(false), finished_(false) {}
  virtual ~MockDelegate() {}

  // PermissionBubbleDelegate:
  virtual int GetIconID() const OVERRIDE {
    return 11;
  }

  virtual base::string16 GetMessageText() const OVERRIDE {
    return base::ASCIIToUTF16("test");
  }

  virtual base::string16 GetAlternateAcceptButtonText() const OVERRIDE {
    return base::ASCIIToUTF16("button");
  }

  virtual void PermissionGranted() OVERRIDE {
    granted_ = true;
  }

  virtual void PermissionDenied() OVERRIDE {
    granted_ = false;
  }

  virtual void Cancelled() OVERRIDE {
    granted_ = false;
    cancelled_ = true;
  }

  virtual void RequestFinished() OVERRIDE {
    finished_ = true;
  }

  bool granted_;
  bool cancelled_;
  bool finished_;
};

class MockView : public PermissionBubbleView {
 public:
  MockView() : shown_(false), delegate_(NULL) {}
  ~MockView() {}

  void Clear() {
    shown_ = false;
    delegate_ = NULL;
    permission_delegates_.clear();
    permission_states_.clear();
  }

  // PermissionBubbleView:
  virtual void SetDelegate(Delegate* delegate) OVERRIDE {
    delegate_ = delegate;
  }

  virtual void AddPermissionBubbleDelegate(
      PermissionBubbleDelegate* delegate) OVERRIDE {}

  virtual void RemovePermissionBubbleDelegate(
      PermissionBubbleDelegate* delegate) OVERRIDE {}

  virtual void Show(
      const std::vector<PermissionBubbleDelegate*>& delegates,
      const std::vector<bool>& accept_state) OVERRIDE {
    shown_ = true;
    permission_delegates_ = delegates;
    permission_states_ = accept_state;
  }

  virtual void Hide() OVERRIDE {
    shown_ = false;
  }

  bool shown_;
  Delegate* delegate_;
  std::vector<PermissionBubbleDelegate*> permission_delegates_;
  std::vector<bool> permission_states_;
};

}  // namespace

class PermissionBubbleManagerTest : public testing::Test {
 public:
  PermissionBubbleManagerTest();

  void ToggleAccept(int index, bool value) {
    manager_->ToggleAccept(index, value);
  }

  void Accept() {
    manager_->Accept();
  }

 protected:
  MockDelegate delegate1_;
  MockDelegate delegate2_;
  MockView view_;
  scoped_ptr<PermissionBubbleManager> manager_;
};

PermissionBubbleManagerTest::PermissionBubbleManagerTest()
    : manager_(new PermissionBubbleManager(NULL)) {
}

TEST_F(PermissionBubbleManagerTest, TestFlag) {
  EXPECT_FALSE(PermissionBubbleManager::Enabled());
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnablePermissionsBubbles);
  EXPECT_TRUE(PermissionBubbleManager::Enabled());
}

TEST_F(PermissionBubbleManagerTest, SingleRequest) {
  manager_->AddPermissionBubbleDelegate(&delegate1_);
  manager_->SetView(&view_);

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(1), view_.permission_delegates_.size());
  EXPECT_EQ(&delegate1_, view_.permission_delegates_[0]);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(delegate1_.granted_);
}

TEST_F(PermissionBubbleManagerTest, TwoRequests) {
  manager_->AddPermissionBubbleDelegate(&delegate1_);
  manager_->AddPermissionBubbleDelegate(&delegate2_);
  manager_->SetView(&view_);

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_.permission_delegates_.size());
  EXPECT_EQ(&delegate1_, view_.permission_delegates_[0]);
  EXPECT_EQ(&delegate2_, view_.permission_delegates_[1]);

  ToggleAccept(0, true);
  ToggleAccept(1, false);
  Accept();
  EXPECT_TRUE(delegate1_.granted_);
  EXPECT_FALSE(delegate2_.granted_);
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsTabSwitch) {
  manager_->AddPermissionBubbleDelegate(&delegate1_);
  manager_->AddPermissionBubbleDelegate(&delegate2_);
  manager_->SetView(&view_);

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_.permission_delegates_.size());
  EXPECT_EQ(&delegate1_, view_.permission_delegates_[0]);
  EXPECT_EQ(&delegate2_, view_.permission_delegates_[1]);

  ToggleAccept(0, true);
  ToggleAccept(1, false);

  manager_->SetView(NULL);
  EXPECT_FALSE(view_.shown_);
  EXPECT_TRUE(view_.delegate_ == NULL);
  view_.Clear();

  manager_->SetView(&view_);
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_.permission_delegates_.size());
  EXPECT_EQ(&delegate1_, view_.permission_delegates_[0]);
  EXPECT_EQ(&delegate2_, view_.permission_delegates_[1]);
  EXPECT_TRUE(view_.permission_states_[0]);
  EXPECT_FALSE(view_.permission_states_[1]);

  Accept();
  EXPECT_TRUE(delegate1_.granted_);
  EXPECT_FALSE(delegate2_.granted_);
}

