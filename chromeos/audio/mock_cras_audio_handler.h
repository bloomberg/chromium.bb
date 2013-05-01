// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_MOCK_CRAS_AUDIO_HANDLER_H_
#define CHROMEOS_AUDIO_MOCK_CRAS_AUDIO_HANDLER_H_

#include "chromeos/audio/cras_audio_handler.h"

namespace chromeos {

// Mock class for CrasAudioHandler.
class CHROMEOS_EXPORT MockCrasAudioHandler : public CrasAudioHandler {
 public:
  MockCrasAudioHandler();
  virtual ~MockCrasAudioHandler();

  virtual void AddAudioObserver(AudioObserver* observer) OVERRIDE;
  virtual void RemoveAudioObserver(AudioObserver* observer) OVERRIDE;
  virtual bool IsOutputMuted() OVERRIDE;
  virtual bool IsInputMuted() OVERRIDE;
  virtual int GetOutputVolumePercent() OVERRIDE;
  virtual uint64 GetActiveOutputNode() const OVERRIDE;
  virtual uint64 GetActiveInputNode() const OVERRIDE;
  virtual void GetAudioDevices(AudioDeviceList* device_list) const OVERRIDE;
  virtual bool GetActiveOutputDevice(AudioDevice* device) const OVERRIDE;
  virtual bool has_alternative_input() const OVERRIDE;
  virtual bool has_alternative_output() const OVERRIDE;
  virtual void SetOutputVolumePercent(int volume_percent) OVERRIDE;
  virtual void AdjustOutputVolumeByPercent(int adjust_by_percent) OVERRIDE;
  virtual void SetOutputMute(bool mute_on) OVERRIDE;
  virtual void SetInputMute(bool mute_on) OVERRIDE;
  virtual void SetActiveOutputNode(uint64 node_id) OVERRIDE;
  virtual void SetActiveInputNode(uint64 node_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCrasAudioHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_MOCK_CRAS_AUDIO_HANDLER_H_
