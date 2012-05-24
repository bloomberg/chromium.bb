// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/volume_controller_chromeos.h"

#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/audio/audio_mixer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/accelerators/accelerator.h"

namespace {

class MockAudioMixer : public chromeos::AudioMixer {
 public:
  MockAudioMixer()
      : volume_(0.0),
        is_muted_(false) {
  }

  virtual void Init() OVERRIDE {
  }

  virtual double GetVolumePercent() OVERRIDE {
    return volume_;
  }

  virtual void SetVolumePercent(double percent) OVERRIDE {
    volume_ = percent;
  }

  virtual bool IsMuted() OVERRIDE {
    return is_muted_;
  }

  virtual void SetMuted(bool do_mute) OVERRIDE {
    is_muted_ = do_mute;
  }

 private:
  double volume_;
  bool is_muted_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioMixer);
};

// This class has to be browsertest because AudioHandler uses prefs_service.
class VolumeControllerTest : public InProcessBrowserTest {
 public:
  VolumeControllerTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    // First we should shutdown the default audio handler.
    chromeos::AudioHandler::Shutdown();
    audio_mixer_ = new MockAudioMixer;
    chromeos::AudioHandler::InitializeForTesting(audio_mixer_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    chromeos::AudioHandler::Shutdown();
    audio_mixer_ = NULL;
  }

 protected:
  void VolumeMute() {
    volume_controller_.HandleVolumeMute(ui::Accelerator());
  }

  void VolumeUp() {
    volume_controller_.HandleVolumeUp(ui::Accelerator());
  }

  void VolumeDown() {
    volume_controller_.HandleVolumeDown(ui::Accelerator());
  }

  MockAudioMixer* audio_mixer() { return audio_mixer_; }

 private:
  VolumeController volume_controller_;
  MockAudioMixer* audio_mixer_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControllerTest);
};

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpAndDown) {
  // Set initial value as 50%
  audio_mixer()->SetVolumePercent(50.0);

  double initial_volume = audio_mixer()->GetVolumePercent();

  VolumeUp();
  EXPECT_LT(initial_volume, audio_mixer()->GetVolumePercent());
  VolumeDown();
  EXPECT_DOUBLE_EQ(initial_volume, audio_mixer()->GetVolumePercent());
  VolumeDown();
  EXPECT_GT(initial_volume, audio_mixer()->GetVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeDownToZero) {
  // Setting to very small
  audio_mixer()->SetVolumePercent(0.1);

  VolumeDown();
  EXPECT_DOUBLE_EQ(0.0, audio_mixer()->GetVolumePercent());
  VolumeDown();
  EXPECT_DOUBLE_EQ(0.0, audio_mixer()->GetVolumePercent());
  VolumeUp();
  EXPECT_LT(0.0, audio_mixer()->GetVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpTo100) {
  // Setting to almost max
  audio_mixer()->SetVolumePercent(99.0);

  VolumeUp();
  EXPECT_DOUBLE_EQ(100.0, audio_mixer()->GetVolumePercent());
  VolumeUp();
  EXPECT_DOUBLE_EQ(100.0, audio_mixer()->GetVolumePercent());
  VolumeDown();
  EXPECT_GT(100.0, audio_mixer()->GetVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, Mutes) {
  ASSERT_FALSE(audio_mixer()->IsMuted());
  double initial_volume = audio_mixer()->GetVolumePercent();

  VolumeMute();
  EXPECT_TRUE(audio_mixer()->IsMuted());

  // Further mute buttons doesn't have effects.
  VolumeMute();
  EXPECT_TRUE(audio_mixer()->IsMuted());

  // Right after the volume up after set_mute recovers to original volume.
  VolumeUp();
  EXPECT_FALSE(audio_mixer()->IsMuted());
  EXPECT_DOUBLE_EQ(initial_volume, audio_mixer()->GetVolumePercent());

  VolumeMute();
  // After the volume down, the volume goes down to zero explicitly.
  VolumeDown();
  EXPECT_TRUE(audio_mixer()->IsMuted());
  EXPECT_DOUBLE_EQ(0.0, audio_mixer()->GetVolumePercent());

  // Thus, further VolumeUp doesn't recover the volume, it's just slightly
  // bigger than 0.
  VolumeUp();
  EXPECT_LT(0.0, audio_mixer()->GetVolumePercent());
  EXPECT_GT(initial_volume, audio_mixer()->GetVolumePercent());
}

}  // namespace
