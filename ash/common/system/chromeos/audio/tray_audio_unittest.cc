// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/audio/tray_audio.h"

#include "ash/common/system/tray/system_tray.h"
#include "ash/common/test/ash_test.h"

namespace ash {

using TrayAudioTest = AshTest;

// Tests that the volume popup view can be explicitly shown.
TEST_F(TrayAudioTest, ShowPopUpVolumeView) {
  TrayAudio* tray_audio = GetPrimarySystemTray()->GetTrayAudio();
  ASSERT_TRUE(tray_audio);

  // The volume popup is not visible initially.
  EXPECT_FALSE(tray_audio->volume_view_for_testing());
  EXPECT_FALSE(tray_audio->pop_up_volume_view_for_testing());

  // Simulate ARC asking to show the volume view.
  TrayAudio::ShowPopUpVolumeView();

  // Volume view is now visible.
  EXPECT_TRUE(tray_audio->volume_view_for_testing());
  EXPECT_TRUE(tray_audio->pop_up_volume_view_for_testing());
}

}  // namespace ash
