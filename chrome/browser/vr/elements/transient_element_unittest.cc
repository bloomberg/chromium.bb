// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transient_element.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(SimpleTransientElementTest, Visibility) {
  SimpleTransientElement element(base::TimeDelta::FromSeconds(2));
  element.SetOpacity(0.0f);

  EXPECT_EQ(0.0f, element.opacity());

  // Enable and disable, making sure the element appears and disappears.
  element.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.SetVisible(false);
  EXPECT_EQ(0.0f, element.opacity());

  // Enable, and ensure that the element transiently disappears.
  element.SetVisible(true);
  EXPECT_FALSE(element.DoBeginFrame(MsToTicks(10), kForwardVector));
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  EXPECT_TRUE(element.DoBeginFrame(MsToTicks(2010), kForwardVector));
  EXPECT_EQ(0.0f, element.opacity());
}

// Test that refreshing the visibility resets the transience timeout if the
// element is currently visible.
TEST(SimpleTransientElementTest, RefreshVisibility) {
  SimpleTransientElement element(base::TimeDelta::FromSeconds(2));
  element.SetOpacity(0.0f);

  // Enable, and ensure that the element is visible.
  element.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  EXPECT_FALSE(element.DoBeginFrame(MsToTicks(1000), kForwardVector));

  // Refresh visibility, and ensure that the element still transiently
  // disappears, but at a later time.
  element.RefreshVisible();
  EXPECT_FALSE(element.DoBeginFrame(MsToTicks(2000), kForwardVector));
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  EXPECT_TRUE(element.DoBeginFrame(MsToTicks(3000), kForwardVector));
  EXPECT_EQ(0.0f, element.opacity());

  // Refresh visibility, and ensure that disabling hides the element.
  element.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.RefreshVisible();
  EXPECT_FALSE(element.DoBeginFrame(MsToTicks(4000), kForwardVector));
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  element.SetVisible(false);
  EXPECT_EQ(0.0f, element.opacity());
}

// Test that changing visibility on transient parent has the same effect on its
// children.
// TODO(mthiesse, vollick): Convert this test to use bindings.
TEST(SimpleTransientElementTest, VisibilityChildren) {
  UiScene scene;
  // Create transient root.
  auto transient_element =
      base::MakeUnique<SimpleTransientElement>(base::TimeDelta::FromSeconds(2));
  SimpleTransientElement* parent = transient_element.get();
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
  EXPECT_TRUE(scene.OnBeginFrame(MsToTicks(0), kForwardVector));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_FALSE(parent->IsVisible());

  // Setting visiblity on parent should make the child visible.
  parent->SetVisible(true);
  scene.set_dirty();
  EXPECT_TRUE(scene.OnBeginFrame(MsToTicks(10), kForwardVector));
  EXPECT_TRUE(child->IsVisible());
  EXPECT_TRUE(parent->IsVisible());

  // Make sure the elements go away.
  EXPECT_TRUE(scene.OnBeginFrame(MsToTicks(2010), kForwardVector));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_FALSE(parent->IsVisible());

  // Test again, but this time manually set the visibility to false.
  parent->SetVisible(true);
  scene.set_dirty();
  EXPECT_TRUE(scene.OnBeginFrame(MsToTicks(2020), kForwardVector));
  EXPECT_TRUE(child->IsVisible());
  EXPECT_TRUE(parent->IsVisible());
  parent->SetVisible(false);
  scene.set_dirty();
  EXPECT_TRUE(scene.OnBeginFrame(MsToTicks(2030), kForwardVector));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_FALSE(parent->IsVisible());
}

class ShowUntilSignalElementTest : public testing::Test {
 public:
  ShowUntilSignalElementTest() {}

  void SetUp() override {
    callback_triggered_ = false;
    element_ = base::MakeUnique<ShowUntilSignalTransientElement>(
        base::TimeDelta::FromSeconds(2), base::TimeDelta::FromSeconds(5),
        base::Bind(&ShowUntilSignalElementTest::OnTimeout,
                   base::Unretained(this)));
  }

  ShowUntilSignalTransientElement& element() { return *element_; }
  TransientElementHideReason hide_reason() { return hide_reason_; }
  bool callback_triggered() { return callback_triggered_; }

  void OnTimeout(TransientElementHideReason reason) {
    callback_triggered_ = true;
    hide_reason_ = reason;
  }

 private:
  bool callback_triggered_ = false;
  TransientElementHideReason hide_reason_;
  std::unique_ptr<ShowUntilSignalTransientElement> element_;
};

// Test that the element disappears when signalled.
TEST_F(ShowUntilSignalElementTest, ElementHidesAfterSignal) {
  EXPECT_FALSE(element().IsVisible());

  // Make element visible.
  element().SetVisible(true);
  EXPECT_FALSE(element().DoBeginFrame(MsToTicks(10), kForwardVector));
  EXPECT_EQ(element().opacity_when_visible(), element().opacity());

  // Signal, element should still be visible since time < min duration.
  element().Signal();
  EXPECT_FALSE(element().DoBeginFrame(MsToTicks(200), kForwardVector));
  EXPECT_EQ(element().opacity_when_visible(), element().opacity());

  // Element hides and callback triggered.
  EXPECT_TRUE(element().DoBeginFrame(MsToTicks(2010), kForwardVector));
  EXPECT_EQ(0.0f, element().opacity());
  EXPECT_TRUE(callback_triggered());
  EXPECT_EQ(TransientElementHideReason::kSignal, hide_reason());
}

// Test that the transient element times out.
TEST_F(ShowUntilSignalElementTest, TimedOut) {
  EXPECT_FALSE(element().IsVisible());

  // Make element visible.
  element().SetVisible(true);
  EXPECT_FALSE(element().DoBeginFrame(MsToTicks(10), kForwardVector));
  EXPECT_EQ(element().opacity_when_visible(), element().opacity());

  // Element should be visible since we haven't signalled.
  EXPECT_FALSE(element().DoBeginFrame(MsToTicks(2010), kForwardVector));
  EXPECT_EQ(element().opacity_when_visible(), element().opacity());

  // Element hides and callback triggered.
  EXPECT_TRUE(element().DoBeginFrame(MsToTicks(6010), kForwardVector));
  EXPECT_EQ(0.0f, element().opacity());
  EXPECT_TRUE(callback_triggered());
  EXPECT_EQ(TransientElementHideReason::kTimeout, hide_reason());
}

// Test that refreshing the visibility resets the transience timeout if the
// element is currently visible.
TEST_F(ShowUntilSignalElementTest, RefreshVisibility) {
  // Enable, and ensure that the element is visible.
  element().SetVisible(true);
  EXPECT_EQ(element().opacity_when_visible(), element().opacity());
  EXPECT_FALSE(element().DoBeginFrame(MsToTicks(1000), kForwardVector));
  element().Signal();

  // Refresh visibility, and ensure that the element still transiently
  // disappears, but at a later time.
  element().RefreshVisible();
  EXPECT_FALSE(element().DoBeginFrame(MsToTicks(2500), kForwardVector));
  EXPECT_EQ(element().opacity_when_visible(), element().opacity());
  EXPECT_TRUE(element().DoBeginFrame(MsToTicks(3000), kForwardVector));
  EXPECT_EQ(0.0f, element().opacity());
  EXPECT_EQ(TransientElementHideReason::kSignal, hide_reason());
}

}  // namespace vr
