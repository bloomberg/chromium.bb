// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/animation.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(UiElement, BoundsContainChildren) {
  const float epsilon = 1e-3f;
  auto parent = base::MakeUnique<UiElement>();
  parent->set_bounds_contain_children(true);
  parent->set_padding(0.1, 0.2);

  auto c1 = base::MakeUnique<UiElement>();
  c1->SetSize(3.0f, 3.0f);
  c1->SetTranslate(2.5f, 2.5f, 0.0f);
  auto* c1_ptr = c1.get();
  parent->AddChild(std::move(c1));

  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(2.5f, 2.5f, 3.2f, 3.4f),
                   gfx::RectF(parent->local_origin(), parent->size()), epsilon);
  EXPECT_EQ(parent->GetCenter().ToString(), c1_ptr->GetCenter().ToString());

  auto c2 = base::MakeUnique<UiElement>();
  c2->SetSize(4.0f, 4.0f);
  c2->SetTranslate(-3.0f, 0.0f, 0.0f);
  parent->AddChild(std::move(c2));

  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 1.0f, 9.2f, 6.4f),
                   gfx::RectF(parent->local_origin(), parent->size()), epsilon);

  auto c3 = base::MakeUnique<UiElement>();
  c3->SetSize(2.0f, 2.0f);
  c3->SetTranslate(0.0f, -2.0f, 0.0f);
  parent->AddChild(std::move(c3));

  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                   gfx::RectF(parent->local_origin(), parent->size()), epsilon);

  auto c4 = base::MakeUnique<UiElement>();
  c4->SetSize(2.0f, 2.0f);
  c4->SetTranslate(20.0f, 20.0f, 0.0f);
  c4->SetVisible(false);
  parent->AddChild(std::move(c4));

  // We expect no change due to an invisible child.
  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                   gfx::RectF(parent->local_origin(), parent->size()), epsilon);
}

TEST(UiElements, AnimateSize) {
  UiScene scene;
  auto rect = base::MakeUnique<UiElement>();
  rect->SetSize(10, 100);
  rect->AddAnimation(CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100),
                                           gfx::SizeF(20, 200),
                                           MicrosecondsToDelta(10000)));
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));
  base::TimeTicks start_time = MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kForwardVector));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(10, 100), rect_ptr->size());
  EXPECT_TRUE(scene.OnBeginFrame(start_time + MicrosecondsToDelta(10000),
                                 kForwardVector));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(20, 200), rect_ptr->size());
}

TEST(UiElements, AnimationAffectsInheritableTransform) {
  UiScene scene;
  auto rect = base::MakeUnique<UiElement>();
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  rect_ptr->AddAnimation(CreateTransformAnimation(
      2, 2, from_operations, to_operations, MicrosecondsToDelta(10000)));

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kForwardVector));
  gfx::Point3F p;
  rect_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(10, 100, 1000), p);
  p = gfx::Point3F();
  EXPECT_TRUE(scene.OnBeginFrame(start_time + MicrosecondsToDelta(10000),
                                 kForwardVector));
  rect_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(20, 200, 2000), p);
}

TEST(UiElements, HitTest) {
  UiElement rect;
  rect.SetSize(1.0, 1.0);

  UiElement circle;
  circle.SetSize(1.0, 1.0);
  circle.set_corner_radius(1.0 / 2);

  UiElement rounded_rect;
  rounded_rect.SetSize(1.0, 0.5);
  rounded_rect.set_corner_radius(0.2);

  struct {
    gfx::PointF location;
    bool expected_rect;
    bool expected_circle;
    bool expected_rounded_rect;
  } test_cases[] = {
      // Walk left edge
      {gfx::PointF(0.f, 0.1f), true, false, false},
      {gfx::PointF(0.f, 0.45f), true, false, true},
      {gfx::PointF(0.f, 0.55f), true, false, true},
      {gfx::PointF(0.f, 0.95f), true, false, false},
      {gfx::PointF(0.f, 1.f), true, false, false},
      // Walk bottom edge
      {gfx::PointF(0.1f, 1.f), true, false, false},
      {gfx::PointF(0.45f, 1.f), true, false, true},
      {gfx::PointF(0.55f, 1.f), true, false, true},
      {gfx::PointF(0.95f, 1.f), true, false, false},
      {gfx::PointF(1.0f, 1.f), true, false, false},
      // Walk right edge
      {gfx::PointF(1.f, 0.95f), true, false, false},
      {gfx::PointF(1.f, 0.55f), true, false, true},
      {gfx::PointF(1.f, 0.45f), true, false, true},
      {gfx::PointF(1.f, 0.1f), true, false, false},
      {gfx::PointF(1.f, 0.f), true, false, false},
      // Walk top edge
      {gfx::PointF(0.95f, 0.f), true, false, false},
      {gfx::PointF(0.55f, 0.f), true, false, true},
      {gfx::PointF(0.45f, 0.f), true, false, true},
      {gfx::PointF(0.1f, 0.f), true, false, false},
      {gfx::PointF(0.f, 0.f), true, false, false},
      // center
      {gfx::PointF(0.5f, 0.5f), true, true, true},
      // A point which is included in rounded rect but not in cicle.
      {gfx::PointF(0.1f, 0.1f), true, false, true},
      // An invalid point.
      {gfx::PointF(-0.1f, -0.1f), false, false, false},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected_rect,
              rect.LocalHitTest(test_cases[i].location));
    EXPECT_EQ(test_cases[i].expected_circle,
              circle.LocalHitTest(test_cases[i].location));
    EXPECT_EQ(test_cases[i].expected_rounded_rect,
              rounded_rect.LocalHitTest(test_cases[i].location));
  }
}

class ElementEventHandlers {
 public:
  explicit ElementEventHandlers(UiElement* element) {
    DCHECK(element);
    EventHandlers event_handlers;
    event_handlers.hover_enter = base::Bind(
        &ElementEventHandlers::HandleHoverEnter, base::Unretained(this));
    event_handlers.hover_move = base::Bind(
        &ElementEventHandlers::HandleHoverMove, base::Unretained(this));
    event_handlers.hover_leave = base::Bind(
        &ElementEventHandlers::HandleHoverLeave, base::Unretained(this));
    event_handlers.button_down = base::Bind(
        &ElementEventHandlers::HandleButtonDown, base::Unretained(this));
    event_handlers.button_up = base::Bind(&ElementEventHandlers::HandleButtonUp,
                                          base::Unretained(this));
    element->set_event_handlers(event_handlers);
  }
  void HandleHoverEnter() { hover_enter_ = true; }
  bool hover_enter_called() { return hover_enter_; }

  void HandleHoverMove(const gfx::PointF& position) { hover_move_ = true; }
  bool hover_move_called() { return hover_move_; }

  void HandleHoverLeave() { hover_leave_ = true; }
  bool hover_leave_called() { return hover_leave_; }

  void HandleButtonDown() { button_down_ = true; }
  bool button_down_called() { return button_down_; }

  void HandleButtonUp() { button_up_ = true; }
  bool button_up_called() { return button_up_; }

  void ExpectCalled(bool called) {
    EXPECT_EQ(hover_enter_called(), called);
    EXPECT_EQ(hover_move_called(), called);
    EXPECT_EQ(hover_leave_called(), called);
    EXPECT_EQ(button_down_called(), called);
    EXPECT_EQ(button_up_called(), called);
  }

 private:
  bool hover_enter_ = false;
  bool hover_move_ = false;
  bool hover_leave_ = false;
  bool button_up_ = false;
  bool button_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(ElementEventHandlers);
};

TEST(UiElement, EventBubbling) {
  auto element = base::MakeUnique<UiElement>();
  auto child = base::MakeUnique<UiElement>();
  auto grand_child = base::MakeUnique<UiElement>();
  auto* child_ptr = child.get();
  auto* grand_child_ptr = grand_child.get();
  child->AddChild(std::move(grand_child));
  element->AddChild(std::move(child));

  // Add event handlers to element and child.
  ElementEventHandlers element_handlers(element.get());
  ElementEventHandlers child_handlers(child_ptr);

  // Events on grand_child don't bubble up the parent chain.
  grand_child_ptr->OnHoverEnter(gfx::PointF());
  grand_child_ptr->OnMove(gfx::PointF());
  grand_child_ptr->OnHoverLeave();
  grand_child_ptr->OnButtonDown(gfx::PointF());
  grand_child_ptr->OnButtonUp(gfx::PointF());
  child_handlers.ExpectCalled(false);
  element_handlers.ExpectCalled(false);

  // Events on grand_child bubble up the parent chain.
  grand_child_ptr->set_bubble_events(true);
  grand_child_ptr->OnHoverEnter(gfx::PointF());
  grand_child_ptr->OnMove(gfx::PointF());
  grand_child_ptr->OnHoverLeave();
  grand_child_ptr->OnButtonDown(gfx::PointF());
  grand_child_ptr->OnButtonUp(gfx::PointF());
  child_handlers.ExpectCalled(true);
  // Events don't bubble to element since it doesn't have the bubble_events bit
  // set.
  element_handlers.ExpectCalled(false);
}

}  // namespace vr
