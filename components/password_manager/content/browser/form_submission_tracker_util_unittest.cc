// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "components/password_manager/core/browser/form_submission_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class FormSubmissionObserverMock : public FormSubmissionObserver {
 public:
  MOCK_METHOD1(DidNavigateMainFrame, void(bool form_may_be_submitted));
};

class FormSubmissionTrackerUtilTest
    : public content::RenderViewHostTestHarness {
 public:
  FormSubmissionTrackerUtilTest() = default;
  ~FormSubmissionTrackerUtilTest() override = default;

  FormSubmissionObserverMock& observer() { return observer_; }

 private:
  FormSubmissionObserverMock observer_;

  DISALLOW_COPY_AND_ASSIGN(FormSubmissionTrackerUtilTest);
};

TEST_F(FormSubmissionTrackerUtilTest, NotRendererInitiated) {
  EXPECT_CALL(observer(), DidNavigateMainFrame(false));
  NotifyDidNavigateMainFrame(false /* is_renderer_initiated */,
                             ui::PAGE_TRANSITION_FORM_SUBMIT, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, LinkTransition) {
  EXPECT_CALL(observer(), DidNavigateMainFrame(false));
  NotifyDidNavigateMainFrame(true /* is_renderer_initiated */,
                             ui::PAGE_TRANSITION_LINK, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, RendererInitiatedFormSubmit) {
  EXPECT_CALL(observer(), DidNavigateMainFrame(true));
  NotifyDidNavigateMainFrame(true /* is_renderer_initiated */,
                             ui::PAGE_TRANSITION_FORM_SUBMIT, &observer());
}

}  // namespace
}  // namespace password_manager
