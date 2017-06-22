// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene_manager.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/android/vr_shell/ui_browser_interface.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element_debug_id.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr_shell {

namespace {

class MockBrowserInterface : public UiBrowserInterface {
 public:
  MockBrowserInterface() {}
  ~MockBrowserInterface() override {}

  MOCK_METHOD0(ExitPresent, void());
  MOCK_METHOD0(ExitFullscreen, void());
  MOCK_METHOD0(NavigateBack, void());
  MOCK_METHOD0(ExitCct, void());
  MOCK_METHOD1(OnUnsupportedMode, void(UiUnsupportedMode mode));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBrowserInterface);
};

std::set<UiElementDebugId> kElementsVisibleInBrowsing = {
    kContentQuad, kBackplane, kCeiling, kFloor, kUrlBar};
std::set<UiElementDebugId> kElementsVisibleWithExitPrompt = {
    kExitPrompt, kExitPromptBackplane, kCeiling, kFloor};

}  // namespace

class UiSceneManagerTest : public testing::Test {
 public:
  void SetUp() override { browser_ = base::MakeUnique<MockBrowserInterface>(); }

 protected:
  enum InCct : bool {
    kNotInCct = false,
    kInCct = true,
  };

  enum InWebVr : bool {
    kNotInWebVr = false,
    kInWebVr = true,
  };

  enum WebVrAutopresented : bool {
    kNotAutopresented = false,
    kAutopresented = true,
  };

  void MakeManager(InCct in_cct, InWebVr in_web_vr) {
    scene_ = base::MakeUnique<UiScene>();
    manager_ = base::MakeUnique<UiSceneManager>(
        browser_.get(), scene_.get(), in_cct, in_web_vr, kNotAutopresented);
  }

  void MakeAutoPresentedManager() {
    scene_ = base::MakeUnique<UiScene>();
    manager_ = base::MakeUnique<UiSceneManager>(
        browser_.get(), scene_.get(), kNotInCct, kInWebVr, kAutopresented);
  }

  bool IsVisible(UiElementDebugId debug_id) {
    UiElement* element = scene_->GetUiElementByDebugId(debug_id);
    return element ? element->visible() : false;
  }

  void VerifyElementsVisible(const std::string& debug_name,
                             const std::set<UiElementDebugId>& elements) {
    SCOPED_TRACE(debug_name);
    for (const auto& element : scene_->GetUiElements()) {
      SCOPED_TRACE(element->debug_id());
      bool should_be_visible =
          elements.find(element->debug_id()) != elements.end();
      EXPECT_EQ(should_be_visible, element->visible());
    }
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<MockBrowserInterface> browser_;
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<UiSceneManager> manager_;
};

TEST_F(UiSceneManagerTest, ExitPresentAndFullscreenOnAppButtonClick) {
  MakeManager(kNotInCct, kInWebVr);

  // Clicking app button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent()).Times(1);
  // And also trigger exit fullscreen.
  EXPECT_CALL(*browser_, ExitFullscreen()).Times(1);
  manager_->OnAppButtonClicked();
}

TEST_F(UiSceneManagerTest, WebVrWarningsShowWhenInitiallyInWebVr) {
  MakeManager(kNotInCct, kInWebVr);

  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrSecureOrigin(true);
  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrSecureOrigin(false);
  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrMode(false, false, false);
  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));
}

TEST_F(UiSceneManagerTest, WebVrWarningsDoNotShowWhenInitiallyOutsideWebVr) {
  MakeManager(kNotInCct, kNotInWebVr);

  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrMode(true, false, false);
  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));
}

// Tests toast not showing when directly entering VR though WebVR presentation.
TEST_F(UiSceneManagerTest, WebVrToastNotShowingWhenInitiallyInWebVr) {
  MakeManager(kNotInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kPresentationToast));
}

// Tests toast visibility is controlled by show_toast parameter.
TEST_F(UiSceneManagerTest, WebVrToastShowAndHide) {
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kPresentationToast));

  manager_->SetWebVrMode(true, false, true);
  EXPECT_TRUE(IsVisible(kPresentationToast));

  manager_->SetWebVrMode(true, false, false);
  EXPECT_FALSE(IsVisible(kPresentationToast));

  manager_->SetWebVrMode(false, false, true);
  EXPECT_TRUE(IsVisible(kPresentationToast));

  manager_->SetWebVrMode(false, false, false);
  EXPECT_FALSE(IsVisible(kPresentationToast));
}

TEST_F(UiSceneManagerTest, CloseButtonVisibleInCctFullscreen) {
  // Button should be visible in cct.
  MakeManager(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should not be visible when not in cct or fullscreen.
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should be visible in fullscreen and hidden when leaving fullscreen.
  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  manager_->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should not be visible when in WebVR.
  MakeManager(kInCct, kInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));
  manager_->SetWebVrMode(false, false, false);
  EXPECT_TRUE(IsVisible(kCloseButton));

  // Button should be visible in Cct across transistions in fullscreen.
  MakeManager(kInCct, kNotInWebVr);
  EXPECT_TRUE(IsVisible(kCloseButton));
  manager_->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  manager_->SetFullscreen(false);
  EXPECT_TRUE(IsVisible(kCloseButton));
}

TEST_F(UiSceneManagerTest, UiUpdatesForIncognito) {
  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = scene_->GetWorldBackgroundColor();
  manager_->SetFullscreen(true);

  {
    SCOPED_TRACE("Entered Fullsceen");
    // Make sure background has changed for fullscreen.
    EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  }

  SkColor fullscreen_background = scene_->GetWorldBackgroundColor();

  manager_->SetIncognito(true);

  {
    SCOPED_TRACE("Entered Incognito");
    // Make sure background has changed for incognito.
    EXPECT_NE(fullscreen_background, scene_->GetWorldBackgroundColor());
    EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  }

  SkColor incognito_background = scene_->GetWorldBackgroundColor();

  manager_->SetIncognito(false);

  {
    SCOPED_TRACE("Exited Incognito");
    EXPECT_EQ(fullscreen_background, scene_->GetWorldBackgroundColor());
  }

  manager_->SetFullscreen(false);

  {
    SCOPED_TRACE("Exited Fullsceen");
    EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  }

  manager_->SetIncognito(true);

  {
    SCOPED_TRACE("Entered Incognito");
    EXPECT_EQ(incognito_background, scene_->GetWorldBackgroundColor());
  }

  manager_->SetIncognito(false);

  {
    SCOPED_TRACE("Exited Incognito");
    EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  }
}

TEST_F(UiSceneManagerTest, WebVrAutopresentedInitially) {
  MakeAutoPresentedManager();
  manager_->SetWebVrSecureOrigin(true);
  VerifyElementsVisible("Autopresented",
                        std::set<UiElementDebugId>{kTransientUrlBar});
}

TEST_F(UiSceneManagerTest, WebVrAutopresented) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Enter WebVR with autopresentation.
  manager_->SetWebVrMode(true, true, false);

  VerifyElementsVisible("Autopresented",
                        std::set<UiElementDebugId>{kTransientUrlBar});
}

TEST_F(UiSceneManagerTest, UiUpdatesForFullscreenChanges) {
  std::set<UiElementDebugId> visible_in_fullscreen = {
      kContentQuad, kCloseButton, kBackplane, kCeiling, kFloor};

  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = scene_->GetWorldBackgroundColor();
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // In fullscreen mode, content elements should be visible, control elements
  // should be hidden.
  manager_->SetFullscreen(true);
  VerifyElementsVisible("In fullscreen", visible_in_fullscreen);
  {
    SCOPED_TRACE("Entered Fullsceen");
    // Make sure background has changed for fullscreen.
    EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  }

  // Everything should return to original state after leaving fullscreen.
  manager_->SetFullscreen(false);
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
  {
    SCOPED_TRACE("Exited Fullsceen");
    EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  }
}

TEST_F(UiSceneManagerTest, UiUpdatesExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Exit prompt visible state.
  manager_->OnSecurityIconClickedForTesting();
  VerifyElementsVisible("Prompt visible", kElementsVisibleWithExitPrompt);

  // Back to initial state.
  manager_->OnExitPromptPrimaryButtonClickedForTesting();
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, BackplaneClickClosesExitPrompt) {
  MakeManager(kNotInCct, kNotInWebVr);

  manager_->SetWebVrSecureOrigin(true);

  // Initial state.
  VerifyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Exit prompt visible state.
  manager_->OnSecurityIconClickedForTesting();
  VerifyElementsVisible("Prompt visble", kElementsVisibleWithExitPrompt);

  // Back to initial state.
  scene_->GetUiElementByDebugId(kExitPromptBackplane)
      ->OnButtonUp(gfx::PointF());
  VerifyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
}

TEST_F(UiSceneManagerTest, UiUpdatesForWebVR) {
  MakeManager(kNotInCct, kInWebVr);

  manager_->SetWebVrSecureOrigin(true);
  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);

  // All elements should be hidden.
  VerifyElementsVisible("Elements hidden", std::set<UiElementDebugId>{});
}

TEST_F(UiSceneManagerTest, UiUpdateTransitionToWebVR) {
  MakeManager(kNotInCct, kNotInWebVr);
  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);

  // Transition to WebVR mode
  manager_->SetWebVrMode(true, false, false);
  manager_->SetWebVrSecureOrigin(true);

  // All elements should be hidden.
  VerifyElementsVisible("Elements hidden", std::set<UiElementDebugId>{});
}

TEST_F(UiSceneManagerTest, CaptureIndicatorsVisibility) {
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kVideoCaptureIndicator));
  EXPECT_FALSE(IsVisible(kScreenCaptureIndicator));
  EXPECT_FALSE(IsVisible(kLocationAccessIndicator));

  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);
  manager_->SetLocationAccessIndicator(true);

  EXPECT_TRUE(IsVisible(kAudioCaptureIndicator));
  EXPECT_TRUE(IsVisible(kVideoCaptureIndicator));
  EXPECT_TRUE(IsVisible(kScreenCaptureIndicator));
  EXPECT_TRUE(IsVisible(kLocationAccessIndicator));

  manager_->SetWebVrMode(true, false, false);
  EXPECT_FALSE(IsVisible(kAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kVideoCaptureIndicator));
  EXPECT_FALSE(IsVisible(kScreenCaptureIndicator));
  EXPECT_FALSE(IsVisible(kLocationAccessIndicator));

  manager_->SetWebVrMode(false, false, false);
  EXPECT_TRUE(IsVisible(kAudioCaptureIndicator));
  EXPECT_TRUE(IsVisible(kVideoCaptureIndicator));
  EXPECT_TRUE(IsVisible(kScreenCaptureIndicator));
  EXPECT_TRUE(IsVisible(kLocationAccessIndicator));

  manager_->SetAudioCapturingIndicator(false);
  manager_->SetVideoCapturingIndicator(false);
  manager_->SetScreenCapturingIndicator(false);
  manager_->SetLocationAccessIndicator(false);

  EXPECT_FALSE(IsVisible(kAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kVideoCaptureIndicator));
  EXPECT_FALSE(IsVisible(kScreenCaptureIndicator));
  EXPECT_FALSE(IsVisible(kLocationAccessIndicator));
}
}  // namespace vr_shell
