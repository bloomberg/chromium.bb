// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cras_audio_client.h"

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {
}  // namespace

namespace chromeos {

// The CrasAudioClient implementation used in production.
class CrasAudioClientImpl : public CrasAudioClient {
 public:
  explicit CrasAudioClientImpl(dbus::Bus* bus)
      : cras_proxy_(NULL),
        weak_ptr_factory_(this) {
    cras_proxy_ = bus->GetObjectProxy(
        cras::kCrasServiceName,
        dbus::ObjectPath(cras::kCrasServicePath));

    // Monitor the D-Bus signal for output volume change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kOutputVolumeChanged,
        base::Bind(&CrasAudioClientImpl::OutputVolumeChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for output mute change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kOutputMuteChanged,
        base::Bind(&CrasAudioClientImpl::OutputMuteChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for input gain change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kInputGainChanged,
        base::Bind(&CrasAudioClientImpl::InputGainChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for input mute change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kInputMuteChanged,
        base::Bind(&CrasAudioClientImpl::InputMuteChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for nodes change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kNodesChanged,
        base::Bind(&CrasAudioClientImpl::NodesChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for active output node change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kActiveOutputNodeChanged,
        base::Bind(&CrasAudioClientImpl::ActiveOutputNodeChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for active input node change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kActiveInputNodeChanged,
        base::Bind(&CrasAudioClientImpl::ActiveInputNodeChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~CrasAudioClientImpl() {
  }

  // CrasAudioClient overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  virtual void GetVolumeState(const GetVolumeStateCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetVolumeState);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&CrasAudioClientImpl::OnGetVolumeState,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void SetOutputVolume(int32 volume) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputVolume);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(volume);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetOutputMute(bool mute_on) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetInputGain(int32 input_gain) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetInputGain);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(input_gain);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetInputMute(bool mute_on) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetInputMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetActiveOutputNode(uint64 node_id) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetActiveInputNode(uint64 node_id) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

 private:
  // Called when the cras signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success)
        << "Failed to connect to cras signal:" << signal_name;
  }

  // Called when a OutputVolumeChanged signal is received.
  void OutputVolumeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32 volume;
    if (!reader.PopInt32(&volume)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    FOR_EACH_OBSERVER(Observer, observers_, OutputVolumeChanged(volume));
  }

  // Called when a OutputMuteChanged signal is received.
  void OutputMuteChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool mute;
    if (!reader.PopBool(&mute)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    FOR_EACH_OBSERVER(Observer, observers_, OutputMuteChanged(mute));
  }

  // Called when a InputGainChanged signal is received.
  void InputGainChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32 gain;
    if (!reader.PopInt32(&gain)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    FOR_EACH_OBSERVER(Observer, observers_, InputGainChanged(gain));
  }

  // Called when a InputMuteChanged signal is received.
  void InputMuteChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool mute;
    if (!reader.PopBool(&mute)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    FOR_EACH_OBSERVER(Observer, observers_, InputMuteChanged(mute));
  }

  void NodesChangedReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, NodesChanged());
  }

  void ActiveOutputNodeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64 node_id;
    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:"
          << signal->ToString();
    }
    FOR_EACH_OBSERVER(Observer, observers_, ActiveOutputNodeChanged(node_id));
  }

  void ActiveInputNodeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64 node_id;
    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:"
          << signal->ToString();
    }
    FOR_EACH_OBSERVER(Observer, observers_, ActiveInputNodeChanged(node_id));
  }

  void OnGetVolumeState(const GetVolumeStateCallback& callback,
                        dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetVolumeState;
      return;
    }

    dbus::MessageReader reader(response);
    VolumeState volume_state;
    if (!reader.PopInt32(&volume_state.output_volume) ||
        !reader.PopBool(&volume_state.output_mute) ||
        !reader.PopInt32(&volume_state.input_gain) ||
        !reader.PopBool(&volume_state.input_mute)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
    }
    callback.Run(volume_state);
  }

  dbus::ObjectProxy* cras_proxy_;
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrasAudioClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioClientImpl);
};

// The CrasAudioClient implementation used on Linux desktop,
// which does nothing.
class CrasAudioClientStubImpl : public CrasAudioClient {
 public:
  CrasAudioClientStubImpl() {
    VLOG(1) << "CrasAudioClientStubImpl is created";
  }
  ~CrasAudioClientStubImpl() {}

  // CrasAudioClient overrides:
  // TODO(jennyz): Implement the observers and callbacks in the stub for UI
  // testing.
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual bool HasObserver(Observer* observer) OVERRIDE { return false; }
  virtual void GetVolumeState(const GetVolumeStateCallback& callback) OVERRIDE {
  }
  virtual void SetOutputVolume(int32 volume) OVERRIDE {}
  virtual void SetOutputMute(bool mute_on) OVERRIDE {}
  virtual void SetInputGain(int32 input_gain) OVERRIDE {}
  virtual void SetInputMute(bool mute_on) OVERRIDE {}
  virtual void SetActiveOutputNode(uint64 node_id) OVERRIDE {}
  virtual void SetActiveInputNode(uint64 node_id) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasAudioClientStubImpl);
};

CrasAudioClient::Observer::~Observer() {
}

void CrasAudioClient::Observer::OutputVolumeChanged(int32 volume) {
}

void CrasAudioClient::Observer::OutputMuteChanged(bool mute_on) {
}

void CrasAudioClient::Observer::InputGainChanged(int gain) {
}

void CrasAudioClient::Observer::InputMuteChanged(bool mute_on) {
}

void CrasAudioClient::Observer::NodesChanged() {
}

void CrasAudioClient::Observer::ActiveOutputNodeChanged(uint64 node_id){
}

void CrasAudioClient::Observer::ActiveInputNodeChanged(uint64 node_id) {
}

CrasAudioClient::CrasAudioClient() {
}

CrasAudioClient::~CrasAudioClient() {
}

// static
CrasAudioClient* CrasAudioClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new CrasAudioClientImpl(bus);
  }
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new CrasAudioClientStubImpl();
}

}  // namespace chromeos
