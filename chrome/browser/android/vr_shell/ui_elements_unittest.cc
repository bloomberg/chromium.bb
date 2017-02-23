// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements.h"

#include <utility>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_VEC3F_EQ(a, b) \
  EXPECT_FLOAT_EQ(a.x, b.x);  \
  EXPECT_FLOAT_EQ(a.y, b.y);  \
  EXPECT_FLOAT_EQ(a.z, b.z);

#define EXPECT_RECTF_EQ(a, b)        \
  EXPECT_FLOAT_EQ(a.x, b.x);         \
  EXPECT_FLOAT_EQ(a.y, b.y);         \
  EXPECT_FLOAT_EQ(a.width, b.width); \
  EXPECT_FLOAT_EQ(a.height, b.height);

#define EXPECT_ROTATION(a, b) \
  EXPECT_FLOAT_EQ(a.x, b.x);  \
  EXPECT_FLOAT_EQ(a.y, b.y);  \
  EXPECT_FLOAT_EQ(a.z, b.z);  \
  EXPECT_FLOAT_EQ(a.angle, b.angle);

namespace vr_shell {

TEST(UiElements, AnimateCopyRect) {
  ContentRectangle rect;
  rect.copy_rect = {10, 100, 1000, 10000};
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::COPYRECT,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200, 2000, 20000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(50000);
  EXPECT_RECTF_EQ(rect.copy_rect, Rectf({10, 100, 1000, 10000}));
  rect.Animate(60000);
  EXPECT_RECTF_EQ(rect.copy_rect, Rectf({20, 200, 2000, 20000}));
}

TEST(UiElements, AnimateSize) {
  ContentRectangle rect;
  rect.size = {10, 100};
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::SIZE,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(50000);
  EXPECT_VEC3F_EQ(rect.size, gvr::Vec3f({10, 100}));
  rect.Animate(60000);
  EXPECT_VEC3F_EQ(rect.size, gvr::Vec3f({20, 200}));
}

TEST(UiElements, AnimateTranslation) {
  ContentRectangle rect;
  rect.translation = {10, 100, 1000};
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200, 2000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(50000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({10, 100, 1000}));
  rect.Animate(60000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({20, 200, 2000}));
}

TEST(UiElements, AnimateRotation) {
  ContentRectangle rect;
  rect.rotation = {10, 100, 1000, 10000};
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::ROTATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200, 2000, 20000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(50000);
  EXPECT_ROTATION(rect.rotation, RotationAxisAngle({10, 100, 1000, 10000}));
  rect.Animate(60000);
  EXPECT_ROTATION(rect.rotation, RotationAxisAngle({20, 200, 2000, 20000}));
}

TEST(UiElements, AnimationHasNoEffectBeforeScheduledStart) {
  ContentRectangle rect;
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()),
                    {10, 100, 1000}, {20, 200, 2000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(49999);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({0, 0, 0}));
}

TEST(UiElements, AnimationPurgedWhenDone) {
  ContentRectangle rect;
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()),
                    {10, 100, 1000}, {20, 200, 2000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(60000);
  EXPECT_EQ(0u, rect.animations.size());
}

TEST(UiElements, AnimationLinearEasing) {
  ContentRectangle rect;
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()),
                    {10, 100, 1000}, {20, 200, 2000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(50000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({10, 100, 1000}));
  rect.Animate(55000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({15, 150, 1500}));
  rect.Animate(60000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({20, 200, 2000}));
}

TEST(UiElements, AnimationStartFromSpecifiedLocation) {
  ContentRectangle rect;
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()),
                    {10, 100, 1000}, {20, 200, 2000}, 50000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.Animate(50000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({10, 100, 1000}));
  rect.Animate(60000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({20, 200, 2000}));
}

// Ensure that when a new animation overlaps another of the same type, the
// newly added animation overrides the original.  For example:
//   Animation 1:  ? .......... 20
//   Animation 2:        ?  .......... 50
//   Result:       0 ... 10 ... 30 ... 50
TEST(UiElements, AnimationOverlap) {
  ContentRectangle rect;
  std::unique_ptr<Animation> animation(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {20, 200, 2000}, 50000, 10000));
  std::unique_ptr<Animation> animation2(
      new Animation(0, Animation::Property::TRANSLATION,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {50, 500, 5000}, 55000, 10000));
  rect.animations.emplace_back(std::move(animation));
  rect.animations.emplace_back(std::move(animation2));
  rect.Animate(55000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({10, 100, 1000}));
  rect.Animate(60000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({30, 300, 3000}));
  rect.Animate(65000);
  EXPECT_VEC3F_EQ(rect.translation, gvr::Vec3f({50, 500, 5000}));
}

}  // namespace vr_shell
