// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_scene_manager_test.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"

namespace vr {

UiSceneManagerTest::UiSceneManagerTest() {}
UiSceneManagerTest::~UiSceneManagerTest() {}

void UiSceneManagerTest::SetUp() {
  browser_ = base::MakeUnique<MockBrowserInterface>();
}

void UiSceneManagerTest::MakeManager(InCct in_cct, InWebVr in_web_vr) {
  scene_ = base::MakeUnique<UiScene>();
  manager_ = base::MakeUnique<UiSceneManager>(
      browser_.get(), scene_.get(), in_cct, in_web_vr, kNotAutopresented);
}

void UiSceneManagerTest::MakeAutoPresentedManager() {
  scene_ = base::MakeUnique<UiScene>();
  manager_ = base::MakeUnique<UiSceneManager>(
      browser_.get(), scene_.get(), kNotInCct, kNotInWebVr, kAutopresented);
}

bool UiSceneManagerTest::IsVisible(UiElementDebugId debug_id) {
  UiElement* element = scene_->GetUiElementByDebugId(debug_id);
  return element ? element->visible() : false;
}

void UiSceneManagerTest::VerifyElementsVisible(
    const std::string& debug_name,
    const std::set<UiElementDebugId>& debug_ids) {
  SCOPED_TRACE(debug_name);
  for (const auto& element : scene_->GetUiElements()) {
    SCOPED_TRACE(element->debug_id());
    bool should_be_visible =
        debug_ids.find(element->debug_id()) != debug_ids.end();
    EXPECT_EQ(should_be_visible, element->visible());
  }
}

bool UiSceneManagerTest::VerifyVisibility(
    const std::set<UiElementDebugId>& debug_ids,
    bool visible) {
  for (const auto& element : scene_->GetUiElements()) {
    if (debug_ids.find(element->debug_id()) != debug_ids.end() &&
        element->visible() != visible) {
      return false;
    }
  }
  return true;
}

void UiSceneManagerTest::AnimateBy(base::TimeDelta delta) {
  base::TimeTicks target_time = current_time_ + delta;
  base::TimeDelta frame_time = base::TimeDelta::FromSecondsD(1.0 / 60.0);
  for (; current_time_ < target_time; current_time_ += frame_time) {
    scene_->OnBeginFrame(current_time_);
  }
  current_time_ = target_time;
  scene_->OnBeginFrame(current_time_);
}

bool UiSceneManagerTest::IsAnimating(
    UiElement* element,
    const std::vector<cc::TargetProperty::Type>& properties) {
  for (auto property : properties) {
    if (!element->animation_player().IsAnimatingProperty(property))
      return false;
  }
  return true;
}

}  // namespace vr
