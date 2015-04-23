// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/scheduled_animation_group.h"

#include "components/view_manager/public/interfaces/animations.mojom.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/test_server_view_delegate.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/transform/transform_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"

using mojo::ANIMATION_PROPERTY_NONE;
using mojo::ANIMATION_PROPERTY_OPACITY;
using mojo::ANIMATION_PROPERTY_TRANSFORM;
using mojo::ANIMATION_TWEEN_TYPE_LINEAR;
using mojo::AnimationGroup;
using mojo::AnimationSequence;
using mojo::AnimationElement;
using mojo::AnimationValue;

namespace view_manager {
namespace {

bool IsAnimationGroupValid(const AnimationGroup& transport_group) {
  TestServerViewDelegate view_delegate;
  ServerView view(&view_delegate, ViewId());
  scoped_ptr<ScheduledAnimationGroup> group(ScheduledAnimationGroup::Create(
      &view, base::TimeTicks::Now(), 1, transport_group));
  return group.get() != nullptr;
}

}  // namespace

TEST(ScheduledAnimationGroupTest, IsAnimationGroupValid) {
  AnimationGroup group;

  // AnimationGroup with no sequences is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  group.sequences.push_back(AnimationSequence::New());

  // Sequence with no elements is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  AnimationSequence& sequence = *(group.sequences[0]);
  sequence.elements.push_back(AnimationElement::New());
  AnimationElement& element = *(sequence.elements[0]);
  element.property = ANIMATION_PROPERTY_OPACITY;
  element.tween_type = ANIMATION_TWEEN_TYPE_LINEAR;

  // Element with no target_value is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  // Opacity must be between 0 and 1.
  element.target_value = AnimationValue::New();
  element.target_value->float_value = 2.5f;
  EXPECT_FALSE(IsAnimationGroupValid(group));

  element.target_value->float_value = .5f;
  EXPECT_TRUE(IsAnimationGroupValid(group));

  // Bogus start value.
  element.start_value = AnimationValue::New();
  element.start_value->float_value = 2.5f;
  EXPECT_FALSE(IsAnimationGroupValid(group));

  element.start_value->float_value = .5f;
  EXPECT_TRUE(IsAnimationGroupValid(group));

  // Bogus transform.
  element.property = ANIMATION_PROPERTY_TRANSFORM;
  EXPECT_FALSE(IsAnimationGroupValid(group));
  element.start_value->transform = mojo::Transform::From(gfx::Transform());
  EXPECT_FALSE(IsAnimationGroupValid(group));
  element.target_value->transform = mojo::Transform::From(gfx::Transform());
  EXPECT_TRUE(IsAnimationGroupValid(group));

  // Add another empty sequence, should be invalid again.
  group.sequences.push_back(AnimationSequence::New());
  EXPECT_FALSE(IsAnimationGroupValid(group));

  AnimationSequence& sequence2 = *(group.sequences[1]);
  sequence2.elements.push_back(AnimationElement::New());
  AnimationElement& element2 = *(sequence2.elements[0]);
  element2.property = ANIMATION_PROPERTY_OPACITY;
  element2.tween_type = ANIMATION_TWEEN_TYPE_LINEAR;

  // Element with no target_value is not valid.
  EXPECT_FALSE(IsAnimationGroupValid(group));

  element2.property = ANIMATION_PROPERTY_NONE;
  EXPECT_TRUE(IsAnimationGroupValid(group));
}

}  // namespace view_manager
