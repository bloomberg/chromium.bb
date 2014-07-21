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
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockView : public PermissionBubbleView {
 public:
  MockView() : shown_(false), can_accept_updates_(true), delegate_(NULL) {}
  virtual ~MockView() {}

  void Clear() {
    shown_ = false;
    can_accept_updates_ = true;
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
    return can_accept_updates_;
  }

  virtual bool IsVisible() OVERRIDE {
    return shown_;
  }

  bool shown_;
  bool can_accept_updates_;
  Delegate* delegate_;
  std::vector<PermissionBubbleRequest*> permission_requests_;
  std::vector<bool> permission_states_;
};

}  // namespace

class PermissionBubbleManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PermissionBubbleManagerTest()
      : ChromeRenderViewHostTestHarness(),
        request1_("test1"),
        request2_("test2"),
        iframe_request_same_domain_("iframe",
                                    GURL("http://www.google.com/some/url")),
        iframe_request_other_domain_("iframe",
                                     GURL("http://www.youtube.com")) {}
  virtual ~PermissionBubbleManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("http://www.google.com"));

    manager_.reset(new PermissionBubbleManager(web_contents()));
  }

  virtual void TearDown() OVERRIDE {
    manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ToggleAccept(int index, bool value) {
    manager_->ToggleAccept(index, value);
  }

  void Accept() {
    manager_->Accept();
  }

  void Closing() {
    manager_->Closing();
  }

  void WaitForFrameLoad() {
    // PermissionBubbleManager ignores all parameters. Yay?
    manager_->DocumentLoadedInFrame(NULL);
    base::MessageLoop::current()->RunUntilIdle();
  }

  void WaitForCoalescing() {
    manager_->DocumentOnLoadCompletedInMainFrame();
    base::MessageLoop::current()->RunUntilIdle();
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

 protected:
  MockPermissionBubbleRequest request1_;
  MockPermissionBubbleRequest request2_;
  MockPermissionBubbleRequest iframe_request_same_domain_;
  MockPermissionBubbleRequest iframe_request_other_domain_;
  MockView view_;
  scoped_ptr<PermissionBubbleManager> manager_;
};

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

TEST_F(PermissionBubbleManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&iframe_request_other_domain_);

  EXPECT_TRUE(view_.shown_);
  ASSERT_EQ(1u, view_.permission_requests_.size());

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForCoalescing();

  EXPECT_FALSE(view_.shown_);
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, TestCancel) {
  manager_->SetView(NULL);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  manager_->SetView(&view_);
  EXPECT_FALSE(view_.shown_);

  manager_->AddRequest(&request2_);
  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);
}

TEST_F(PermissionBubbleManagerTest, TestCancelWhileDialogShown) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.shown_);
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
}

TEST_F(PermissionBubbleManagerTest, TestCancelWhileDialogShownNoUpdate) {
  manager_->SetView(&view_);
  view_.can_accept_updates_ = false;
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_.shown_);
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  Closing();
}

TEST_F(PermissionBubbleManagerTest, TestCancelPendingRequest) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_.shown_);
  EXPECT_EQ(1u, view_.permission_requests_.size());
  manager_->CancelRequest(&request2_);

  EXPECT_TRUE(view_.shown_);
  EXPECT_FALSE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->SetView(&view_);
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForCoalescing();
  WaitForFrameLoad();

  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();

  EXPECT_TRUE(view_.shown_);
  EXPECT_EQ(2u, view_.permission_requests_.size());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(iframe_request_same_domain_.finished());
  EXPECT_FALSE(view_.shown_);
}

TEST_F(PermissionBubbleManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();

  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);

  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  EXPECT_EQ(1u, view_.permission_requests_.size());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  EXPECT_TRUE(view_.shown_);
  EXPECT_EQ(1u, view_.permission_requests_.size());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_.shown_);

  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, IFrameUserGestureRequest) {
  iframe_request_other_domain_.SetHasUserGesture();
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(request2_.finished());
  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, AllUserGestureRequests) {
  iframe_request_other_domain_.SetHasUserGesture();
  request2_.SetHasUserGesture();
  manager_->SetView(&view_);
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForCoalescing();
  WaitForFrameLoad();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(request2_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_.shown_);
  Closing();
  EXPECT_TRUE(request2_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(view_.shown_);
}
