// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_renderer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element.h"
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
    scene_->OnBeginFrame(MicrosecondsToTicks(1), kForwardVector);
  }
};

TEST_P(UiRendererTest, UiRendererSortingTest) {
  auto unsorted = ((*scene_).*GetParam().f)();
  auto sorted = UiRenderer::GetElementsInDrawOrder(unsorted);

  EXPECT_EQ(GetParam().expected_order.size(), sorted.size());

  for (size_t i = 0; i < sorted.size(); ++i) {
    EXPECT_EQ(GetParam().expected_order[i], sorted[i]->name());
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
         kExitPrompt,
         kExitPromptBackplane,
         kUrlBar,
         kUnderDevelopmentNotice,
         kCloseButton,
         kExclusiveScreenToast,
         kVoiceSearchButton,

     }},
    {&UiScene::GetVisible2dBrowsingOverlayElements,
     {
         kScreenDimmer, kExitWarning,
     }},
    {&UiScene::GetVisibleSplashScreenElements,
     {
         kSplashScreenBackground, kSplashScreenText,
     }},
    {&UiScene::GetVisibleWebVrOverlayForegroundElements,
     {
         kWebVrUrlToast, kExclusiveScreenToastViewportAware,
     }},
};

INSTANTIATE_TEST_CASE_P(SortingTests,
                        UiRendererTest,
                        testing::ValuesIn(params));

}  // namespace vr
