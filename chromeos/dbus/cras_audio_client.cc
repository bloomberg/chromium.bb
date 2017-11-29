// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cras_audio_client.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The CrasAudioClient implementation used in production.
class CrasAudioClientImpl : public CrasAudioClient {
 public:
  CrasAudioClientImpl() : cras_proxy_(NULL), weak_ptr_factory_(this) {}

  ~CrasAudioClientImpl() override = default;

  // CrasAudioClient overrides:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void GetVolumeState(DBusMethodCallback<VolumeState> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetVolumeState);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetVolumeState,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetDefaultOutputBufferSize(DBusMethodCallback<int> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetDefaultOutputBufferSize);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetDefaultOutputBufferSize,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNodes(DBusMethodCallback<AudioNodeList> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNodes);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetNodes,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetOutputNodeVolume(uint64_t node_id, int32_t volume) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputNodeVolume);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendInt32(volume);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SetOutputUserMute(bool mute_on) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputUserMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SetInputNodeGain(uint64_t node_id, int32_t input_gain) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetInputNodeGain);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendInt32(input_gain);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SetInputMute(bool mute_on) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetInputMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SetActiveOutputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SetActiveInputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void AddActiveInputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kAddActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void RemoveActiveInputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kRemoveActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void AddActiveOutputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kAddActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            dbus::ObjectProxy::EmptyResponseCallback());
  }

  void RemoveActiveOutputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kRemoveActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SwapLeftRight(uint64_t node_id, bool swap) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSwapLeftRight);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendBool(swap);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            dbus::ObjectProxy::EmptyResponseCallback());
  }

  void SetGlobalOutputChannelRemix(int32_t channels,
                                   const std::vector<double>& mixer) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetGlobalOutputChannelRemix);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(channels);
    writer.AppendArrayOfDoubles(mixer.data(), mixer.size());
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            dbus::ObjectProxy::EmptyResponseCallback());
  }

  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override {
    cras_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    cras_proxy_ = bus->GetObjectProxy(cras::kCrasServiceName,
                                      dbus::ObjectPath(cras::kCrasServicePath));

    // Monitor NameOwnerChanged signal.
    cras_proxy_->SetNameOwnerChangedCallback(
        base::Bind(&CrasAudioClientImpl::NameOwnerChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for output mute change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kOutputMuteChanged,
        base::Bind(&CrasAudioClientImpl::OutputMuteChangedReceived,
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

    // Monitor the D-Bus signal for output node volume change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kOutputNodeVolumeChanged,
        base::Bind(&CrasAudioClientImpl::OutputNodeVolumeChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for hotword.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kHotwordTriggered,
        base::Bind(&CrasAudioClientImpl::HotwordTriggeredReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&CrasAudioClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Called when the cras signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success)
        << "Failed to connect to cras signal:" << signal_name;
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    for (auto& observer : observers_)
      observer.AudioClientRestarted();
  }

  // Called when a OutputMuteChanged signal is received.
  void OutputMuteChangedReceived(dbus::Signal* signal) {
    // Chrome should always call SetOutputUserMute api to set the output
    // mute state and monitor user_mute state from OutputMuteChanged signal.
    dbus::MessageReader reader(signal);
    bool system_mute, user_mute;
    if (!reader.PopBool(&system_mute) || !reader.PopBool(&user_mute)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    for (auto& observer : observers_)
      observer.OutputMuteChanged(user_mute);
  }

  // Called when a InputMuteChanged signal is received.
  void InputMuteChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool mute;
    if (!reader.PopBool(&mute)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    for (auto& observer : observers_)
      observer.InputMuteChanged(mute);
  }

  void NodesChangedReceived(dbus::Signal* signal) {
    for (auto& observer : observers_)
      observer.NodesChanged();
  }

  void ActiveOutputNodeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    for (auto& observer : observers_)
      observer.ActiveOutputNodeChanged(node_id);
  }

  void ActiveInputNodeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:"
                 << signal->ToString();
    }
    for (auto& observer : observers_)
      observer.ActiveInputNodeChanged(node_id);
  }

  void OutputNodeVolumeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    int volume;

    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error eading signal from cras:" << signal->ToString();
    }
    if (!reader.PopInt32(&volume)) {
      LOG(ERROR) << "Error eading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_)
      observer.OutputNodeVolumeChanged(node_id, volume);
  }

  void HotwordTriggeredReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int64_t tv_sec, tv_nsec;

    if (!reader.PopInt64(&tv_sec)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
      return;
    }

    if (!reader.PopInt64(&tv_nsec)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
      return;
    }
    for (auto& observer : observers_)
      observer.HotwordTriggered(tv_sec, tv_nsec);
  }

  void OnGetVolumeState(DBusMethodCallback<VolumeState> callback,
                        dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetVolumeState;
      std::move(callback).Run(base::nullopt);
      return;
    }

    VolumeState volume_state;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&volume_state.output_volume) ||
        !reader.PopBool(&volume_state.output_system_mute) ||
        !reader.PopInt32(&volume_state.input_gain) ||
        !reader.PopBool(&volume_state.input_mute) ||
        !reader.PopBool(&volume_state.output_user_mute)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(volume_state));
  }

  void OnGetDefaultOutputBufferSize(DBusMethodCallback<int> callback,
                                    dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetDefaultOutputBufferSize;
      std::move(callback).Run(base::nullopt);
      return;
    }
    int32_t buffer_size = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&buffer_size)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(buffer_size);
  }

  void OnGetNodes(DBusMethodCallback<AudioNodeList> callback,
                  dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    AudioNodeList node_list;
    dbus::MessageReader response_reader(response);
    dbus::MessageReader array_reader(response);
    while (response_reader.HasMoreData()) {
      if (!response_reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Error reading response from cras: "
                   << response->ToString();
        std::move(callback).Run(base::nullopt);
        return;
      }

      AudioNode node;
      if (!GetAudioNode(response, &array_reader, &node)) {
        LOG(WARNING) << "Error reading audio node data from cras: "
                     << response->ToString();
        std::move(callback).Run(base::nullopt);
        return;
      }

      // Filter out the "UNKNOWN" type of audio devices.
      if (node.type != "UNKNOWN")
        node_list.push_back(std::move(node));
    }

    std::move(callback).Run(std::move(node_list));
  }

  bool GetAudioNode(dbus::Response* response,
                    dbus::MessageReader* array_reader,
                    AudioNode* node) {
    while (array_reader->HasMoreData()) {
      dbus::MessageReader dict_entry_reader(response);
      dbus::MessageReader value_reader(response);
      std::string key;
      if (!array_reader->PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key) ||
          !dict_entry_reader.PopVariant(&value_reader)) {
         return false;
      }

      if (key == cras::kIsInputProperty) {
        if (!value_reader.PopBool(&node->is_input))
          return false;
      } else if (key == cras::kIdProperty) {
        if (!value_reader.PopUint64(&node->id))
          return false;
      } else if (key == cras::kDeviceNameProperty) {
        if (!value_reader.PopString(&node->device_name))
          return false;
      } else if (key == cras::kTypeProperty) {
        if (!value_reader.PopString(&node->type))
          return false;
      } else if (key == cras::kNameProperty) {
        if (!value_reader.PopString(&node->name))
          return false;
      } else if (key == cras::kActiveProperty) {
        if (!value_reader.PopBool(&node->active))
          return false;
      } else if (key == cras::kPluggedTimeProperty) {
        if (!value_reader.PopUint64(&node->plugged_time))
          return false;
      } else if (key == cras::kMicPositionsProperty) {
        if (!value_reader.PopString(&node->mic_positions))
          return false;
      } else if (key == cras::kStableDeviceIdProperty) {
        if (!value_reader.PopUint64(&node->stable_device_id_v1))
          return false;
      } else if (key == cras::kStableDeviceIdNewProperty) {
        if (!value_reader.PopUint64(&node->stable_device_id_v2))
          return false;
        node->has_v2_stable_device_id = true;
      }
    }

    return true;
  }

  dbus::ObjectProxy* cras_proxy_;
  base::ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrasAudioClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioClientImpl);
};

CrasAudioClient::Observer::~Observer() = default;

void CrasAudioClient::Observer::AudioClientRestarted() {
}

void CrasAudioClient::Observer::OutputMuteChanged(bool mute_on) {
}

void CrasAudioClient::Observer::InputMuteChanged(bool mute_on) {
}

void CrasAudioClient::Observer::NodesChanged() {
}

void CrasAudioClient::Observer::ActiveOutputNodeChanged(uint64_t node_id) {}

void CrasAudioClient::Observer::ActiveInputNodeChanged(uint64_t node_id) {}

void CrasAudioClient::Observer::OutputNodeVolumeChanged(uint64_t node_id,
                                                        int volume) {
}

void CrasAudioClient::Observer::HotwordTriggered(uint64_t tv_sec,
                                                 uint64_t tv_nsec) {}

CrasAudioClient::CrasAudioClient() = default;

CrasAudioClient::~CrasAudioClient() = default;

// static
CrasAudioClient* CrasAudioClient::Create() {
  return new CrasAudioClientImpl();
}

}  // namespace chromeos
