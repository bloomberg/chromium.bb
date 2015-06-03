// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/audio_dsp_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {
namespace {

// TODO(benchan): Move these DBus constants to system_api.
namespace audio_dsp {

const char kAudioDspInterface[] = "org.chromium.AudioDsp";
const char kAudioDspServiceName[] = "org.chromium.AudioDsp";
const char kAudioDspServicePath[] = "/org/chromium/AudioDsp";
const char kInitializeMethod[] = "Initialize";
const char kSetStandbyModeMethod[] = "SetStandbyMode";
const char kSetNightModeMethod[] = "SetNightMode";
const char kSetTrebleMethod[] = "SetTreble";
const char kSetBassMethod[] = "SetBass";
const char kGetNightModeMethod[] = "GetNightMode";
const char kGetTrebleMethod[] = "GetTreble";
const char kGetBassMethod[] = "GetBass";
const char kGetCapabilitiesOEMMethod[] = "GetCapabilitiesOEM";
const char kSetCapabilitiesOEMMethod[] = "SetCapabilitiesOEM";
const char kGetFilterConfigOEMMethod[] = "GetFilterConfigOEM";
const char kSetFilterConfigOEMMethod[] = "SetFilterConfigOEM";
const char kSetSourceTypeMethod[] = "SetSourceType";
const char kAmplifierVolumeChangedMethod[] = "AmplifierVolumeChanged";
const char kErrorSignal[] = "Error";

}  // namespace audio_dsp

void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                      dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

void OnBoolDBusMethod(const BoolDBusMethodCallback& callback,
                      dbus::Response* response) {
  if (!response) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, false);
    return;
  }

  dbus::MessageReader reader(response);
  bool result;
  if (!reader.PopBool(&result)) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, false);
    return;
  }

  callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
}

void OnDoubleDBusMethod(
    const AudioDspClient::DoubleDBusMethodCallback& callback,
    dbus::Response* response) {
  bool ok = false;
  double result = 0.0;
  if (response) {
    dbus::MessageReader reader(response);
    ok = reader.PopDouble(&result);
  }
  callback.Run(ok ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE,
               result);
}

void OnTwoStringDBusMethod(
    const AudioDspClient::TwoStringDBusMethodCallback& callback,
    dbus::Response* response) {
  std::string result1;
  std::string result2;
  bool ok = false;
  if (response) {
    dbus::MessageReader reader(response);
    ok = reader.PopString(&result1) && reader.PopString(&result2);
  }
  callback.Run(ok ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE,
               result1, result2);
}

void OnThreeStringDBusMethod(
    const AudioDspClient::ThreeStringDBusMethodCallback& callback,
    dbus::Response* response) {
  std::string result1;
  std::string result2;
  std::string result3;
  bool ok = false;
  if (response) {
    dbus::MessageReader reader(response);
    ok = reader.PopString(&result1) && reader.PopString(&result2) &&
         reader.PopString(&result3);
  }
  callback.Run(ok ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE,
               result1, result2, result3);
}

// The AudioDspClient implementation.
class AudioDspClientImpl : public AudioDspClient {
 public:
  AudioDspClientImpl()
      : proxy_(nullptr), signal_connected_(false), weak_ptr_factory_(this) {}

  ~AudioDspClientImpl() override {}

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
  // Handles Error signal and notifies |observers_|.
  void OnError(dbus::Signal* signal);

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded);

  dbus::ObjectProxy* proxy_;

  // True when |proxy_| has been connected to the Error signal.
  bool signal_connected_;

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AudioDspClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDspClientImpl);
};

void AudioDspClientImpl::Init(dbus::Bus* bus) {
  proxy_ =
      bus->GetObjectProxy(audio_dsp::kAudioDspServiceName,
                          dbus::ObjectPath(audio_dsp::kAudioDspServicePath));
  DCHECK(proxy_);
}

void AudioDspClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (!signal_connected_) {
    signal_connected_ = true;
    DCHECK(proxy_);
    proxy_->ConnectToSignal(audio_dsp::kAudioDspInterface,
                            audio_dsp::kErrorSignal,
                            base::Bind(&AudioDspClientImpl::OnError,
                                       weak_ptr_factory_.GetWeakPtr()),
                            base::Bind(&AudioDspClientImpl::OnSignalConnected,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
  observers_.AddObserver(observer);
}

void AudioDspClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AudioDspClientImpl::Initialize(const BoolDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kInitializeMethod);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnBoolDBusMethod, callback));
}

void AudioDspClientImpl::SetStandbyMode(
    bool standby,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetStandbyModeMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(standby);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::SetNightMode(bool night_mode,
                                      const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetNightModeMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(night_mode);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::GetNightMode(const BoolDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kGetNightModeMethod);
  dbus::MessageWriter writer(&method_call);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnBoolDBusMethod, callback));
}

void AudioDspClientImpl::SetTreble(double db_fs,
                                   const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetTrebleMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(db_fs);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::GetTreble(const DoubleDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kGetTrebleMethod);
  dbus::MessageWriter writer(&method_call);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnDoubleDBusMethod, callback));
}

void AudioDspClientImpl::SetBass(double db_fs,
                                 const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetBassMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(db_fs);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::GetBass(const DoubleDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kGetBassMethod);
  dbus::MessageWriter writer(&method_call);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnDoubleDBusMethod, callback));
}

void AudioDspClientImpl::GetCapabilitiesOEM(
    const ThreeStringDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kGetCapabilitiesOEMMethod);
  dbus::MessageWriter writer(&method_call);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnThreeStringDBusMethod, callback));
}

void AudioDspClientImpl::SetCapabilitiesOEM(
    uint32 speaker_id,
    const std::string& speaker_capabilities,
    const std::string& driver_capabilities,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetCapabilitiesOEMMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(speaker_id);
  writer.AppendString(speaker_capabilities);
  writer.AppendString(driver_capabilities);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::GetFilterConfigOEM(
    uint32 speaker_id,
    const TwoStringDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kGetFilterConfigOEMMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(speaker_id);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnTwoStringDBusMethod, callback));
}

void AudioDspClientImpl::SetFilterConfigOEM(
    const std::string& speaker_config,
    const std::string& driver_config,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetFilterConfigOEMMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(speaker_config);
  writer.AppendString(driver_config);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::SetSourceType(uint16 source_type,
                                       const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kSetSourceTypeMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendUint16(source_type);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::AmplifierVolumeChanged(
    double db_spl,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(audio_dsp::kAudioDspInterface,
                               audio_dsp::kAmplifierVolumeChangedMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(db_spl);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AudioDspClientImpl::OnError(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  int32 error_code = 0;
  if (!reader.PopInt32(&error_code)) {
    LOG(ERROR) << "Invalid signal: " << signal->ToString();
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnError(error_code));
}

void AudioDspClientImpl::OnSignalConnected(const std::string& interface,
                                           const std::string& signal,
                                           bool succeeded) {
  LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " << signal
                            << " failed.";
}

}  // anonymous namespace

AudioDspClient::AudioDspClient() {
}

AudioDspClient::~AudioDspClient() {
}

// static
AudioDspClient* AudioDspClient::Create() {
  return new AudioDspClientImpl();
}

}  // namespace chromeos
