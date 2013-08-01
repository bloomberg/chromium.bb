// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "ui/base/accelerators/accelerator.h"

namespace {

class VolumeControllerTest : public InProcessBrowserTest {
 public:
  VolumeControllerTest() {}
  virtual ~VolumeControllerTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    volume_controller_.reset(new VolumeController());
    audio_handler_ = chromeos::CrasAudioHandler::Get();
  }

 protected:
  void VolumeMute() {
    volume_controller_->HandleVolumeMute(ui::Accelerator());
  }

  void VolumeUp() {
    volume_controller_->HandleVolumeUp(ui::Accelerator());
  }

  void VolumeDown() {
    volume_controller_->HandleVolumeDown(ui::Accelerator());
  }

  chromeos::CrasAudioHandler* audio_handler_;  // Not owned.

 private:
  scoped_ptr<VolumeController> volume_controller_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControllerTest);
};

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpAndDown) {
  // Set initial value as 50%
  const int kInitVolume = 50;
  audio_handler_->SetOutputVolumePercent(kInitVolume);

  EXPECT_EQ(audio_handler_->GetOutputVolumePercent(), kInitVolume);

  VolumeUp();
  EXPECT_LT(kInitVolume, audio_handler_->GetOutputVolumePercent());
  VolumeDown();
  EXPECT_EQ(kInitVolume, audio_handler_->GetOutputVolumePercent());
  VolumeDown();
  EXPECT_GT(kInitVolume, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeDownToZero) {
  // Setting to very small volume.
  audio_handler_->SetOutputVolumePercent(1);

  VolumeDown();
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());
  VolumeDown();
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());
  VolumeUp();
  EXPECT_LT(0, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpTo100) {
  // Setting to almost max
  audio_handler_->SetOutputVolumePercent(99);

  VolumeUp();
  EXPECT_EQ(100, audio_handler_->GetOutputVolumePercent());
  VolumeUp();
  EXPECT_EQ(100, audio_handler_->GetOutputVolumePercent());
  VolumeDown();
  EXPECT_GT(100, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, Mutes) {
  ASSERT_FALSE(audio_handler_->IsOutputMuted());
  const int initial_volume = audio_handler_->GetOutputVolumePercent();

  VolumeMute();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());

  // Further mute buttons doesn't have effects.
  VolumeMute();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());

  // Right after the volume up after set_mute recovers to original volume.
  VolumeUp();
  EXPECT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(initial_volume, audio_handler_->GetOutputVolumePercent());

  VolumeMute();
  // After the volume down, the volume goes down to zero explicitly.
  VolumeDown();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());

  // Thus, further VolumeUp doesn't recover the volume, it's just slightly
  // bigger than 0.
  VolumeUp();
  EXPECT_LT(0, audio_handler_->GetOutputVolumePercent());
  EXPECT_GT(initial_volume, audio_handler_->GetOutputVolumePercent());
}

}  // namespace
