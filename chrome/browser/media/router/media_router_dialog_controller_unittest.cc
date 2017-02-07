// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace media_router {

class TestMediaRouterDialogController : public MediaRouterDialogController {
 public:
  explicit TestMediaRouterDialogController(content::WebContents* initiator)
      : MediaRouterDialogController(initiator) {}
  ~TestMediaRouterDialogController() override {}

  bool IsShowingMediaRouterDialog() const override { return has_dialog_; }
  void CreateMediaRouterDialog() override { has_dialog_ = true; }
  void CloseMediaRouterDialog() override { has_dialog_ = false; }

 private:
  bool has_dialog_ = false;
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() {}
  ~MockWebContentsDelegate() override {}

  MOCK_METHOD1(ActivateContents, void(content::WebContents* web_contents));
};

class MediaRouterDialogControllerTest : public ChromeRenderViewHostTestHarness {
 protected:
  MediaRouterDialogControllerTest() {}
  ~MediaRouterDialogControllerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_contents_delegate_.reset(new MockWebContentsDelegate());
    web_contents()->SetDelegate(web_contents_delegate_.get());
    dialog_controller_.reset(
        new TestMediaRouterDialogController(web_contents()));
  }

  void TearDown() override {
    dialog_controller_.reset();
    web_contents_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void RequestSuccess(const content::PresentationSessionInfo&,
                      const MediaRoute&) {}
  void RequestError(const content::PresentationError& error) {}

  std::unique_ptr<CreatePresentationConnectionRequest> GetRequest() {
    return std::unique_ptr<CreatePresentationConnectionRequest>(
        new CreatePresentationConnectionRequest(
            RenderFrameHostId(1, 2),
            {GURL("http://example.com"), GURL("http://example2.com")},
            url::Origin(GURL("http://google.com")),
            base::Bind(&MediaRouterDialogControllerTest::RequestSuccess,
                       base::Unretained(this)),
            base::Bind(&MediaRouterDialogControllerTest::RequestError,
                       base::Unretained(this))));
  }

  std::unique_ptr<TestMediaRouterDialogController> dialog_controller_;
  std::unique_ptr<MockWebContentsDelegate> web_contents_delegate_;
};

#if defined(OS_ANDROID)
// The non-Android implementation is tested in
// MediaRouterDialogControllerImplTest.
TEST_F(MediaRouterDialogControllerTest, CreateForWebContents) {
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents());
  ASSERT_NE(dialog_controller, nullptr);
}
#endif

TEST_F(MediaRouterDialogControllerTest, ShowAndHideDialog) {
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(dialog_controller_->ShowMediaRouterDialog());
  EXPECT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());

  // If a dialog is already shown, ShowMediaRouterDialog() should return false.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_FALSE(dialog_controller_->ShowMediaRouterDialog());

  dialog_controller_->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller_->IsShowingMediaRouterDialog());

  // Once the dialog is hidden, ShowMediaRouterDialog() should return true
  // again.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(dialog_controller_->ShowMediaRouterDialog());
}

TEST_F(MediaRouterDialogControllerTest, ShowDialogForPresentation) {
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(
      dialog_controller_->ShowMediaRouterDialogForPresentation(GetRequest()));
  EXPECT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());

  // If a dialog is already shown, ShowMediaRouterDialogForPresentation() should
  // return false.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_FALSE(
      dialog_controller_->ShowMediaRouterDialogForPresentation(GetRequest()));
}

}  // namespace media_router
