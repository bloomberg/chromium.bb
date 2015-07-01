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
  ~MockView() override {}

  static scoped_ptr<PermissionBubbleView> Create(Browser* browser) {
    return make_scoped_ptr(new MockView());
  }

  void Clear() {
    shown_ = false;
    can_accept_updates_ = true;
    delegate_ = NULL;
    permission_requests_.clear();
    permission_states_.clear();
  }

  // PermissionBubbleView:
  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }

  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override {
    shown_ = true;
    permission_requests_ = requests;
    permission_states_ = accept_state;
  }

  void Hide() override { shown_ = false; }
  bool CanAcceptRequestUpdate() override { return can_accept_updates_; }
  bool IsVisible() override { return shown_; }
  void UpdateAnchorPosition() override {}
  gfx::NativeWindow GetNativeWindow() override { return nullptr; }

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
  ~PermissionBubbleManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("http://www.google.com"));

    manager_.reset(new PermissionBubbleManager(web_contents()));
    manager_->view_factory_ = base::Bind(MockView::Create);
  }

  void TearDown() override {
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
  scoped_ptr<PermissionBubbleManager> manager_;

  MockView* view_() { return static_cast<MockView*>(manager_->view_.get()); }

  void ShowBubble() {
    // nullptr browser is OK for test.
    manager_->DisplayPendingRequests(nullptr);
  }
};

TEST_F(PermissionBubbleManagerTest, TestFlag) {
  EXPECT_TRUE(PermissionBubbleManager::Enabled());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePermissionsBubbles);
  EXPECT_FALSE(PermissionBubbleManager::Enabled());
}

TEST_F(PermissionBubbleManagerTest, SingleRequest) {
  manager_->AddRequest(&request1_);
  ShowBubble();
  WaitForCoalescing();

  EXPECT_TRUE(view_()->delegate_ == manager_.get());
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(static_cast<size_t>(1), view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionBubbleManagerTest, SingleRequestViewFirst) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_()->delegate_ == manager_.get());
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(static_cast<size_t>(1), view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionBubbleManagerTest, TwoRequests) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  ShowBubble();
  WaitForCoalescing();

  EXPECT_TRUE(view_()->delegate_ == manager_.get());
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);
  EXPECT_EQ(&request2_, view_()->permission_requests_[1]);

  ToggleAccept(0, true);
  ToggleAccept(1, false);
  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  ShowBubble();
  WaitForCoalescing();

  EXPECT_TRUE(view_()->delegate_ == manager_.get());
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);
  EXPECT_EQ(&request2_, view_()->permission_requests_[1]);

  ToggleAccept(0, true);
  ToggleAccept(1, false);

  manager_->HideBubble();
  EXPECT_FALSE(view_());

  ShowBubble();
  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(static_cast<size_t>(2), view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);
  EXPECT_EQ(&request2_, view_()->permission_requests_[1]);
  EXPECT_TRUE(view_()->permission_states_[0]);
  EXPECT_FALSE(view_()->permission_states_[1]);

  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, NoRequests) {
  ShowBubble();
  WaitForCoalescing();
  EXPECT_FALSE(view_()->shown_);
}

TEST_F(PermissionBubbleManagerTest, NoView) {
  manager_->AddRequest(&request1_);
  // Don't display the pending requests.
  WaitForCoalescing();
  EXPECT_FALSE(view_());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsCoalesce) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  EXPECT_FALSE(view_()->shown_);
  WaitForCoalescing();

  EXPECT_TRUE(view_()->shown_);
  EXPECT_EQ(2u, view_()->permission_requests_.size());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsDoNotCoalesce) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_()->shown_);
  EXPECT_EQ(1u, view_()->permission_requests_.size());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsShownInTwoBubbles) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(1u, view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);

  view_()->Hide();
  Accept();
  WaitForCoalescing();

  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(1u, view_()->permission_requests_.size());
  EXPECT_EQ(&request2_, view_()->permission_requests_[0]);
}

TEST_F(PermissionBubbleManagerTest, TestAddDuplicateRequest) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&request1_);

  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(2u, view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);
  EXPECT_EQ(&request2_, view_()->permission_requests_[1]);
}

TEST_F(PermissionBubbleManagerTest, SequentialRequests) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);

  Accept();
  EXPECT_TRUE(request1_.granted());

  EXPECT_FALSE(view_()->shown_);

  manager_->AddRequest(&request2_);
  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);
  Accept();
  EXPECT_FALSE(view_()->shown_);
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, SameRequestRejected) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(1u, view_()->permission_requests_.size());
  EXPECT_EQ(&request1_, view_()->permission_requests_[0]);
}

TEST_F(PermissionBubbleManagerTest, DuplicateRequestRejected) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  MockPermissionBubbleRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());
}

TEST_F(PermissionBubbleManagerTest, DuplicateQueuedRequest) {
  ShowBubble();
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
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&iframe_request_other_domain_);

  EXPECT_TRUE(view_()->shown_);
  ASSERT_EQ(1u, view_()->permission_requests_.size());

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForCoalescing();

  EXPECT_FALSE(view_()->shown_);
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, TestCancel) {
  manager_->HideBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  ShowBubble();
  EXPECT_FALSE(view_()->shown_);

  manager_->AddRequest(&request2_);
  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);
}

TEST_F(PermissionBubbleManagerTest, TestCancelWhileDialogShown) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_()->shown_);
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
}

TEST_F(PermissionBubbleManagerTest, TestCancelWhileDialogShownNoUpdate) {
  ShowBubble();
  view_()->can_accept_updates_ = false;
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_()->shown_);
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  Closing();
}

TEST_F(PermissionBubbleManagerTest, TestCancelPendingRequest) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_()->shown_);
  EXPECT_EQ(1u, view_()->permission_requests_.size());
  manager_->CancelRequest(&request2_);

  EXPECT_TRUE(view_()->shown_);
  EXPECT_FALSE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, MainFrameNoRequestIFrameRequest) {
  ShowBubble();
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForCoalescing();
  WaitForFrameLoad();

  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, MainFrameAndIFrameRequestSameDomain) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();

  EXPECT_TRUE(view_()->shown_);
  EXPECT_EQ(2u, view_()->permission_requests_.size());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(iframe_request_same_domain_.finished());
  EXPECT_FALSE(view_()->shown_);
}

TEST_F(PermissionBubbleManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();

  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, IFrameRequestWhenMainRequestVisible) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);

  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  EXPECT_EQ(1u, view_()->permission_requests_.size());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  EXPECT_TRUE(view_()->shown_);
  EXPECT_EQ(1u, view_()->permission_requests_.size());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  ShowBubble();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_()->shown_);

  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, IFrameUserGestureRequest) {
  iframe_request_other_domain_.SetHasUserGesture();
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(request2_.finished());
  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, AllUserGestureRequests) {
  iframe_request_other_domain_.SetHasUserGesture();
  request2_.SetHasUserGesture();
  ShowBubble();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForCoalescing();
  WaitForFrameLoad();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(request2_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_()->shown_);
  Closing();
  EXPECT_TRUE(request2_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(view_()->shown_);
}

TEST_F(PermissionBubbleManagerTest, RequestsWithoutUserGesture) {
  manager_->RequireUserGesture(true);
  ShowBubble();
  WaitForFrameLoad();
  WaitForCoalescing();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  manager_->AddRequest(&request2_);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(view_()->shown_);
}

TEST_F(PermissionBubbleManagerTest, RequestsWithUserGesture) {
  manager_->RequireUserGesture(true);
  ShowBubble();
  WaitForFrameLoad();
  WaitForCoalescing();
  request1_.SetHasUserGesture();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  manager_->AddRequest(&request2_);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(view_()->shown_);
}

TEST_F(PermissionBubbleManagerTest, RequestsDontNeedUserGesture) {
  ShowBubble();
  WaitForFrameLoad();
  WaitForCoalescing();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  manager_->AddRequest(&request2_);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(view_()->shown_);
}
