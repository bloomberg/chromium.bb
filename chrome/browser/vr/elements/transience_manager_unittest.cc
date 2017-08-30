// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transience_manager.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(TransienceManager, Visibility) {
  base::MessageLoop message_loop_;
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  UiElement element;
  element.SetOpacity(0.0f);
  element.set_opacity_when_visible(0.8);

  TransienceManager transience(&element, base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(0.0f, element.opacity());

  // Enable and disable, making sure the element appears and disappears.
  transience.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  transience.SetVisible(false);
  EXPECT_EQ(0.0f, element.opacity());

  // Enable, and ensure that the element transiently disappears.
  transience.SetVisible(true);
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0.0f, element.opacity());

  // Kick visibility, and ensure that the element transiently disappears.
  transience.KickVisibility();
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0.0f, element.opacity());

  // Kick visibility, and ensure that ending visibility hides the element.
  transience.KickVisibility();
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  transience.EndVisibility();
  EXPECT_EQ(0.0f, element.opacity());

  // Kick visibility, and ensure that disabling hides the element.
  transience.KickVisibility();
  EXPECT_EQ(element.opacity_when_visible(), element.opacity());
  transience.SetVisible(false);
  EXPECT_EQ(0.0f, element.opacity());
}

}  // namespace vr
