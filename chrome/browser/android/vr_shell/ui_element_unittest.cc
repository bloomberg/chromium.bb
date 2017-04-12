// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_element.h"

#include <utility>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "device/vr/vr_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_VEC3F_EQ(a, b)    \
  EXPECT_FLOAT_EQ(a.x(), b.x()); \
  EXPECT_FLOAT_EQ(a.y(), b.y()); \
  EXPECT_FLOAT_EQ(a.z(), b.z());

#define EXPECT_RECTF_EQ(a, b)            \
  EXPECT_FLOAT_EQ(a.x(), b.x());         \
  EXPECT_FLOAT_EQ(a.y(), b.y());         \
  EXPECT_FLOAT_EQ(a.width(), b.width()); \
  EXPECT_FLOAT_EQ(a.height(), b.height());

#define EXPECT_ROTATION(a, b) \
  EXPECT_FLOAT_EQ(a.x, b.x);  \
  EXPECT_FLOAT_EQ(a.y, b.y);  \
  EXPECT_FLOAT_EQ(a.z, b.z);  \
  EXPECT_FLOAT_EQ(a.angle, b.angle);

namespace vr_shell {

namespace {

base::TimeTicks usToTicks(uint64_t us) {
  return base::TimeTicks::FromInternalValue(us);
}

base::TimeDelta usToDelta(uint64_t us) {
  return base::TimeDelta::FromInternalValue(us);
}

}  // namespace

TEST(UiElements, AnimateCopyRect) {
  UiElement rect;
  rect.copy_rect = {10, 100, 1000, 10000};
  std::unique_ptr<Animation> animation(new Animation(
      0, Animation::Property::COPYRECT,
      std::unique_ptr<easing::Easing>(new easing::Linear()), {},
      {20, 200, 2000, 20000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(50000));
  EXPECT_RECTF_EQ(rect.copy_rect, gfx::RectF(10, 100, 1000, 10000));
  rect.Animate(usToTicks(60000));
  EXPECT_RECTF_EQ(rect.copy_rect, gfx::RectF(20, 200, 2000, 20000));
}

TEST(UiElements, AnimateSize) {
  UiElement rect;
  rect.size = {10, 100, 1};
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::SIZE,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(50000));
  EXPECT_VEC3F_EQ(rect.size, gfx::Vector3dF(10, 100, 1));
  rect.Animate(usToTicks(60000));
  EXPECT_VEC3F_EQ(rect.size, gfx::Vector3dF(20, 200, 1));
}

TEST(UiElements, AnimateTranslation) {
  UiElement rect;
  rect.translation = {10, 100, 1000};
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200, 2000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(50000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(10, 100, 1000));
  rect.Animate(usToTicks(60000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(20, 200, 2000));
}

TEST(UiElements, AnimateRotation) {
  UiElement rect;
  rect.rotation = {10, 100, 1000, 10000};
  std::unique_ptr<Animation> animation(new Animation(
      0, Animation::Property::ROTATION,
      std::unique_ptr<easing::Easing>(new easing::Linear()), {},
      {20, 200, 2000, 20000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(50000));
  EXPECT_ROTATION(rect.rotation, vr::RotationAxisAngle({10, 100, 1000, 10000}));
  rect.Animate(usToTicks(60000));
  EXPECT_ROTATION(rect.rotation, vr::RotationAxisAngle({20, 200, 2000, 20000}));
}

TEST(UiElements, AnimationHasNoEffectBeforeScheduledStart) {
  UiElement rect;
  std::unique_ptr<Animation> animation(new Animation(
      0, Animation::Property::TRANSLATION,
      std::unique_ptr<easing::Easing>(new easing::Linear()), {10, 100, 1000},
      {20, 200, 2000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(49999));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(0, 0, 0));
}

TEST(UiElements, AnimationPurgedWhenDone) {
  UiElement rect;
  std::unique_ptr<Animation> animation(new Animation(
      0, Animation::Property::TRANSLATION,
      std::unique_ptr<easing::Easing>(new easing::Linear()), {10, 100, 1000},
      {20, 200, 2000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(60000));
  EXPECT_EQ(0u, rect.animations.size());
}

TEST(UiElements, AnimationLinearEasing) {
  UiElement rect;
  std::unique_ptr<Animation> animation(new Animation(
      0, Animation::Property::TRANSLATION,
      std::unique_ptr<easing::Easing>(new easing::Linear()), {10, 100, 1000},
      {20, 200, 2000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(50000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(10, 100, 1000));
  rect.Animate(usToTicks(55000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(15, 150, 1500));
  rect.Animate(usToTicks(60000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(20, 200, 2000));
}

TEST(UiElements, AnimationStartFromSpecifiedLocation) {
  UiElement rect;
  std::unique_ptr<Animation> animation(new Animation(
      0, Animation::Property::TRANSLATION,
      std::unique_ptr<easing::Easing>(new easing::Linear()), {10, 100, 1000},
      {20, 200, 2000}, usToTicks(50000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(usToTicks(50000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(10, 100, 1000));
  rect.Animate(usToTicks(60000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(20, 200, 2000));
}

// Ensure that when a new animation overlaps another of the same type, the
// newly added animation overrides the original.  For example:
//   Animation 1:  ? .......... 20
//   Animation 2:        ?  .......... 50
//   Result:       0 ... 10 ... 30 ... 50
TEST(UiElements, AnimationOverlap) {
  UiElement rect;
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200, 2000}, usToTicks(50000), usToDelta(10000)));
  std::unique_ptr<Animation> animation2(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {50, 500, 5000}, usToTicks(55000), usToDelta(10000)));
  rect.animations.emplace_back(std::move(animation));
  rect.animations.emplace_back(std::move(animation2));
  rect.Animate(usToTicks(55000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(10, 100, 1000));
  rect.Animate(usToTicks(60000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(30, 300, 3000));
  rect.Animate(usToTicks(65000));
  EXPECT_VEC3F_EQ(rect.translation, gfx::Vector3dF(50, 500, 5000));
}

}  // namespace vr_shell
