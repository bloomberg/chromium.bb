// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "cc/animation/keyframe_model.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(UiElement, BoundsContainChildren) {
  auto parent = std::make_unique<UiElement>();
  parent->set_bounds_contain_children(true);
  parent->set_padding(0.1, 0.2);

  auto c1 = std::make_unique<UiElement>();
  c1->SetSize(3.0f, 3.0f);
  c1->SetTranslate(2.5f, 2.5f, 0.0f);
  auto* c1_ptr = c1.get();
  parent->AddChild(std::move(c1));

  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(2.5f, 2.5f, 3.2f, 3.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);
  EXPECT_EQ(parent->GetCenter().ToString(), c1_ptr->GetCenter().ToString());

  auto c2 = std::make_unique<UiElement>();
  c2->SetSize(4.0f, 4.0f);
  c2->SetTranslate(-3.0f, 0.0f, 0.0f);
  parent->AddChild(std::move(c2));

  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 1.0f, 9.2f, 6.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);

  auto c3 = std::make_unique<UiElement>();
  c3->SetSize(2.0f, 2.0f);
  c3->SetTranslate(0.0f, -2.0f, 0.0f);
  parent->AddChild(std::move(c3));

  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);

  auto c4 = std::make_unique<UiElement>();
  c4->SetSize(2.0f, 2.0f);
  c4->SetTranslate(20.0f, 20.0f, 0.0f);
  c4->SetVisible(false);
  parent->AddChild(std::move(c4));

  // We expect no change due to an invisible child.
  parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(-0.5f, 0.5f, 9.2f, 7.4f),
                   gfx::RectF(parent->local_origin(), parent->size()),
                   kEpsilon);

  auto grand_parent = std::make_unique<UiElement>();
  grand_parent->set_bounds_contain_children(true);
  grand_parent->set_padding(0.1, 0.2);
  grand_parent->AddChild(std::move(parent));

  auto anchored = std::make_unique<UiElement>();
  anchored->set_y_anchoring(BOTTOM);
  anchored->set_contributes_to_parent_bounds(false);

  auto* anchored_ptr = anchored.get();
  grand_parent->AddChild(std::move(anchored));

  grand_parent->DoLayOutChildren();
  EXPECT_RECT_NEAR(
      gfx::RectF(-0.5f, 0.5f, 9.4f, 7.8f),
      gfx::RectF(grand_parent->local_origin(), grand_parent->size()), kEpsilon);

  gfx::Point3F p;
  anchored_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_FLOAT_EQ(-3.9, p.y());
}

TEST(UiElement, IgnoringAsymmetricPadding) {
  // This test ensures that when we ignore asymmetric padding that we don't
  // accidentally shift the location of the parent; it should stay put.
  auto a = std::make_unique<UiElement>();
  a->set_bounds_contain_children(true);

  auto b = std::make_unique<UiElement>();
  b->set_bounds_contain_children(true);
  b->set_bounds_contain_padding(false);
  b->set_padding(0.0f, 5.0f, 0.0f, 0.0f);

  auto c = std::make_unique<UiElement>();
  c->set_bounds_contain_children(true);
  c->set_bounds_contain_padding(false);
  c->set_padding(0.0f, 2.0f, 0.0f, 0.0f);

  auto d = std::make_unique<UiElement>();
  d->SetSize(0.5f, 0.5f);

  c->AddChild(std::move(d));
  c->DoLayOutChildren();
  b->AddChild(std::move(c));
  b->DoLayOutChildren();
  a->AddChild(std::move(b));
  a->DoLayOutChildren();

  a->UpdateWorldSpaceTransformRecursive(false);

  gfx::Point3F p;
  a->world_space_transform().TransformPoint(&p);

  EXPECT_VECTOR3DF_EQ(gfx::Point3F(), p);
}

TEST(UiElement, BoundsContainScaledChildren) {
  auto a = std::make_unique<UiElement>();
  a->SetSize(0.4, 0.3);

  auto b = std::make_unique<UiElement>();
  b->SetSize(0.2, 0.2);
  b->SetTranslate(0.6, 0.0, 0.0);
  b->SetScale(2.0, 3.0, 1.0);

  auto c = std::make_unique<UiElement>();
  c->set_bounds_contain_children(true);
  c->set_padding(0.05, 0.05);
  c->AddChild(std::move(a));
  c->AddChild(std::move(b));

  c->DoLayOutChildren();
  EXPECT_RECT_NEAR(gfx::RectF(0.3f, 0.0f, 1.1f, 0.7f),
                   gfx::RectF(c->local_origin(), c->size()), kEpsilon);
}

TEST(UiElement, AnimateSize) {
  UiScene scene;
  auto rect = std::make_unique<UiElement>();
  rect->SetSize(10, 100);
  rect->AddKeyframeModel(CreateBoundsAnimation(1, 1, gfx::SizeF(10, 100),
                                               gfx::SizeF(20, 200),
                                               MicrosecondsToDelta(10000)));
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));
  base::TimeTicks start_time = MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kStartHeadPose));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(10, 100), rect_ptr->size());
  EXPECT_TRUE(scene.OnBeginFrame(start_time + MicrosecondsToDelta(10000),
                                 kStartHeadPose));
  EXPECT_FLOAT_SIZE_EQ(gfx::SizeF(20, 200), rect_ptr->size());
}

TEST(UiElement, AnimationAffectsInheritableTransform) {
  UiScene scene;
  auto rect = std::make_unique<UiElement>();
  UiElement* rect_ptr = rect.get();
  scene.AddUiElement(kRoot, std::move(rect));

  cc::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  cc::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  rect_ptr->AddKeyframeModel(CreateTransformAnimation(
      2, 2, from_operations, to_operations, MicrosecondsToDelta(10000)));

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  EXPECT_TRUE(scene.OnBeginFrame(start_time, kStartHeadPose));
  gfx::Point3F p;
  rect_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(10, 100, 1000), p);
  p = gfx::Point3F();
  EXPECT_TRUE(scene.OnBeginFrame(start_time + MicrosecondsToDelta(10000),
                                 kStartHeadPose));
  rect_ptr->LocalTransform().TransformPoint(&p);
  EXPECT_VECTOR3DF_EQ(gfx::Vector3dF(20, 200, 2000), p);
}

TEST(UiElement, HitTest) {
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
    event_handlers.hover_enter = base::BindRepeating(
        &ElementEventHandlers::HandleHoverEnter, base::Unretained(this));
    event_handlers.hover_move = base::BindRepeating(
        &ElementEventHandlers::HandleHoverMove, base::Unretained(this));
    event_handlers.hover_leave = base::BindRepeating(
        &ElementEventHandlers::HandleHoverLeave, base::Unretained(this));
    event_handlers.button_down = base::BindRepeating(
        &ElementEventHandlers::HandleButtonDown, base::Unretained(this));
    event_handlers.button_up = base::BindRepeating(
        &ElementEventHandlers::HandleButtonUp, base::Unretained(this));
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
  auto element = std::make_unique<UiElement>();
  auto child = std::make_unique<UiElement>();
  auto grand_child = std::make_unique<UiElement>();
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
