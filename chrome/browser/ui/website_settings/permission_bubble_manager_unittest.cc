// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_factory.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    view_factory_.reset(new MockPermissionBubbleFactory(false, manager_.get()));
  }

  void TearDown() override {
    view_factory_.reset();
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

  void MockTabSwitchAway() { manager_->HideBubble(); }

  void MockTabSwitchBack() { manager_->DisplayPendingRequests(); }

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
  scoped_ptr<MockPermissionBubbleFactory> view_factory_;
};

TEST_F(PermissionBubbleManagerTest, SingleRequest) {
  manager_->AddRequest(&request1_);
  manager_->DisplayPendingRequests();
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionBubbleManagerTest, SingleRequestViewFirst) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);

  ToggleAccept(0, true);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionBubbleManagerTest, TwoRequests) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->DisplayPendingRequests();
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 2);

  ToggleAccept(0, true);
  ToggleAccept(1, false);
  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->DisplayPendingRequests();
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 2);

  ToggleAccept(0, true);
  ToggleAccept(1, false);

  MockTabSwitchAway();
  EXPECT_FALSE(view_factory_->is_visible());

  MockTabSwitchBack();
  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, NoRequests) {
  manager_->DisplayPendingRequests();
  WaitForCoalescing();
  EXPECT_FALSE(view_factory_->is_visible());
}

TEST_F(PermissionBubbleManagerTest, NoView) {
  manager_->AddRequest(&request1_);
  // Don't display the pending requests.
  WaitForCoalescing();
  EXPECT_FALSE(view_factory_->is_visible());
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsCoalesce) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  EXPECT_FALSE(view_factory_->is_visible());
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 2);
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsDoNotCoalesce) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);
}

TEST_F(PermissionBubbleManagerTest, TwoRequestsShownInTwoBubbles) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);

  Accept();
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);
  ASSERT_EQ(view_factory_->show_count(), 2);
}

TEST_F(PermissionBubbleManagerTest, TestAddDuplicateRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&request1_);

  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 2);
}

TEST_F(PermissionBubbleManagerTest, SequentialRequests) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());

  EXPECT_FALSE(view_factory_->is_visible());

  manager_->AddRequest(&request2_);
  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());
  Accept();
  EXPECT_FALSE(view_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionBubbleManagerTest, SameRequestRejected) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);
}

TEST_F(PermissionBubbleManagerTest, DuplicateRequestCancelled) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  MockPermissionBubbleRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_TRUE(request1_.finished());
}

TEST_F(PermissionBubbleManagerTest, DuplicateQueuedRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  MockPermissionBubbleRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());

  MockPermissionBubbleRequest dupe_request2("test2");
  manager_->AddRequest(&dupe_request2);
  EXPECT_FALSE(dupe_request2.finished());
  EXPECT_FALSE(request2_.finished());

  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_TRUE(request1_.finished());

  manager_->CancelRequest(&request2_);
  EXPECT_TRUE(dupe_request2.finished());
  EXPECT_TRUE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&iframe_request_other_domain_);

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForCoalescing();

  EXPECT_FALSE(view_factory_->is_visible());
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, TestCancelQueued) {
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(view_factory_->is_visible());

  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(view_factory_->is_visible());
  manager_->DisplayPendingRequests();
  EXPECT_FALSE(view_factory_->is_visible());

  manager_->AddRequest(&request2_);
  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);
}

TEST_F(PermissionBubbleManagerTest, TestCancelWhileDialogShown) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();

  view_factory_->SetCanUpdateUi(true);
  EXPECT_TRUE(view_factory_->is_visible());
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(view_factory_->is_visible());
}

TEST_F(PermissionBubbleManagerTest, TestCancelWhileDialogShownNoUpdate) {
  manager_->DisplayPendingRequests();
  view_factory_->SetCanUpdateUi(false);
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  view_factory_->SetCanUpdateUi(false);

  EXPECT_TRUE(view_factory_->is_visible());
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
}

TEST_F(PermissionBubbleManagerTest, TestCancelPendingRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);
  manager_->CancelRequest(&request2_);

  EXPECT_TRUE(view_factory_->is_visible());
  EXPECT_FALSE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForCoalescing();
  WaitForFrameLoad();

  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 2);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(iframe_request_same_domain_.finished());
  EXPECT_FALSE(view_factory_->is_visible());
}

TEST_F(PermissionBubbleManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();

  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());

  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  ASSERT_EQ(view_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  EXPECT_TRUE(view_factory_->is_visible());
  ASSERT_EQ(view_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForCoalescing();
  EXPECT_TRUE(view_factory_->is_visible());

  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionBubbleManagerTest, IFrameUserGestureRequest) {
  iframe_request_other_domain_.SetHasUserGesture();
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForCoalescing();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(request2_.finished());
  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(request2_.finished());
}

TEST_F(PermissionBubbleManagerTest, AllUserGestureRequests) {
  iframe_request_other_domain_.SetHasUserGesture();
  request2_.SetHasUserGesture();
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForCoalescing();
  WaitForFrameLoad();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(request2_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(view_factory_->is_visible());
  Closing();
  EXPECT_TRUE(request2_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  EXPECT_FALSE(view_factory_->is_visible());
}

TEST_F(PermissionBubbleManagerTest, RequestsDontNeedUserGesture) {
  manager_->DisplayPendingRequests();
  WaitForFrameLoad();
  WaitForCoalescing();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  manager_->AddRequest(&request2_);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(view_factory_->is_visible());
}
