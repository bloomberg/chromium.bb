// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_AUDIO_DSP_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_AUDIO_DSP_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/audio_dsp_client.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// A fake implementation of AudioDspClient. Invokes callbacks immediately.
class CHROMEOS_EXPORT FakeAudioDspClient : public AudioDspClient {
 public:
  FakeAudioDspClient();
  ~FakeAudioDspClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // AudioDspClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Initialize(const BoolDBusMethodCallback& callback) override;
  void SetStandbyMode(bool standby,
                      const VoidDBusMethodCallback& callback) override;
  void SetNightMode(bool standby,
                    const VoidDBusMethodCallback& callback) override;
  void GetNightMode(const BoolDBusMethodCallback& callback) override;
  void SetTreble(double db_fs, const VoidDBusMethodCallback& callback) override;
  void GetTreble(const DoubleDBusMethodCallback& callback) override;
  void SetBass(double db_fs, const VoidDBusMethodCallback& callback) override;
  void GetBass(const DoubleDBusMethodCallback& callback) override;
  void GetCapabilitiesOEM(
      const ThreeStringDBusMethodCallback& callback) override;
  void SetCapabilitiesOEM(uint32 speaker_id,
                          const std::string& speaker_capabilities,
                          const std::string& driver_capabilities,
                          const VoidDBusMethodCallback& callback) override;
  void GetFilterConfigOEM(uint32 speaker_id,
                          const TwoStringDBusMethodCallback& callback) override;
  void SetFilterConfigOEM(const std::string& speaker_config,
                          const std::string& driver_config,
                          const VoidDBusMethodCallback& callback) override;
  void SetSourceType(uint16 source_type,
                     const VoidDBusMethodCallback& callback) override;
  void AmplifierVolumeChanged(double db_spl,
                              const VoidDBusMethodCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioDspClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_AUDIO_DSP_CLIENT_H_
