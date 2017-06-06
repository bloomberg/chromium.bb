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

using testing::InSequence;

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

  void MakeManager(InCct in_cct, InWebVr in_web_vr) {
    scene_ = base::MakeUnique<UiScene>();
    manager_ = base::MakeUnique<UiSceneManager>(browser_.get(), scene_.get(),
                                                in_cct, in_web_vr);
  }

  bool IsVisible(UiElementDebugId debug_id) {
    UiElement* element = scene_->GetUiElementByDebugId(debug_id);
    return element ? element->visible() : false;
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

  manager_->SetWebVrMode(false);
  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));
}

TEST_F(UiSceneManagerTest, WebVrWarningsDoNotShowWhenInitiallyOutsideWebVr) {
  MakeManager(kNotInCct, kNotInWebVr);

  EXPECT_FALSE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_FALSE(IsVisible(kWebVrTransientHttpSecurityWarning));

  manager_->SetWebVrMode(true);
  EXPECT_TRUE(IsVisible(kWebVrPermanentHttpSecurityWarning));
  EXPECT_TRUE(IsVisible(kWebVrTransientHttpSecurityWarning));
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
  manager_->SetWebVrMode(false);
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

TEST_F(UiSceneManagerTest, UiUpdatesForFullscreenChanges) {
  std::set<UiElementDebugId> visible_in_browsing = {
      UiElementDebugId::kContentQuad, UiElementDebugId::kBackplane,
      UiElementDebugId::kCeiling,     UiElementDebugId::kFloor,
      UiElementDebugId::kUrlBar,      UiElementDebugId::kLoadingIndicator};
  std::set<UiElementDebugId> visible_in_fullscreen = {
      UiElementDebugId::kContentQuad, UiElementDebugId::kCloseButton,
      UiElementDebugId::kBackplane, UiElementDebugId::kCeiling,
      UiElementDebugId::kFloor};

  MakeManager(kNotInCct, kNotInWebVr);

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = scene_->GetWorldBackgroundColor();

  for (const auto& element : scene_->GetUiElements()) {
    SCOPED_TRACE(element->debug_id());
    bool should_be_visible = visible_in_browsing.find(element->debug_id()) !=
                             visible_in_browsing.end();
    EXPECT_EQ(should_be_visible, element->visible());
  }

  // Transistion to fullscreen.
  manager_->SetFullscreen(true);

  // Content elements should be visible, control elements should be hidden.
  for (const auto& element : scene_->GetUiElements()) {
    SCOPED_TRACE(element->debug_id());
    bool should_be_visible = visible_in_fullscreen.find(element->debug_id()) !=
                             visible_in_fullscreen.end();
    EXPECT_EQ(should_be_visible, element->visible());
  }

  {
    SCOPED_TRACE("Entered Fullsceen");
    // Make sure background has changed for fullscreen.
    EXPECT_NE(initial_background, scene_->GetWorldBackgroundColor());
  }

  // Exit fullscreen.
  manager_->SetFullscreen(false);

  // Everything should return to original state after leaving fullscreen.
  for (const auto& element : scene_->GetUiElements()) {
    SCOPED_TRACE(element->debug_id());
    bool should_be_visible = visible_in_browsing.find(element->debug_id()) !=
                             visible_in_browsing.end();
    EXPECT_EQ(should_be_visible, element->visible());
  }
  {
    SCOPED_TRACE("Exited Fullsceen");
    EXPECT_EQ(initial_background, scene_->GetWorldBackgroundColor());
  }
}

TEST_F(UiSceneManagerTest, UiUpdatesForWebVR) {
  MakeManager(kNotInCct, kInWebVr);

  manager_->SetWebVrSecureOrigin(true);
  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);

  // All elements should be hidden.
  for (const auto& element : scene_->GetUiElements()) {
    SCOPED_TRACE(element->debug_id());
    EXPECT_FALSE(element->visible());
  }
}

TEST_F(UiSceneManagerTest, UiUpdateTransitionToWebVR) {
  MakeManager(kNotInCct, kNotInWebVr);
  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);

  // Transition to WebVR mode
  manager_->SetWebVrMode(true);
  manager_->SetWebVrSecureOrigin(true);

  // All elements should be hidden.
  for (const auto& element : scene_->GetUiElements()) {
    SCOPED_TRACE(element->debug_id());
    EXPECT_FALSE(element->visible());
  }
}

TEST_F(UiSceneManagerTest, CaptureIndicatorsVisibility) {
  MakeManager(kNotInCct, kNotInWebVr);
  EXPECT_FALSE(IsVisible(kAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kVideoCaptureIndicator));
  EXPECT_FALSE(IsVisible(kScreenCaptureIndicator));

  manager_->SetAudioCapturingIndicator(true);
  manager_->SetVideoCapturingIndicator(true);
  manager_->SetScreenCapturingIndicator(true);

  EXPECT_TRUE(IsVisible(kAudioCaptureIndicator));
  EXPECT_TRUE(IsVisible(kVideoCaptureIndicator));
  EXPECT_TRUE(IsVisible(kScreenCaptureIndicator));

  manager_->SetWebVrMode(true);
  EXPECT_FALSE(IsVisible(kAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kVideoCaptureIndicator));
  EXPECT_FALSE(IsVisible(kScreenCaptureIndicator));

  manager_->SetWebVrMode(false);
  EXPECT_TRUE(IsVisible(kAudioCaptureIndicator));
  EXPECT_TRUE(IsVisible(kVideoCaptureIndicator));
  EXPECT_TRUE(IsVisible(kScreenCaptureIndicator));

  manager_->SetAudioCapturingIndicator(false);
  manager_->SetVideoCapturingIndicator(false);
  manager_->SetScreenCapturingIndicator(false);

  EXPECT_FALSE(IsVisible(kAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kVideoCaptureIndicator));
  EXPECT_FALSE(IsVisible(kScreenCaptureIndicator));
}
}  // namespace vr_shell
