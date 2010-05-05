// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/multi_animation.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test MultiAnimationTest;

TEST_F(MultiAnimationTest, Basic) {
  // Create a MultiAnimation with two parts.
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(100, Tween::LINEAR));
  parts.push_back(MultiAnimation::Part(100, Tween::EASE_OUT));

  MultiAnimation animation(parts);
  AnimationContainer::Element* as_element =
      static_cast<AnimationContainer::Element*>(&animation);
  as_element->SetStartTime(base::TimeTicks());

  // Step to 50, which is half way through the first part.
  as_element->Step(base::TimeTicks() + base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(.5, animation.GetCurrentValue());

  // Step to 120, which is 20% through the second part.
  as_element->Step(base::TimeTicks() +
                   base::TimeDelta::FromMilliseconds(120));
  EXPECT_EQ(Tween::CalculateValue(Tween::EASE_OUT, .2),
            animation.GetCurrentValue());

  // Step to 320, which is 20% through the second part.
  as_element->Step(base::TimeTicks() +
                   base::TimeDelta::FromMilliseconds(320));
  EXPECT_EQ(Tween::CalculateValue(Tween::EASE_OUT, .2),
            animation.GetCurrentValue());
}
