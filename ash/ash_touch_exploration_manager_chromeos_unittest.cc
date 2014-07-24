// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace ash {

typedef test::AshTestBase AshTouchExplorationManagerTest;

TEST_F(AshTouchExplorationManagerTest, AdjustSound) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AshTouchExplorationManager touch_exploration_manager(controller);
  chromeos::CrasAudioHandler* audio_handler =
      chromeos::CrasAudioHandler::Get();

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

} //  namespace ash
