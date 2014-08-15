// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cras_audio_client.h"

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/cras_audio_client_stub_impl.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Error name if cras dbus call fails with empty ErrorResponse.
const char kNoResponseError[] =
    "org.chromium.cras.Error.NoResponse";

// The CrasAudioClient implementation used in production.
class CrasAudioClientImpl : public CrasAudioClient {
 public:
  CrasAudioClientImpl() : cras_proxy_(NULL), weak_ptr_factory_(this) {}

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

  virtual void GetNodes(const GetNodesCallback& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNodes);
    cras_proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&CrasAudioClientImpl::OnGetNodes,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&CrasAudioClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  virtual void SetOutputNodeVolume(uint64 node_id, int32 volume) OVERRIDE {
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

  virtual void SetOutputUserMute(bool mute_on) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputUserMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetInputNodeGain(uint64 node_id, int32 input_gain) OVERRIDE {
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

  virtual void AddActiveInputNode(uint64 node_id) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kAddActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void RemoveActiveInputNode(uint64 node_id) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kRemoveActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void AddActiveOutputNode(uint64 node_id) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kAddActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void RemoveActiveOutputNode(uint64 node_id) OVERRIDE {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kRemoveActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            dbus::ObjectProxy::EmptyResponseCallback());
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
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
    FOR_EACH_OBSERVER(Observer, observers_, AudioClientRestarted());
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
    FOR_EACH_OBSERVER(Observer, observers_, OutputMuteChanged(user_mute));
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
    bool success = true;
    VolumeState volume_state;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopInt32(&volume_state.output_volume) ||
          !reader.PopBool(&volume_state.output_system_mute) ||
          !reader.PopInt32(&volume_state.input_gain) ||
          !reader.PopBool(&volume_state.input_mute) ||
          !reader.PopBool(&volume_state.output_user_mute)) {
        success = false;
        LOG(ERROR) << "Error reading response from cras: "
                   << response->ToString();
      }
    } else {
      success = false;
      LOG(ERROR) << "Error calling " << cras::kGetVolumeState;
    }

    callback.Run(volume_state, success);
  }

  void OnGetNodes(const GetNodesCallback& callback,
                  dbus::Response* response) {
    bool success = true;
    AudioNodeList node_list;
    if (response) {
      dbus::MessageReader response_reader(response);
      dbus::MessageReader array_reader(response);
      while (response_reader.HasMoreData()) {
        if (!response_reader.PopArray(&array_reader)) {
          success = false;
          LOG(ERROR) << "Error reading response from cras: "
                     << response->ToString();
          break;
        }

        AudioNode node;
        if (!GetAudioNode(response, &array_reader, &node)) {
          success = false;
          LOG(WARNING) << "Error reading audio node data from cras: "
                       << response->ToString();
          break;
        }
        // Filter out the "UNKNOWN" type of audio devices.
        if (node.type != "UNKNOWN")
          node_list.push_back(node);
      }
    }

    if (node_list.empty())
      return;

    callback.Run(node_list, success);
  }

  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    error_callback.Run(error_name, error_message);
  }

  bool GetAudioNode(dbus::Response* response,
                    dbus::MessageReader* array_reader,
                    AudioNode *node) {
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
      }
    }

    return true;
  }

  dbus::ObjectProxy* cras_proxy_;
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrasAudioClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioClientImpl);
};

CrasAudioClient::Observer::~Observer() {
}

void CrasAudioClient::Observer::AudioClientRestarted() {
}

void CrasAudioClient::Observer::OutputMuteChanged(bool mute_on) {
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
CrasAudioClient* CrasAudioClient::Create() {
  return new CrasAudioClientImpl();
}

}  // namespace chromeos
