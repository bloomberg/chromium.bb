// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_SCENE_MANAGER_TEST_H_
#define CHROME_BROWSER_VR_TEST_UI_SCENE_MANAGER_TEST_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/test/mock_browser_interface.h"
#include "chrome/browser/vr/test/mock_content_input_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class UiElement;
class UiScene;
class UiSceneManager;
struct Model;

class UiSceneManagerTest : public testing::Test {
 public:
  UiSceneManagerTest();
  ~UiSceneManagerTest() override;

  void SetUp() override;

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

  void MakeManager(InCct in_cct, InWebVr in_web_vr);
  void MakeAutoPresentedManager();

  bool IsVisible(UiElementName name) const;

  // Verify that only the elements in the set are visible.
  void VerifyElementsVisible(const std::string& debug_name,
                             const std::set<UiElementName>& names) const;

  // Return false if not all elements in the set match the specified visibility
  // state. Other elements are ignored.
  bool VerifyVisibility(const std::set<UiElementName>& names,
                        bool visible) const;

  // Return false if not all elements in the set match the specified requires
  // layout state. Other elements are ignored.
  bool VerifyRequiresLayout(const std::set<UiElementName>& names,
                            bool requires_layout) const;

  // Check if element is using correct opacity in Render recursively.
  void CheckRendererOpacityRecursive(UiElement* element);

  // Advances current_time_ by delta. This is done in frame increments and
  // UiScene::OnBeginFrame is called at each increment.
  bool AnimateBy(base::TimeDelta delta);

  // A wrapper to call scene_->OnBeginFrame.
  bool OnBeginFrame();

  void GetBackgroundColor(SkColor* background_color) const;

  std::unique_ptr<MockBrowserInterface> browser_;
  std::unique_ptr<MockContentInputDelegate> content_input_delegate_;
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<UiSceneManager> manager_;
  base::TimeTicks current_time_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_SCENE_MANAGER_TEST_H_
