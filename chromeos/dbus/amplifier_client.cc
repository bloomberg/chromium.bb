// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/amplifier_client.h"

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
namespace amplifier {

const char kAmplifierInterface[] = "org.chromium.Amplifier";
const char kAmplifierServiceName[] = "org.chromium.Amplifier";
const char kAmplifierServicePath[] = "/org/chromium/Amplifier";
const char kInitializeMethod[] = "Initialize";
const char kSetStandbyModeMethod[] = "SetStandbyMode";
const char kSetVolumeMethod[] = "SetVolume";
const char kErrorSignal[] = "Error";

}  // namespace amplifier

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

void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                      dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

// The AmplifierClient implementation.
class AmplifierClientImpl : public AmplifierClient {
 public:
  AmplifierClientImpl()
      : proxy_(nullptr), signal_connected_(false), weak_ptr_factory_(this) {}

  ~AmplifierClientImpl() override {}

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // AmplifierClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Initialize(const BoolDBusMethodCallback& callback) override;
  void SetStandbyMode(bool standby,
                      const VoidDBusMethodCallback& callback) override;
  void SetVolume(double db_spl,
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
  base::WeakPtrFactory<AmplifierClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AmplifierClientImpl);
};

void AmplifierClientImpl::Init(dbus::Bus* bus) {
  proxy_ =
      bus->GetObjectProxy(amplifier::kAmplifierServiceName,
                          dbus::ObjectPath(amplifier::kAmplifierServicePath));
  DCHECK(proxy_);
}

void AmplifierClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (!signal_connected_ && proxy_) {
    signal_connected_ = true;
    proxy_->ConnectToSignal(amplifier::kAmplifierInterface,
                            amplifier::kErrorSignal,
                            base::Bind(&AmplifierClientImpl::OnError,
                                       weak_ptr_factory_.GetWeakPtr()),
                            base::Bind(&AmplifierClientImpl::OnSignalConnected,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
  observers_.AddObserver(observer);
}

void AmplifierClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AmplifierClientImpl::Initialize(const BoolDBusMethodCallback& callback) {
  dbus::MethodCall method_call(amplifier::kAmplifierInterface,
                               amplifier::kInitializeMethod);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnBoolDBusMethod, callback));
}

void AmplifierClientImpl::SetStandbyMode(
    bool standby,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(amplifier::kAmplifierInterface,
                               amplifier::kSetStandbyModeMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(standby);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AmplifierClientImpl::SetVolume(double db_spl,
                                    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(amplifier::kAmplifierInterface,
                               amplifier::kSetVolumeMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(db_spl);
  DCHECK(proxy_);
  proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                     base::Bind(&OnVoidDBusMethod, callback));
}

void AmplifierClientImpl::OnError(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  int32 error_code = 0;
  if (!reader.PopInt32(&error_code)) {
    LOG(ERROR) << "Invalid signal: " << signal->ToString();
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnError(error_code));
}

void AmplifierClientImpl::OnSignalConnected(const std::string& interface,
                                            const std::string& signal,
                                            bool succeeded) {
  LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " << signal
                            << " failed.";
}

}  // anonymous namespace

AmplifierClient::AmplifierClient() {
}

AmplifierClient::~AmplifierClient() {
}

// static
AmplifierClient* AmplifierClient::Create() {
  return new AmplifierClientImpl();
}

}  // namespace chromeos
