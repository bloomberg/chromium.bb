// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace ash {

using AshTouchExplorationManagerTest = AshTestBase;

TEST_F(AshTouchExplorationManagerTest, AdjustSound) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AshTouchExplorationManager touch_exploration_manager(controller);
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();

  touch_exploration_manager.SetOutputLevel(10);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 10);
  EXPECT_FALSE(audio_handler->IsOutputMuted());

  touch_exploration_manager.SetOutputLevel(100);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 100);
  EXPECT_FALSE(audio_handler->IsOutputMuted());

  touch_exploration_manager.SetOutputLevel(0);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  EXPECT_TRUE(audio_handler->IsOutputMuted());

  touch_exploration_manager.SetOutputLevel(-10);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
}

TEST_F(AshTouchExplorationManagerTest, HandleAccessibilityGesture) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AshTouchExplorationManager touch_exploration_manager(controller);
  AccessibilityController* a11y_controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  a11y_controller->SetClient(client.CreateInterfacePtrAndBind());

  touch_exploration_manager.HandleAccessibilityGesture(ui::AX_GESTURE_CLICK);
  a11y_controller->FlushMojoForTest();
  EXPECT_EQ("click", client.last_a11y_gesture());

  touch_exploration_manager.HandleAccessibilityGesture(
      ui::AX_GESTURE_SWIPE_LEFT_1);
  a11y_controller->FlushMojoForTest();
  EXPECT_EQ("swipeLeft1", client.last_a11y_gesture());
}

}  //  namespace ash
