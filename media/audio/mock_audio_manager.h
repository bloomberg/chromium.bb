// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_

#include "base/macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "media/audio/audio_manager.h"

namespace media {

// This class is a simple mock around AudioManager, used exclusively for tests,
// which avoids to use the actual (system and platform dependent) AudioManager.
// Some bots does not have input devices, thus using the actual AudioManager
// would causing failures on classes which expect that.
class MockAudioManager : public AudioManager {
 public:
  class Deleter {
   public:
    void operator()(const MockAudioManager* instance) const;
  };

  using UniquePtr = std::unique_ptr<MockAudioManager, Deleter>;

  explicit MockAudioManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  bool HasAudioOutputDevices() override;

  bool HasAudioInputDevices() override;

  base::string16 GetAudioInputDeviceModel() override;

  void ShowAudioInputSettings() override;

  void GetAudioInputDeviceDescriptions(
      media::AudioDeviceDescriptions* device_descriptions) override;

  void GetAudioOutputDeviceDescriptions(
      media::AudioDeviceDescriptions* device_descriptions) override;

  AudioOutputStream* MakeAudioOutputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  AudioOutputStream* MakeAudioOutputStreamProxy(
      const media::AudioParameters& params,
      const std::string& device_id) override;

  AudioInputStream* MakeAudioInputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  void AddOutputDeviceChangeListener(AudioDeviceListener* listener) override;
  void RemoveOutputDeviceChangeListener(AudioDeviceListener* listener) override;

  AudioParameters GetDefaultOutputStreamParameters() override;
  AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;

  std::unique_ptr<AudioLog> CreateAudioLog(
      AudioLogFactory::AudioComponent component) override;

  void InitializeOutputDebugRecording(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) override;
  void EnableOutputDebugRecording(
      const base::FilePath& base_file_name) override;
  void DisableOutputDebugRecording() override;

  const char* GetName() override;

  // Setters to emulate desired in-test behavior.
  void SetInputStreamParameters(const AudioParameters& input_params);
  void SetHasInputDevices(bool has_input_devices);

 protected:
  ~MockAudioManager() override;

 private:
  friend class base::DeleteHelper<MockAudioManager>;
  AudioParameters input_params_;
  bool has_input_devices_ = true;

  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

}  // namespace media.

#endif  // MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
