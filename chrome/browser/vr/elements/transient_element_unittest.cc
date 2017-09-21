// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transient_element.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(TransientElementTest, Visibility) {
  TransientElement element(base::TimeDelta::FromSeconds(2));
  element.SetOpacity(0.0f);

  EXPECT_EQ(0.0f, element.opacity());

  // Enable and disable, making sure the element appears and disappears.
  element.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.SetVisible(false);
  EXPECT_EQ(0.0f, element.opacity());

  // Enable, and ensure that the element transiently disappears.
  element.SetVisible(true);
  element.OnBeginFrame(MsToTicks(10));
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.OnBeginFrame(MsToTicks(2010));
  EXPECT_EQ(0.0f, element.opacity());
}

// Test that refreshing the visibility resets the transience timeout if the
// element is currently visible.
TEST(TransientElementTest, RefreshVisibility) {
  TransientElement element(base::TimeDelta::FromSeconds(2));
  element.SetOpacity(0.0f);

  // Enable, and ensure that the element is visible.
  element.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.OnBeginFrame(MsToTicks(1000));

  // Refresh visibility, and ensure that the element still transiently
  // disappears, but at a later time.
  element.RefreshVisible();
  element.OnBeginFrame(MsToTicks(2000));
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.OnBeginFrame(MsToTicks(3000));
  EXPECT_EQ(0.0f, element.opacity());

  // Refresh visibility, and ensure that disabling hides the element.
  element.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.RefreshVisible();
  element.OnBeginFrame(MsToTicks(4000));
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.SetVisible(false);
  EXPECT_EQ(0.0f, element.opacity());
}

// Test that changing visibility on transient parent has the same effect on its
// children.
TEST(TransientElementTest, VisibilityChildren) {
  UiScene scene;
  // Create transient root.
  auto transient_element =
      base::MakeUnique<TransientElement>(base::TimeDelta::FromSeconds(2));
  TransientElement* parent = transient_element.get();
  transient_element->set_opacity_when_visible(0.5);
  transient_element->set_draw_phase(0);
  scene.AddUiElement(kRoot, std::move(transient_element));

  // Create child.
  auto element = base::MakeUnique<UiElement>();
  UiElement* child = element.get();
  element->set_opacity_when_visible(0.5);
  element->SetVisible(true);
  element->set_draw_phase(0);
  parent->AddChild(std::move(element));

  // Child hidden because parent is hidden.
  scene.OnBeginFrame(MsToTicks(0), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_FALSE(parent->IsVisible());

  // Setting visiblity on parent should make the child visible.
  parent->SetVisible(true);
  scene.OnBeginFrame(MsToTicks(10), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_TRUE(child->IsVisible());
  EXPECT_TRUE(parent->IsVisible());

  // Make sure the elements go away.
  scene.OnBeginFrame(MsToTicks(2010), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_FALSE(parent->IsVisible());

  // Test again, but this time manually set the visibility to false.
  parent->SetVisible(true);
  scene.OnBeginFrame(MsToTicks(2020), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_TRUE(child->IsVisible());
  EXPECT_TRUE(parent->IsVisible());
  parent->SetVisible(false);
  scene.OnBeginFrame(MsToTicks(2030), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_FALSE(parent->IsVisible());
}

}  // namespace vr
