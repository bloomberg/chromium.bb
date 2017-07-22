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
  float opacity_when_enabled = 0.8;
  element.SetOpacity(0.0f);

  TransienceManager transience(&element, opacity_when_enabled,
                               base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());

  transience.KickVisibilityIfEnabled();
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());

  // Enable and disable, making sure the element appears and disappears.
  transience.SetEnabled(true);
  EXPECT_EQ(true, element.visible());
  EXPECT_EQ(opacity_when_enabled, element.opacity());
  transience.SetEnabled(false);
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());

  // Enable, and ensure that the element transiently disappears.
  transience.SetEnabled(true);
  EXPECT_EQ(true, element.visible());
  EXPECT_EQ(opacity_when_enabled, element.opacity());
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());

  // Kick visibility, and ensure that the element transiently disappears.
  transience.KickVisibilityIfEnabled();
  EXPECT_EQ(true, element.visible());
  EXPECT_EQ(opacity_when_enabled, element.opacity());
  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());

  // Kick visibility, and ensure that ending visibility hides the element.
  transience.KickVisibilityIfEnabled();
  EXPECT_EQ(true, element.visible());
  EXPECT_EQ(opacity_when_enabled, element.opacity());
  transience.EndVisibilityIfEnabled();
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());

  // Kick visibility, and ensure that disabling hides the element.
  transience.KickVisibilityIfEnabled();
  EXPECT_EQ(true, element.visible());
  EXPECT_EQ(opacity_when_enabled, element.opacity());
  transience.SetEnabled(false);
  EXPECT_EQ(false, element.visible());
  EXPECT_EQ(0.0f, element.opacity());
}

}  // namespace vr
