// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/mock_permission_request.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

class PermissionRequestManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(),
        request1_("test1",
                  PermissionRequestType::QUOTA,
                  PermissionRequestGestureType::GESTURE),
        request2_("test2",
                  PermissionRequestType::DOWNLOAD,
                  PermissionRequestGestureType::NO_GESTURE),
        request_mic_("mic",
                     PermissionRequestType::PERMISSION_MEDIASTREAM_MIC,
                     PermissionRequestGestureType::NO_GESTURE),
        request_camera_("cam",
                        PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA,
                        PermissionRequestGestureType::NO_GESTURE),
        iframe_request_same_domain_("iframe",
                                    PermissionRequestType::UNKNOWN,
                                    GURL("http://www.google.com/some/url")),
        iframe_request_other_domain_("iframe",
                                     PermissionRequestType::UNKNOWN,
                                     GURL("http://www.youtube.com")),
        iframe_request_mic_other_domain_(
            "iframe",
            PermissionRequestType::PERMISSION_MEDIASTREAM_MIC,
            GURL("http://www.youtube.com")) {}
  ~PermissionRequestManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("http://www.google.com"));

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_.reset(new MockPermissionPromptFactory(manager_));
  }

  void TearDown() override {
    prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Accept() {
    manager_->Accept();
  }

  void Deny() {
    manager_->Deny();
  }

  void Closing() {
    manager_->Closing();
  }

  void WaitForFrameLoad() {
    // PermissionRequestManager ignores all parameters. Yay?
    manager_->DocumentLoadedInFrame(NULL);
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame();
    base::RunLoop().RunUntilIdle();
  }

  void MockTabSwitchAway() { manager_->HideBubble(); }

  void MockTabSwitchBack() { manager_->DisplayPendingRequests(); }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

 protected:
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  MockPermissionRequest request_mic_;
  MockPermissionRequest request_camera_;
  MockPermissionRequest iframe_request_same_domain_;
  MockPermissionRequest iframe_request_other_domain_;
  MockPermissionRequest iframe_request_mic_other_domain_;
  PermissionRequestManager* manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(PermissionRequestManagerTest, SingleRequest) {
  manager_->AddRequest(&request1_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionRequestManagerTest, SingleRequestViewFirst) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Most requests should never be grouped.
TEST_F(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request2_);
  manager_->DisplayPendingRequests();

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
}

// Only mic/camera requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraGrouped) {
  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(&iframe_request_mic_other_domain_);
  manager_->AddRequest(&request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  MockTabSwitchAway();
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}

TEST_F(PermissionRequestManagerTest, NoRequests) {
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

#if !defined(OS_ANDROID)
TEST_F(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  manager_->AddRequest(&request1_);
  // Don't mark the tab as active.
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}
#endif

TEST_F(PermissionRequestManagerTest, TwoRequestsDoNotCoalesce) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TwoRequestsShownInTwoBubbles) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  ASSERT_EQ(prompt_factory_->show_count(), 2);
}

TEST_F(PermissionRequestManagerTest, TestAddDuplicateRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, SequentialRequests) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());

  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(&request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionRequestManagerTest, SameRequestRejected) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, DuplicateRequestCancelled) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  MockPermissionRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(dupe_request.finished());
  EXPECT_TRUE(request1_.finished());
}

TEST_F(PermissionRequestManagerTest, DuplicateQueuedRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  MockPermissionRequest dupe_request("test1");
  manager_->AddRequest(&dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());

  MockPermissionRequest dupe_request2("test2");
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

TEST_F(PermissionRequestManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);
  manager_->AddRequest(&iframe_request_other_domain_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForBubbleToBeShown();

  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, TestCancelQueued) {
  manager_->AddRequest(&request1_);
  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(prompt_factory_->is_visible());
  manager_->DisplayPendingRequests();
  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(&request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

TEST_F(PermissionRequestManagerTest, TestCancelWhileDialogShown) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();

  prompt_factory_->SetCanUpdateUi(true);
  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, TestCancelWhileDialogShownNoUpdate) {
  manager_->DisplayPendingRequests();
  prompt_factory_->SetCanUpdateUi(false);
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  prompt_factory_->SetCanUpdateUi(false);

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_FALSE(request1_.finished());
  manager_->CancelRequest(&request1_);
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
}

TEST_F(PermissionRequestManagerTest, TestCancelPendingRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  manager_->CancelRequest(&request2_);

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_FALSE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(&iframe_request_same_domain_);
  WaitForFrameLoad();
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_same_domain_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->DisplayPendingRequests();
  manager_->AddRequest(&request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(&iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  EXPECT_TRUE(request1_.finished());
  EXPECT_FALSE(iframe_request_other_domain_.finished());
  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, RequestsDontNeedUserGesture) {
  manager_->DisplayPendingRequests();
  WaitForFrameLoad();
  WaitForBubbleToBeShown();
  manager_->AddRequest(&request1_);
  manager_->AddRequest(&iframe_request_other_domain_);
  manager_->AddRequest(&request2_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptRequestsPerPrompt, 1, 1);

  Accept();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
}

TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request2_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();

  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShownNoGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::DOWNLOAD),
      1);
  // No need to test the other UMA for showing prompts again, they were tested
  // in UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptAccepted, 0);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::DOWNLOAD),
      1);

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDeniedNoGesture,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::DOWNLOAD),
      1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptDeniedGesture, 0);
}

// This code path (calling Accept on a non-merged bubble, with no accepted
// permission) would never be used in actual Chrome, but its still tested for
// completeness.
TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedBubbleAlternatePath) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request1_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::QUOTA),
      1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedAcceptedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectBucketCount(
      PermissionUmaUtil::kPermissionsPromptMergedBubbleTypes,
      static_cast<base::HistogramBase::Sample>(
          PermissionRequestType::PERMISSION_MEDIASTREAM_MIC),
      1);
  histograms.ExpectBucketCount(
      PermissionUmaUtil::kPermissionsPromptMergedBubbleTypes,
      static_cast<base::HistogramBase::Sample>(
          PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA),
      1);
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptRequestsPerPrompt, 2, 1);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectTotalCount(
      PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);

  Accept();

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectBucketCount(
      PermissionUmaUtil::kPermissionsPromptMergedBubbleAccepted,
      static_cast<base::HistogramBase::Sample>(
          PermissionRequestType::PERMISSION_MEDIASTREAM_MIC),
      1);
  histograms.ExpectBucketCount(
      PermissionUmaUtil::kPermissionsPromptMergedBubbleAccepted,
      static_cast<base::HistogramBase::Sample>(
          PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA),
      1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedDeniedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(&request_mic_);
  manager_->AddRequest(&request_camera_);
  manager_->DisplayPendingRequests();
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForMergedAcceptedBubble.

  Deny();

  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(PermissionRequestType::MULTIPLE),
      1);
  histograms.ExpectBucketCount(
      PermissionUmaUtil::kPermissionsPromptMergedBubbleDenied,
      static_cast<base::HistogramBase::Sample>(
          PermissionRequestType::PERMISSION_MEDIASTREAM_MIC),
      1);
  histograms.ExpectBucketCount(
      PermissionUmaUtil::kPermissionsPromptMergedBubbleDenied,
      static_cast<base::HistogramBase::Sample>(
          PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA),
      1);
}
