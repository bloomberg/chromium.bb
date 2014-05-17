// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_sms_handler.h"

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/sms_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Not exposed/exported:
const char kIndexKey[] = "index";

// Keys from ModemManager1
const char kModemManager1NumberKey[] = "Number";
const char kModemManager1TextKey[] = "Text";
const char kModemManager1TimestampKey[] = "Timestamp";

// Maximum number of messages stored for RequestUpdate(true).
const size_t kMaxReceivedMessages = 100;

}  // namespace

namespace chromeos {

// static
const char NetworkSmsHandler::kNumberKey[] = "number";
const char NetworkSmsHandler::kTextKey[] = "text";
const char NetworkSmsHandler::kTimestampKey[] = "timestamp";

class NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  NetworkSmsDeviceHandler() {}
  virtual ~NetworkSmsDeviceHandler() {}

  virtual void RequestUpdate() = 0;
};

class NetworkSmsHandler::ModemManagerNetworkSmsDeviceHandler
    : public NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  ModemManagerNetworkSmsDeviceHandler(NetworkSmsHandler* host,
                                      const std::string& service_name,
                                      const dbus::ObjectPath& object_path);

  virtual void RequestUpdate() OVERRIDE;

 private:
  void ListCallback(const base::ListValue& message_list);
  void SmsReceivedCallback(uint32 index, bool complete);
  void GetCallback(uint32 index, const base::DictionaryValue& dictionary);
  void DeleteMessages();
  void MessageReceived(const base::DictionaryValue& dictionary);

  NetworkSmsHandler* host_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  bool deleting_messages_;
  base::WeakPtrFactory<ModemManagerNetworkSmsDeviceHandler> weak_ptr_factory_;
  std::vector<uint32> delete_queue_;

  DISALLOW_COPY_AND_ASSIGN(ModemManagerNetworkSmsDeviceHandler);
};

NetworkSmsHandler::
ModemManagerNetworkSmsDeviceHandler::ModemManagerNetworkSmsDeviceHandler(
    NetworkSmsHandler* host,
    const std::string& service_name,
    const dbus::ObjectPath& object_path)
    : host_(host),
      service_name_(service_name),
      object_path_(object_path),
      deleting_messages_(false),
      weak_ptr_factory_(this) {
  // Set the handler for received Sms messaages.
  DBusThreadManager::Get()->GetGsmSMSClient()->SetSmsReceivedHandler(
      service_name_, object_path_,
      base::Bind(&ModemManagerNetworkSmsDeviceHandler::SmsReceivedCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  // List the existing messages.
  DBusThreadManager::Get()->GetGsmSMSClient()->List(
      service_name_, object_path_,
      base::Bind(&NetworkSmsHandler::
                 ModemManagerNetworkSmsDeviceHandler::ListCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::ModemManagerNetworkSmsDeviceHandler::RequestUpdate() {
  DBusThreadManager::Get()->GetGsmSMSClient()->RequestUpdate(
      service_name_, object_path_);
}

void NetworkSmsHandler::ModemManagerNetworkSmsDeviceHandler::ListCallback(
    const base::ListValue& message_list) {
  // This receives all messages, so clear any pending deletes.
  delete_queue_.clear();
  for (base::ListValue::const_iterator iter = message_list.begin();
       iter != message_list.end(); ++iter) {
    base::DictionaryValue* message = NULL;
    if (!(*iter)->GetAsDictionary(&message))
      continue;
    MessageReceived(*message);
    double index = 0;
    if (message->GetDoubleWithoutPathExpansion(kIndexKey, &index))
      delete_queue_.push_back(static_cast<uint32>(index));
  }
  DeleteMessages();
}

// Messages must be deleted one at a time, since we can not guarantee
// the order the deletion will be executed in. Delete messages from
// the back of the list so that the indices are valid.
void NetworkSmsHandler::ModemManagerNetworkSmsDeviceHandler::DeleteMessages() {
  if (delete_queue_.empty()) {
    deleting_messages_ = false;
    return;
  }
  deleting_messages_ = true;
  uint32 index = delete_queue_.back();
  delete_queue_.pop_back();
  DBusThreadManager::Get()->GetGsmSMSClient()->Delete(
      service_name_, object_path_, index,
      base::Bind(&NetworkSmsHandler::
                 ModemManagerNetworkSmsDeviceHandler::DeleteMessages,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::
ModemManagerNetworkSmsDeviceHandler::SmsReceivedCallback(
    uint32 index,
    bool complete) {
  // Only handle complete messages.
  if (!complete)
    return;
  DBusThreadManager::Get()->GetGsmSMSClient()->Get(
      service_name_, object_path_, index,
      base::Bind(&NetworkSmsHandler::
                 ModemManagerNetworkSmsDeviceHandler::GetCallback,
                 weak_ptr_factory_.GetWeakPtr(), index));
}

void NetworkSmsHandler::ModemManagerNetworkSmsDeviceHandler::GetCallback(
    uint32 index,
    const base::DictionaryValue& dictionary) {
  MessageReceived(dictionary);
  delete_queue_.push_back(index);
  if (!deleting_messages_)
    DeleteMessages();
}

void NetworkSmsHandler::
ModemManagerNetworkSmsDeviceHandler::MessageReceived(
    const base::DictionaryValue& dictionary) {
  // The keys of the ModemManager.Modem.Gsm.SMS interface match the
  // exported keys, so the dictionary used as a notification argument
  // unchanged.
  host_->MessageReceived(dictionary);
}

class NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler
    : public NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  ModemManager1NetworkSmsDeviceHandler(NetworkSmsHandler* host,
                                       const std::string& service_name,
                                       const dbus::ObjectPath& object_path);

  virtual void RequestUpdate() OVERRIDE;

 private:
  void ListCallback(const std::vector<dbus::ObjectPath>& paths);
  void SmsReceivedCallback(const dbus::ObjectPath& path, bool complete);
  void GetCallback(const base::DictionaryValue& dictionary);
  void DeleteMessages();
  void GetMessages();
  void MessageReceived(const base::DictionaryValue& dictionary);

  NetworkSmsHandler* host_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  bool deleting_messages_;
  bool retrieving_messages_;
  base::WeakPtrFactory<ModemManager1NetworkSmsDeviceHandler> weak_ptr_factory_;
  std::vector<dbus::ObjectPath> delete_queue_;
  std::deque<dbus::ObjectPath> retrieval_queue_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager1NetworkSmsDeviceHandler);
};

NetworkSmsHandler::
ModemManager1NetworkSmsDeviceHandler::ModemManager1NetworkSmsDeviceHandler(
    NetworkSmsHandler* host,
    const std::string& service_name,
    const dbus::ObjectPath& object_path)
    : host_(host),
      service_name_(service_name),
      object_path_(object_path),
      deleting_messages_(false),
      retrieving_messages_(false),
      weak_ptr_factory_(this) {
  // Set the handler for received Sms messaages.
  DBusThreadManager::Get()->GetModemMessagingClient()->SetSmsReceivedHandler(
      service_name_, object_path_,
      base::Bind(
          &NetworkSmsHandler::
          ModemManager1NetworkSmsDeviceHandler::SmsReceivedCallback,
          weak_ptr_factory_.GetWeakPtr()));

  // List the existing messages.
  DBusThreadManager::Get()->GetModemMessagingClient()->List(
      service_name_, object_path_,
      base::Bind(&NetworkSmsHandler::
                 ModemManager1NetworkSmsDeviceHandler::ListCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::RequestUpdate() {
  // Calling List using the service "AddSMS" causes the stub
  // implementation to deliver new sms messages.
  DBusThreadManager::Get()->GetModemMessagingClient()->List(
      std::string("AddSMS"), dbus::ObjectPath("/"),
      base::Bind(&NetworkSmsHandler::
                 ModemManager1NetworkSmsDeviceHandler::ListCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::ListCallback(
    const std::vector<dbus::ObjectPath>& paths) {
  // This receives all messages, so clear any pending gets and deletes.
  retrieval_queue_.clear();
  delete_queue_.clear();

  retrieval_queue_.resize(paths.size());
  std::copy(paths.begin(), paths.end(), retrieval_queue_.begin());
  if (!retrieving_messages_)
    GetMessages();
}

// Messages must be deleted one at a time, since we can not guarantee
// the order the deletion will be executed in. Delete messages from
// the back of the list so that the indices are valid.
void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::DeleteMessages() {
  if (delete_queue_.empty()) {
    deleting_messages_ = false;
    return;
  }
  deleting_messages_ = true;
  dbus::ObjectPath sms_path = delete_queue_.back();
  delete_queue_.pop_back();
  DBusThreadManager::Get()->GetModemMessagingClient()->Delete(
      service_name_, object_path_, sms_path,
      base::Bind(&NetworkSmsHandler::
                 ModemManager1NetworkSmsDeviceHandler::DeleteMessages,
                 weak_ptr_factory_.GetWeakPtr()));
}

// Messages must be fetched one at a time, so that we do not queue too
// many requests to a single threaded server.
void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetMessages() {
  if (retrieval_queue_.empty()) {
    retrieving_messages_ = false;
    if (!deleting_messages_)
      DeleteMessages();
    return;
  }
  retrieving_messages_ = true;
  dbus::ObjectPath sms_path = retrieval_queue_.front();
  retrieval_queue_.pop_front();
  DBusThreadManager::Get()->GetSMSClient()->GetAll(
      service_name_, sms_path,
      base::Bind(&NetworkSmsHandler::
                 ModemManager1NetworkSmsDeviceHandler::GetCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  delete_queue_.push_back(sms_path);
}

void NetworkSmsHandler::
ModemManager1NetworkSmsDeviceHandler::SmsReceivedCallback(
    const dbus::ObjectPath& sms_path,
    bool complete) {
  // Only handle complete messages.
  if (!complete)
    return;
  retrieval_queue_.push_back(sms_path);
  if (!retrieving_messages_)
    GetMessages();
}

void NetworkSmsHandler::ModemManager1NetworkSmsDeviceHandler::GetCallback(
    const base::DictionaryValue& dictionary) {
  MessageReceived(dictionary);
  GetMessages();
}

void NetworkSmsHandler::
ModemManager1NetworkSmsDeviceHandler::MessageReceived(
    const base::DictionaryValue& dictionary) {
  // The keys of the ModemManager1.SMS interface do not match the
  // exported keys, so a new dictionary is created with the expected
  // key namaes.
  base::DictionaryValue new_dictionary;
  std::string text, number, timestamp;
  if (dictionary.GetStringWithoutPathExpansion(kModemManager1NumberKey,
                                               &number))
    new_dictionary.SetString(kNumberKey, number);
  if (dictionary.GetStringWithoutPathExpansion(kModemManager1TextKey, &text))
    new_dictionary.SetString(kTextKey, text);
  // TODO(jglasgow): consider normalizing timestamp.
  if (dictionary.GetStringWithoutPathExpansion(kModemManager1TimestampKey,
                                               &timestamp))
    new_dictionary.SetString(kTimestampKey, timestamp);
  host_->MessageReceived(new_dictionary);
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSmsHandler

NetworkSmsHandler::NetworkSmsHandler()
    : weak_ptr_factory_(this) {
}

NetworkSmsHandler::~NetworkSmsHandler() {
  DBusThreadManager::Get()->GetShillManagerClient()->
      RemovePropertyChangedObserver(this);
}

void NetworkSmsHandler::Init() {
  // Add as an observer here so that new devices added after this call are
  // recognized.
  DBusThreadManager::Get()->GetShillManagerClient()->AddPropertyChangedObserver(
      this);
  // Request network manager properties so that we can get the list of devices.
  DBusThreadManager::Get()->GetShillManagerClient()->GetProperties(
      base::Bind(&NetworkSmsHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::RequestUpdate(bool request_existing) {
  // If we already received messages and |request_existing| is true, send
  // updates for existing messages.
  for (ScopedVector<base::DictionaryValue>::iterator iter =
           received_messages_.begin();
       iter != received_messages_.end(); ++iter) {
    base::DictionaryValue* message = *iter;
    NotifyMessageReceived(*message);
  }
  // Request updates from each device.
  for (ScopedVector<NetworkSmsDeviceHandler>::iterator iter =
           device_handlers_.begin(); iter != device_handlers_.end(); ++iter) {
    (*iter)->RequestUpdate();
  }
}

void NetworkSmsHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkSmsHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkSmsHandler::OnPropertyChanged(const std::string& name,
                                          const base::Value& value) {
  if (name != shill::kDevicesProperty)
    return;
  const base::ListValue* devices = NULL;
  if (!value.GetAsList(&devices) || !devices)
    return;
  UpdateDevices(devices);
}

// Private methods

void NetworkSmsHandler::AddReceivedMessage(
    const base::DictionaryValue& message) {
  base::DictionaryValue* new_message = message.DeepCopy();
  if (received_messages_.size() >= kMaxReceivedMessages)
    received_messages_.erase(received_messages_.begin());
  received_messages_.push_back(new_message);
}

void NetworkSmsHandler::NotifyMessageReceived(
    const base::DictionaryValue& message) {
  FOR_EACH_OBSERVER(Observer, observers_, MessageReceived(message));
}

void NetworkSmsHandler::MessageReceived(const base::DictionaryValue& message) {
  AddReceivedMessage(message);
  NotifyMessageReceived(message);
}

void NetworkSmsHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "NetworkSmsHandler: Failed to get manager properties.";
    return;
  }
  const base::Value* value;
  if (!properties.GetWithoutPathExpansion(shill::kDevicesProperty, &value) ||
      value->GetType() != base::Value::TYPE_LIST) {
    LOG(ERROR) << "NetworkSmsHandler: No list value for: "
               << shill::kDevicesProperty;
    return;
  }
  const base::ListValue* devices = static_cast<const base::ListValue*>(value);
  UpdateDevices(devices);
}

void NetworkSmsHandler::UpdateDevices(const base::ListValue* devices) {
  for (base::ListValue::const_iterator iter = devices->begin();
       iter != devices->end(); ++iter) {
    std::string device_path;
    (*iter)->GetAsString(&device_path);
    if (!device_path.empty()) {
      // Request device properties.
      VLOG(1) << "GetDeviceProperties: " << device_path;
      DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
          dbus::ObjectPath(device_path),
          base::Bind(&NetworkSmsHandler::DevicePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     device_path));
    }
  }
}

void NetworkSmsHandler::DevicePropertiesCallback(
    const std::string& device_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "NetworkSmsHandler: ERROR: " << call_status
               << " For: " << device_path;
    return;
  }

  std::string device_type;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kTypeProperty, &device_type)) {
    LOG(ERROR) << "NetworkSmsHandler: No type for: " << device_path;
    return;
  }
  if (device_type != shill::kTypeCellular)
    return;

  std::string service_name;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kDBusServiceProperty, &service_name)) {
    LOG(ERROR) << "Device has no DBusService Property: " << device_path;
    return;
  }

  std::string object_path_string;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kDBusObjectProperty, &object_path_string)) {
    LOG(ERROR) << "Device has no DBusObject Property: " << device_path;
    return;
  }
  dbus::ObjectPath object_path(object_path_string);
  if (service_name == modemmanager::kModemManager1ServiceName) {
    device_handlers_.push_back(
        new ModemManager1NetworkSmsDeviceHandler(
            this, service_name, object_path));
  } else {
    device_handlers_.push_back(
        new ModemManagerNetworkSmsDeviceHandler(
            this, service_name, object_path));
  }
}

}  // namespace chromeos
