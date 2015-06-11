// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_audio_dsp_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace chromeos {

namespace {

void OnVoidDBusMethod(const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}

void OnBoolDBusMethod(const BoolDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, true);
}

void OnDoubleDBusMethod(
    const AudioDspClient::DoubleDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, 0.0);
}

void OnTwoStringDBusMethod(
    const AudioDspClient::TwoStringDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, std::string(), std::string());
}

void OnThreeStringDBusMethod(
    const AudioDspClient::ThreeStringDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, std::string(), std::string(),
               std::string());
}

}  // anonymous namespace

FakeAudioDspClient::FakeAudioDspClient() {
}

FakeAudioDspClient::~FakeAudioDspClient() {
}

void FakeAudioDspClient::Init(dbus::Bus* bus) {
}

void FakeAudioDspClient::AddObserver(Observer* observer) {
}

void FakeAudioDspClient::RemoveObserver(Observer* observer) {
}

void FakeAudioDspClient::Initialize(const BoolDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnBoolDBusMethod, callback));
}

void FakeAudioDspClient::SetStandbyMode(
    bool standby,
    const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::SetNightMode(bool standby,
                                      const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::GetNightMode(const BoolDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnBoolDBusMethod, callback));
}

void FakeAudioDspClient::SetTreble(double db_fs,
                                   const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::GetTreble(const DoubleDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnDoubleDBusMethod, callback));
}

void FakeAudioDspClient::SetBass(double db_fs,
                                 const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::GetBass(const DoubleDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnDoubleDBusMethod, callback));
}

void FakeAudioDspClient::GetCapabilitiesOEM(
    const ThreeStringDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnThreeStringDBusMethod, callback));
}

void FakeAudioDspClient::SetCapabilitiesOEM(
    uint32 speaker_id,
    const std::string& speaker_capabilities,
    const std::string& driver_capabilities,
    const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::GetFilterConfigOEM(
    uint32 speaker_id,
    const TwoStringDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnTwoStringDBusMethod, callback));
}

void FakeAudioDspClient::SetFilterConfigOEM(
    const std::string& speaker_config,
    const std::string& driver_config,
    const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::SetSourceType(uint16 source_type,
                                       const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAudioDspClient::AmplifierVolumeChanged(
    double db_spl,
    const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

}  // namespace chromeos
