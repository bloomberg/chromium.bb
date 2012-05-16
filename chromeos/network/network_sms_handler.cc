// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_sms_handler.h"

#include <string>

#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "chromeos/dbus/flimflam_manager_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// Not exposed/exported.
namespace {
const char kSmscKey[] = "smsc";
const char kValidityKey[] = "validity";
const char kClassKey[] = "class";
const char kIndexKey[] = "index";
}  // namespace

namespace chromeos {

// static
const char NetworkSmsHandler::kNumberKey[] = "number";
const char NetworkSmsHandler::kTextKey[] = "text";
const char NetworkSmsHandler::kTimestampKey[] = "timestamp";

class NetworkSmsHandler::NetworkSmsDeviceHandler {
 public:
  NetworkSmsDeviceHandler(NetworkSmsHandler* host,
                          std::string dbus_connection,
                          dbus::ObjectPath object_path);

  void RequestUpdate();

 private:
  void ListCallback(const base::ListValue& message_list);
  void SmsReceivedCallback(uint32 index, bool complete);
  void GetCallback(uint32 index, const base::DictionaryValue& dictionary);
  void DeleteMessages();
  void NotifyMessageReceived(const base::DictionaryValue& dictionary);

  NetworkSmsHandler* host_;
  std::string dbus_connection_;
  dbus::ObjectPath object_path_;
  bool deleting_messages_;
  base::WeakPtrFactory<NetworkSmsDeviceHandler> weak_ptr_factory_;
  std::vector<uint32> delete_queue_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSmsDeviceHandler);
};

NetworkSmsHandler::NetworkSmsDeviceHandler::NetworkSmsDeviceHandler(
    NetworkSmsHandler* host,
    std::string dbus_connection,
    dbus::ObjectPath object_path)
    : host_(host),
      dbus_connection_(dbus_connection),
      object_path_(object_path),
      deleting_messages_(false),
      weak_ptr_factory_(this) {
  // Set the handler for received Sms messaages.
  DBusThreadManager::Get()->GetGsmSMSClient()->SetSmsReceivedHandler(
      dbus_connection_, object_path_,
      base::Bind(&NetworkSmsDeviceHandler::SmsReceivedCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  // List the existing messages.
  DBusThreadManager::Get()->GetGsmSMSClient()->List(
      dbus_connection_, object_path_,
      base::Bind(&NetworkSmsDeviceHandler::ListCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::NetworkSmsDeviceHandler::RequestUpdate() {
  DBusThreadManager::Get()->GetGsmSMSClient()->RequestUpdate(
      dbus_connection_, object_path_);
}

void NetworkSmsHandler::NetworkSmsDeviceHandler::ListCallback(
    const base::ListValue& message_list) {
  // This receives all messages, so clear any pending deletes.
  delete_queue_.clear();
  for (base::ListValue::const_iterator iter = message_list.begin();
       iter != message_list.end(); ++iter) {
    base::DictionaryValue* message = NULL;
    if (!(*iter)->GetAsDictionary(&message))
      continue;
    NotifyMessageReceived(*message);
    double index = 0;
    if (message->GetDoubleWithoutPathExpansion(kIndexKey, &index))
      delete_queue_.push_back(static_cast<uint32>(index));
  }
  DeleteMessages();
}

// Messages must be deleted one at a time, since we can not gaurantee the order
// the deletion will be executed in. Delete messages from the back of the list
// so that the indices are valid.
void NetworkSmsHandler::NetworkSmsDeviceHandler::DeleteMessages() {
  if (delete_queue_.empty()) {
    deleting_messages_ = false;
    return;
  }
  deleting_messages_ = true;
  uint32 index = delete_queue_.back();
  delete_queue_.pop_back();
  DBusThreadManager::Get()->GetGsmSMSClient()->Delete(
      dbus_connection_, object_path_, index,
      base::Bind(&NetworkSmsDeviceHandler::DeleteMessages,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::NetworkSmsDeviceHandler::SmsReceivedCallback(
    uint32 index,
    bool complete) {
  // Only handle complete messages.
  if (!complete)
    return;
  DBusThreadManager::Get()->GetGsmSMSClient()->Get(
      dbus_connection_, object_path_, index,
      base::Bind(&NetworkSmsDeviceHandler::GetCallback,
                 weak_ptr_factory_.GetWeakPtr(), index));
}

void NetworkSmsHandler::NetworkSmsDeviceHandler::GetCallback(
    uint32 index,
    const base::DictionaryValue& dictionary) {
  NotifyMessageReceived(dictionary);
  delete_queue_.push_back(index);
  if (!deleting_messages_)
    DeleteMessages();
}

void NetworkSmsHandler::NetworkSmsDeviceHandler::NotifyMessageReceived(
    const base::DictionaryValue& dictionary) {
  host_->NotifyMessageReceived(dictionary);
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSmsHandler

NetworkSmsHandler::NetworkSmsHandler()
    : weak_ptr_factory_(this) {
}

NetworkSmsHandler::~NetworkSmsHandler() {
}

void NetworkSmsHandler::Init() {
  // Request network manager properties so that we can get the list of devices.
  DBusThreadManager::Get()->GetFlimflamManagerClient()->GetProperties(
      base::Bind(&NetworkSmsHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkSmsHandler::RequestUpdate() {
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

void NetworkSmsHandler::NotifyMessageReceived(
    const base::DictionaryValue& message) {
  FOR_EACH_OBSERVER(Observer, observers_, MessageReceived(message));
}

void NetworkSmsHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "NetworkSmsHandler: Failed to get manager properties.";
    return;
  }
  base::Value* value;
  if (!properties.GetWithoutPathExpansion(flimflam::kDevicesProperty, &value) ||
      value->GetType() != base::Value::TYPE_LIST) {
    LOG(ERROR) << "NetworkSmsDeviceHandler: No list value for: "
               << flimflam::kDevicesProperty;
    return;
  }
  const base::ListValue* devices = static_cast<const base::ListValue*>(value);
  for (base::ListValue::const_iterator iter = devices->begin();
       iter != devices->end(); ++iter) {
    std::string device_path;
    (*iter)->GetAsString(&device_path);
    if (!device_path.empty()) {
      // Request device properties.
      DBusThreadManager::Get()->GetFlimflamDeviceClient()->GetProperties(
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
  if (call_status != DBUS_METHOD_CALL_SUCCESS)
    return;

  std::string device_type;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kTypeProperty, &device_type)) {
    LOG(ERROR) << "NetworkSmsDeviceHandler: No type for: " << device_path;
    return;
  }
  if (device_type != flimflam::kTypeCellular)
    return;

  std::string dbus_connection;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kDBusConnectionProperty, &dbus_connection)) {
    LOG(ERROR) << "Device has no DBusConnection Property: " << device_path;
    return;
  }

  std::string object_path_string;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kDBusObjectProperty, &object_path_string)) {
    LOG(ERROR) << "Device has no DBusObject Property: " << device_path;
    return;
  }
  dbus::ObjectPath object_path(object_path_string);
  device_handlers_.push_back(
      new NetworkSmsDeviceHandler(this, dbus_connection, object_path));
}


}  // namespace chromeos
