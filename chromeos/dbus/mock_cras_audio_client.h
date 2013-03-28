// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CRAS_AUDIO_CLIENT_H_

#include "chromeos/dbus/cras_audio_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCrasAudioClient : public CrasAudioClient {
 public:
  MockCrasAudioClient();
  virtual ~MockCrasAudioClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(HasObserver, bool(Observer*));
  MOCK_METHOD1(GetVolumeState, void(const GetVolumeStateCallback&));
  MOCK_METHOD1(SetOutputVolume, void(int32));
  MOCK_METHOD1(SetOutputMute, void(bool));
  MOCK_METHOD1(SetInputGain, void(int32));
  MOCK_METHOD1(SetInputMute, void(bool));
  MOCK_METHOD1(SetActiveOutputNode, void(uint64));
  MOCK_METHOD1(SetActiveInputNode, void(uint64));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCrasAudioClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CRAS_AUDIO_CLIENT_H_
