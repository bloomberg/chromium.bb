// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockView : public PermissionBubbleView {
 public:
  MockView() : shown_(false), delegate_(NULL) {}
  virtual ~MockView() {}

  void Clear() {
    shown_ = false;
    delegate_ = NULL;
    permission_requests_.clear();
    permission_states_.clear();
  }

  // PermissionBubbleView:
  virtual void SetDelegate(Delegate* delegate) OVERRIDE {
    delegate_ = delegate;
  }

  virtual void Show(
      const std::vector<PermissionBubbleRequest*>& requests,
      const std::vector<bool>& accept_state,
      bool customization_state_) OVERRIDE {
    shown_ = true;
    permission_requests_ = requests;
    permission_states_ = accept_state;
  }

  virtual void Hide() OVERRIDE {
    shown_ = false;
  }

  virtual bool CanAcceptRequestUpdate() OVERRIDE {
    return true;
  }

  bool shown_;
  Delegate* delegate_;
  std::vector<PermissionBubbleRequest*> permission_requests_;
  std::vector<bool> permission_states_;
};

}  // namespace

class PermissionBubbleManagerTest : public testing::Test {
 public:
  PermissionBubbleManagerTest();
  virtual ~PermissionBubbleManagerTest() {}

  virtual void TearDown() {
    manager_.reset();
  }

  void ToggleAccept(int index, bool value) {
    manager_->ToggleAccept(index, value);
  }

  void Accept() {
    manager_->Accept();
  }

  void WaitForCoalescing() {
    base::MessageLoop::current()->RunUntilIdle();
  }

 protected:
  MockPermissionBubbleRequest request1_;
  MockPermissionBubbleRequest request2_;
  MockView view_;
  scoped_ptr<PermissionBubbleManager> manager_;

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

PermissionBubbleManagerTest::PermissionBubbleManagerTest()
    : request1_("test1"),
      request2_("test2"),
      manager_(new PermissionBubbleManager(NULL)),
      ui_thread_(content::BrowserThread::UI, &message_loop_) {
  manager_->SetCoalesceIntervalForTesting(0);
}

TEST_F(PermissionBubbleManagerTest, TestFlag) {
  EXPECT_FALSE(PermissionBubbleManager::Enabled());
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnablePermissionsBubbles);
  EXPECT_TRUE(PermissionBubbleManager::Enabled());
}

TEST_F(PermissionBubbleManagerTest, SingleRequest) {
  manager_->AddRequest(&request1_);
  manager_->SetView(&view_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(1), view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionBubbleManagerTest, SingleRequestViewFirst) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(1), view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionBubbleManagerTest, TwoRequests) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->SetView(&view_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);
  EXPECT_EQ(&request2_, view_.permission_requests_[1]);

  ToggleAccept(0, true);
  ToggleAccept(1, false);
  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->SetView(&view_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.delegate_ == manager_.get());
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);
  EXPECT_EQ(&request2_, view_.permission_requests_[1]);

  ToggleAccept(0, true);
  ToggleAccept(1, false);

  manager_->SetView(NULL);
  EXPECT_FALSE(view_.shown_);
  EXPECT_TRUE(view_.delegate_ == NULL);
  view_.Clear();

  manager_->SetView(&view_);
  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);
  EXPECT_EQ(&request2_, view_.permission_requests_[1]);
  EXPECT_TRUE(view_.permission_states_[0]);
  EXPECT_FALSE(view_.permission_states_[1]);

  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, NoRequests) {
  manager_->SetView(&view_);
  WaitForCoalescing();
  EXPECT_FALSE(view_.shown_);
}

TEST_F(PermissionBubbleManagerTest, NoView) {
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_FALSE(view_.shown_);
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsCoalesce) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  EXPECT_FALSE(view_.shown_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.shown_);
  EXPECT_EQ(2u, view_.permission_requests_.size());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsDoNotCoalesce) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_.shown_);
  EXPECT_EQ(1u, view_.permission_requests_.size());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsShownInTwoBubbles) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(1u, view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);

  view_.Hide();
  Accept();
  EXPECT_FALSE(view_.shown_);

  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(1u, view_.permission_requests_.size());
  EXPECT_EQ(&request2_, view_.permission_requests_[0]);
}

TEST_F(PermissionBubbleManagerTest, TestAddDuplicateRequest) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&request1_);

  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(2u, view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);
  EXPECT_EQ(&request2_, view_.permission_requests_[1]);
}

TEST_F(PermissionBubbleManagerTest, SequentialRequests) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);

  Accept();
  EXPECT_TRUE(request1_.granted());

  EXPECT_FALSE(view_.shown_);

  manager_->AddRequest(&request2_);
  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);
  Accept();
  EXPECT_FALSE(view_.shown_);
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, SameRequestRejected) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(1u, view_.permission_requests_.size());
  EXPECT_EQ(&request1_, view_.permission_requests_[0]);
}

TEST_F(PermissionBubbleManagerTest, DuplicateRequestRejected) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  MockPermissionBubbleRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());
}

TEST_F(PermissionBubbleManagerTest, DuplicateQueuedRequest) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  MockPermissionBubbleRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());

  MockPermissionBubbleRequest dupe_request2("test1");
  manager_->AddRequest(&dupe_request2);
  EXPECT_TRUE(dupe_request2.finished());
  EXPECT_FALSE(request2_.finished());
}
