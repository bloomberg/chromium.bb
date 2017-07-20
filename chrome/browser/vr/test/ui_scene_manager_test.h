// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_SCENE_MANAGER_TEST_H_
#define CHROME_BROWSER_VR_TEST_UI_SCENE_MANAGER_TEST_H_

#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "cc/trees/target_property.h"
#include "chrome/browser/vr/elements/ui_element_debug_id.h"
#include "chrome/browser/vr/test/mock_browser_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

class UiElement;
class UiScene;
class UiSceneManager;

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
  bool IsVisible(UiElementDebugId debug_id);

  // Verify that only the elements in the set are visible.
  void VerifyElementsVisible(const std::string& debug_name,
                             const std::set<UiElementDebugId>& debug_ids);

  // Return false if not all elements in the set match the specified visibility
  // state. Other elements are ignored.
  bool VerifyVisibility(const std::set<UiElementDebugId>& debug_ids,
                        bool visible);

  // Advances current_time_ by delta. This is done in frame increments and
  // UiScene::OnBeginFrame is called at each increment.
  void AnimateBy(base::TimeDelta delta);

  // Returns true if the given properties are being animated by the element.
  bool IsAnimating(UiElement* element,
                   const std::vector<cc::TargetProperty::Type>& properties);

  base::MessageLoop message_loop_;
  std::unique_ptr<MockBrowserInterface> browser_;
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<UiSceneManager> manager_;
  base::TimeTicks current_time_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_SCENE_MANAGER_TEST_H_
