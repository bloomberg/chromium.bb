// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/gsm_sms_client.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// A class actually making method calls for SMS services, used by
// GsmSMSClietnImpl.
class SMSProxy {
 public:
  typedef GsmSMSClient::SmsReceivedHandler SmsReceivedHandler;
  typedef GsmSMSClient::DeleteCallback DeleteCallback;
  typedef GsmSMSClient::GetCallback GetCallback;
  typedef GsmSMSClient::ListCallback ListCallback;

  SMSProxy(dbus::Bus* bus,
           const std::string& service_name,
           const dbus::ObjectPath& object_path)
      : proxy_(bus->GetObjectProxy(service_name, object_path)),
        weak_ptr_factory_(this) {
    proxy_->ConnectToSignal(
        modemmanager::kModemManagerSMSInterface,
        modemmanager::kSMSReceivedSignal,
        base::Bind(&SMSProxy::OnSmsReceived, weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SMSProxy::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

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
  void Delete(uint32 index, const DeleteCallback& callback) {
    dbus::MethodCall method_call(modemmanager::kModemManagerSMSInterface,
                                 modemmanager::kSMSDeleteFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(index);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&SMSProxy::OnDelete,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // Calls Get method.
  void Get(uint32 index, const GetCallback& callback) {
    dbus::MethodCall method_call(modemmanager::kModemManagerSMSInterface,
                                 modemmanager::kSMSGetFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(index);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&SMSProxy::OnGet,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

  // Calls List method.
  void List(const ListCallback& callback) {
    dbus::MethodCall method_call(modemmanager::kModemManagerSMSInterface,
                                 modemmanager::kSMSListFunction);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&SMSProxy::OnList,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

 private:
  // Handles SmsReceived signal.
  void OnSmsReceived(dbus::Signal* signal) {
    uint32 index = 0;
    bool complete = false;
    dbus::MessageReader reader(signal);
    if (!reader.PopUint32(&index) ||
        !reader.PopBool(&complete)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!sms_received_handler_.is_null())
      sms_received_handler_.Run(index, complete);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool successed) {
    LOG_IF(ERROR, !successed) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  // Handles responses of Delete method calls.
  void OnDelete(const DeleteCallback& callback, dbus::Response* response) {
    if (!response)
      return;
    callback.Run();
  }

  // Handles responses of Get method calls.
  void OnGet(const GetCallback& callback, dbus::Response* response) {
    if (!response)
      return;
    dbus::MessageReader reader(response);
    scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
    base::DictionaryValue* dictionary_value = NULL;
    if (!value.get() || !value->GetAsDictionary(&dictionary_value)) {
      LOG(WARNING) << "Invalid response: " << response->ToString();
      return;
    }
    callback.Run(*dictionary_value);
  }

  // Handles responses of List method calls.
  void OnList(const ListCallback& callback, dbus::Response* response) {
    if (!response)
      return;
    dbus::MessageReader reader(response);
    scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
    base::ListValue* list_value = NULL;
    if (!value.get() || !value->GetAsList(&list_value)) {
      LOG(WARNING) << "Invalid response: " << response->ToString();
      return;
    }
    callback.Run(*list_value);
  }

  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<SMSProxy> weak_ptr_factory_;
  SmsReceivedHandler sms_received_handler_;

  DISALLOW_COPY_AND_ASSIGN(SMSProxy);
};

// The GsmSMSClient implementation.
class GsmSMSClientImpl : public GsmSMSClient {
 public:
  explicit GsmSMSClientImpl(dbus::Bus* bus)
      : bus_(bus),
        proxies_deleter_(&proxies_) {
  }

  // GsmSMSClient override.
  virtual void SetSmsReceivedHandler(
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      const SmsReceivedHandler& handler) OVERRIDE {
    GetProxy(service_name, object_path)->SetSmsReceivedHandler(handler);
  }

  // GsmSMSClient override.
  virtual void ResetSmsReceivedHandler(
      const std::string& service_name,
      const dbus::ObjectPath& object_path) OVERRIDE {
    GetProxy(service_name, object_path)->ResetSmsReceivedHandler();
  }

  // GsmSMSClient override.
  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      uint32 index,
                      const DeleteCallback& callback) OVERRIDE {
    GetProxy(service_name, object_path)->Delete(index, callback);
  }

  // GsmSMSClient override.
  virtual void Get(const std::string& service_name,
                   const dbus::ObjectPath& object_path,
                   uint32 index,
                   const GetCallback& callback) OVERRIDE {
    GetProxy(service_name, object_path)->Get(index, callback);
  }

  // GsmSMSClient override.
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const ListCallback& callback) OVERRIDE {
    GetProxy(service_name, object_path)->List(callback);
  }

  // GsmSMSClient override.
  virtual void RequestUpdate(const std::string& service_name,
                             const dbus::ObjectPath& object_path) {
  }

 private:
  typedef std::map<std::pair<std::string, std::string>, SMSProxy*> ProxyMap;

  // Returns a SMSProxy for the given service name and object path.
  SMSProxy* GetProxy(const std::string& service_name,
                     const dbus::ObjectPath& object_path) {
    const ProxyMap::key_type key(service_name, object_path.value());
    ProxyMap::iterator it = proxies_.find(key);
    if (it != proxies_.end())
      return it->second;

    // There is no proxy for the service_name and object_path, create it.
    SMSProxy* proxy = new SMSProxy(bus_, service_name, object_path);
    proxies_.insert(ProxyMap::value_type(key, proxy));
    return proxy;
  }

  dbus::Bus* bus_;
  ProxyMap proxies_;
  STLValueDeleter<ProxyMap> proxies_deleter_;

  DISALLOW_COPY_AND_ASSIGN(GsmSMSClientImpl);
};

// A stub implementaion of GsmSMSClient.
class GsmSMSClientStubImpl : public GsmSMSClient {
 public:
  GsmSMSClientStubImpl() : test_index_(-1), weak_ptr_factory_(this) {
    test_messages_.push_back("Test Message 0");
    test_messages_.push_back("Test Message 1");
    test_messages_.push_back("Test a relatively long message 2");
    test_messages_.push_back("Test a very, the quick brown fox jumped"
                             " over the lazy dog, long message 3");
    test_messages_.push_back("Test Message 4");
    test_messages_.push_back("Test Message 5");
    test_messages_.push_back("Test Message 6");
  }

  virtual ~GsmSMSClientStubImpl() {}

  // GsmSMSClient override.
  virtual void SetSmsReceivedHandler(
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      const SmsReceivedHandler& handler) OVERRIDE {
    handler_ = handler;
  }

  // GsmSMSClient override.
  virtual void ResetSmsReceivedHandler(
      const std::string& service_name,
      const dbus::ObjectPath& object_path) OVERRIDE {
    handler_.Reset();
  }

  // GsmSMSClient override.
  virtual void Delete(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      uint32 index,
                      const DeleteCallback& callback) OVERRIDE {
    message_list_.Remove(index, NULL);
    callback.Run();
  }

  // GsmSMSClient override.
  virtual void Get(const std::string& service_name,
                   const dbus::ObjectPath& object_path,
                   uint32 index,
                   const GetCallback& callback) OVERRIDE {
    base::DictionaryValue* dictionary = NULL;
    if (message_list_.GetDictionary(index, &dictionary)) {
      callback.Run(*dictionary);
      return;
    }
    base::DictionaryValue empty_dictionary;
    callback.Run(empty_dictionary);
  }

  // GsmSMSClient override.
  virtual void List(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const ListCallback& callback) OVERRIDE {
    callback.Run(message_list_);
  }

  // GsmSMSClient override.
  virtual void RequestUpdate(const std::string& service_name,
                             const dbus::ObjectPath& object_path) {
    if (test_index_ >= 0)
      return;
    test_index_ = 0;
    // Call PushTestMessageChain asynchronously so that the handler_ callback
    // does not get called from the update request.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&GsmSMSClientStubImpl::PushTestMessageChain,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void PushTestMessageChain() {
    if (PushTestMessage())
      PushTestMessageDelayed();
  }

  void PushTestMessageDelayed() {
    const int kSmsMessageDelaySeconds = 5;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GsmSMSClientStubImpl::PushTestMessageChain,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kSmsMessageDelaySeconds));
  }

  bool PushTestMessage() {
    if (test_index_ >= static_cast<int>(test_messages_.size()))
      return false;
    base::DictionaryValue* message = new base::DictionaryValue;
    message->SetString("number", "000-000-0000");
    message->SetString("text", test_messages_[test_index_]);
    message->SetInteger("index", test_index_);
    int msg_index = message_list_.GetSize();
    message_list_.Append(message);
    if (!handler_.is_null())
      handler_.Run(msg_index, true);
    ++test_index_;
    return true;
  }

  int test_index_;
  std::vector<std::string> test_messages_;
  base::ListValue message_list_;
  SmsReceivedHandler handler_;
  base::WeakPtrFactory<GsmSMSClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GsmSMSClientStubImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GsmSMSClient

GsmSMSClient::GsmSMSClient() {}

GsmSMSClient::~GsmSMSClient() {}

// static
GsmSMSClient* GsmSMSClient::Create(DBusClientImplementationType type,
                                   dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new GsmSMSClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new GsmSMSClientStubImpl();
}

}  // namespace chromeos
