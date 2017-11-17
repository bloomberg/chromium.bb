// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_renderer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"

namespace vr {

typedef UiScene::Elements (UiScene::*GeneratorFn)() const;
struct TestParams {
  GeneratorFn f;
  std::vector<UiElementName> expected_order;
};

class UiRendererTest : public UiSceneManagerTest,
                       public ::testing::WithParamInterface<TestParams> {
 public:
  void SetUp() override {
    UiSceneManagerTest::SetUp();
    MakeManager(kNotInCct, kNotInWebVr);
    for (auto& e : scene_->root_element()) {
      e.SetTransitionedProperties({});
      e.SetVisible(true);
    }
  }
};

TEST_F(UiRendererTest, ReticleStacking) {
  UiElement* content = scene_->GetUiElementByName(kContentQuad);
  EXPECT_TRUE(content);
  model_->reticle.target_element_id = content->id();
  auto unsorted = scene_->GetVisible2dBrowsingElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  bool saw_target = false;
  for (auto* e : sorted) {
    if (e == content) {
      saw_target = true;
    } else if (saw_target) {
      EXPECT_EQ(kReticle, e->name());
      break;
    }
  }

  EXPECT_TRUE(saw_target);

  auto controller_elements = scene_->GetVisibleControllerElements();
  bool saw_reticle = false;
  for (auto* e : controller_elements) {
    if (e->name() == kReticle) {
      saw_reticle = true;
    }
  }
  EXPECT_FALSE(saw_reticle);
}

TEST_F(UiRendererTest, ReticleStackingAtopForeground) {
  UiElement* element = scene_->GetUiElementByName(kContentQuad);
  EXPECT_TRUE(element);
  element->set_draw_phase(kPhaseOverlayForeground);
  model_->reticle.target_element_id = element->id();
  auto unsorted = scene_->GetVisible2dBrowsingOverlayElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  bool saw_target = false;
  for (auto* e : sorted) {
    if (e == element) {
      saw_target = true;
    } else if (saw_target) {
      EXPECT_EQ(kReticle, e->name());
      break;
    }
  }
  EXPECT_TRUE(saw_target);

  auto controller_elements = scene_->GetVisibleControllerElements();
  bool saw_reticle = false;
  for (auto* e : controller_elements) {
    if (e->name() == kReticle) {
      saw_reticle = true;
    }
  }
  EXPECT_FALSE(saw_reticle);
}

TEST_F(UiRendererTest, ReticleStackingWithControllerElements) {
  UiElement* element = scene_->GetUiElementByName(kReticle);
  EXPECT_TRUE(element);
  element->set_draw_phase(kPhaseBackground);
  EXPECT_NE(scene_->GetUiElementByName(kLaser)->draw_phase(),
            element->draw_phase());
  model_->reticle.target_element_id = 0;
  auto unsorted = scene_->GetVisibleControllerElements();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);
  EXPECT_EQ(element->DebugName(), sorted.back()->DebugName());
  EXPECT_EQ(scene_->GetUiElementByName(kLaser)->draw_phase(),
            element->draw_phase());
}

TEST_P(UiRendererTest, UiRendererSortingTest) {
  model_->reticle.target_element_id = 0;
  auto unsorted = ((*scene_).*GetParam().f)();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);

  // Filter elements with no name. These elements usually created in ctor of
  // their root element. See button.cc for example.
  // We shouldn't need this once crbug.com/782395 is fixed.
  sorted.erase(
      std::remove_if(sorted.begin(), sorted.end(),
                     [](const UiElement* e) { return e->name() == kNone; }),
      sorted.end());

  EXPECT_EQ(GetParam().expected_order.size(), sorted.size());

  for (size_t i = 0; i < sorted.size(); ++i) {
    EXPECT_NE(0, sorted[i]->name());
    EXPECT_EQ(UiElementNameToString(GetParam().expected_order[i]),
              sorted[i]->DebugName());
  }
}

TestParams params[] = {
    {&UiScene::GetVisible2dBrowsingElements,
     {
         kBackgroundFront,
         kBackgroundLeft,
         kBackgroundBack,
         kBackgroundRight,
         kBackgroundTop,
         kBackgroundBottom,
         kFloor,
         kCeiling,
         kBackplane,
         kContentQuad,
         kAudioCaptureIndicator,
         kVideoCaptureIndicator,
         kScreenCaptureIndicator,
         kBluetoothConnectedIndicator,
         kLocationAccessIndicator,
         kUrlBar,
         kLoadingIndicator,
         kLoadingIndicatorForeground,
         kUnderDevelopmentNotice,
         kVoiceSearchButton,
         kCloseButton,
         kExclusiveScreenToast,
         kExitPromptBackplane,
         kExitPrompt,
         kAudioPermissionPromptBackplane,
         kAudioPermissionPromptShadow,
         kAudioPermissionPrompt,
         kSpeechRecognitionResultText,
         kSpeechRecognitionResultCircle,
         kSpeechRecognitionResultMicrophoneIcon,
         kSpeechRecognitionResultBackplane,
         kSpeechRecognitionListeningGrowingCircle,
         kSpeechRecognitionListeningInnerCircle,
         kSpeechRecognitionListeningMicrophoneIcon,
         kSpeechRecognitionListeningBackplane,
     }},
    {&UiScene::GetVisible2dBrowsingOverlayElements,
     {
         kScreenDimmer, kExitWarning,
     }},
    {&UiScene::GetVisibleSplashScreenElements,
     {
         kSplashScreenBackground, kWebVrTimeoutSpinnerBackground,
         kSplashScreenText, kWebVrTimeoutSpinner, kWebVrTimeoutMessage,
         kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
         kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText,
     }},
    {&UiScene::GetVisibleWebVrOverlayForegroundElements,
     {
         kWebVrUrlToast, kExclusiveScreenToastViewportAware,
     }},
    {&UiScene::GetVisibleControllerElements,
     {
         kController, kLaser, kReticle,
     }},
};

INSTANTIATE_TEST_CASE_P(SortingTests,
                        UiRendererTest,
                        testing::ValuesIn(params));

}  // namespace vr
