// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/modem_messaging_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// A class which makes method calls for SMS services via the
// org.freedesktop.ModemManager1.Messaging object.
class ModemMessagingProxy {
 public:
  typedef ModemMessagingClient::SmsReceivedHandler SmsReceivedHandler;
  typedef ModemMessagingClient::ListCallback ListCallback;
  typedef ModemMessagingClient::DeleteCallback DeleteCallback;

  ModemMessagingProxy(dbus::Bus* bus,
           const std::string& service_name,
           const dbus::ObjectPath& object_path)
      : bus_(bus),
        proxy_(bus->GetObjectProxy(service_name, object_path)),
        service_name_(service_name),
        weak_ptr_factory_(this) {
    proxy_->ConnectToSignal(
        modemmanager::kModemManager1MessagingInterface,
        modemmanager::kSMSAddedSignal,
        base::Bind(&ModemMessagingProxy::OnSmsAdded,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&ModemMessagingProxy::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  virtual ~ModemMessagingProxy() {}

  // Sets SmsReceived signal handler.
  void SetSmsReceivedHandler(const SmsReceivedHandler& handler) {
    DCHECK(sms_received_handler_.is_null());
    sms_received_handler_ = handler;
  }

  // Resets SmsReceived signal handler.
  void ResetSmsReceivedHandler() {
    sms_received_handler_.Reset();
  }

  // Calls Delete method.
  void Delete(const dbus::ObjectPath& message_path,
              const DeleteCallback& callback) {
    dbus::MethodCall method_call(modemmanager::kModemManager1MessagingInterface,
                                 modemmanager::kSMSDeleteFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(message_path);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&ModemMessagingProxy::OnDelete,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // Calls List method.
  virtual void List(const ListCallback& callback) {
    dbus::MethodCall method_call(modemmanager::kModemManager1MessagingInterface,
                                 modemmanager::kSMSListFunction);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&ModemMessagingProxy::OnList,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

 private:
  // Handles SmsAdded signal.
  void OnSmsAdded(dbus::Signal* signal) {
    dbus::ObjectPath message_path;
    bool complete = false;
    dbus::MessageReader reader(signal);
    if (!reader.PopObjectPath(&message_path) ||
        !reader.PopBool(&complete)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!sms_received_handler_.is_null()) {
      sms_received_handler_.Run(message_path, complete);
    }
  }

  // Handles responses of Delete method calls.
  void OnDelete(const DeleteCallback& callback, dbus::Response* response) {
    if (!response)
      return;
    callback.Run();
  }

  // Handles responses of List method calls.
  void OnList(const ListCallback& callback, dbus::Response* response) {
    if (!response)
      return;
    dbus::MessageReader reader(response);
    std::vector<dbus::ObjectPath> sms_paths;
    if (!reader.PopArrayOfObjectPaths(&sms_paths))
      LOG(WARNING) << "Invalid response: " << response->ToString();
    callback.Run(sms_paths);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " "
                              << signal << " failed.";
  }

  dbus::Bus* bus_;
  dbus::ObjectProxy* proxy_;
  std::string service_name_;
  SmsReceivedHandler sms_received_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ModemMessagingProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModemMessagingProxy);
};

class CHROMEOS_EXPORT ModemMessagingClientImpl : public ModemMessagingClient {
 public:
  ModemMessagingClientImpl()
      : bus_(NULL),
        proxies_deleter_(&proxies_) {
  }

  virtual void SetSmsReceivedHandler(
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      const SmsReceivedHandler& handler) OVERRIDE {
    GetProxy(service_name, object_path)->SetSmsReceivedHandler(handler);
  }

  virtual void ResetSmsReceivedHandler(
      const std::string& service_name,
      const dbus::ObjectPath& object_path) OVERRIDE {
    GetProxy(service_name, object_path)->ResetSmsReceivedHandler();
  }

  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      const dbus::ObjectPath& sms_path,
                      const DeleteCallback& callback) OVERRIDE {
    GetProxy(service_name, object_path)->Delete(sms_path, callback);
  }

  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const ListCallback& callback) OVERRIDE {
    GetProxy(service_name, object_path)->List(callback);
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    bus_ = bus;
  };

 private:
  typedef std::map<std::pair<std::string, std::string>, ModemMessagingProxy*>
      ProxyMap;

  // Returns a SMSProxy for the given service name and object path.
  ModemMessagingProxy* GetProxy(const std::string& service_name,
                                const dbus::ObjectPath& object_path) {
    const ProxyMap::key_type key(service_name, object_path.value());
    ProxyMap::iterator it = proxies_.find(key);
    if (it != proxies_.end())
      return it->second;

    // There is no proxy for the service_name and object_path, create it.
    ModemMessagingProxy* proxy
        = new ModemMessagingProxy(bus_, service_name, object_path);
    proxies_.insert(ProxyMap::value_type(key, proxy));
    return proxy;
  }

  dbus::Bus* bus_;
  ProxyMap proxies_;
  STLValueDeleter<ProxyMap> proxies_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ModemMessagingClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ModemMessagingClient

ModemMessagingClient::ModemMessagingClient() {}

ModemMessagingClient::~ModemMessagingClient() {}


// static
ModemMessagingClient* ModemMessagingClient::Create() {
  return new ModemMessagingClientImpl();
}


}  // namespace chromeos
