// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/sms_watcher.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

int decode_bcd(const char *s) {
  return (s[0] - '0') * 10 + s[1] - '0';
}

// Callback for Delete() method.  This method does nothing.
void DeleteSMSCallback() {}

}  // namespace

const char SMSWatcher::kNumberKey[] = "number";
const char SMSWatcher::kTextKey[] = "text";
const char SMSWatcher::kTimestampKey[] = "timestamp";
const char SMSWatcher::kSmscKey[] = "smsc";
const char SMSWatcher::kValidityKey[] = "validity";
const char SMSWatcher::kClassKey[] = "class";
const char SMSWatcher::kIndexKey[] = "index";

SMSWatcher::SMSWatcher(const std::string& modem_device_path,
                       MonitorSMSCallback callback,
                       void* object)
    : weak_ptr_factory_(this),
      device_path_(modem_device_path),
      callback_(callback),
      object_(object) {
  DBusThreadManager::Get()->GetFlimflamDeviceClient()->GetProperties(
      dbus::ObjectPath(modem_device_path),
      base::Bind(&SMSWatcher::DevicePropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

SMSWatcher::~SMSWatcher() {
  if (!dbus_connection_.empty() && !object_path_.value().empty()) {
    DBusThreadManager::Get()->GetGsmSMSClient()->ResetSmsReceivedHandler(
        dbus_connection_, object_path_);
  }
}

void SMSWatcher::DevicePropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS)
    return;

  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kDBusConnectionProperty, &dbus_connection_)) {
    LOG(WARNING) << "Modem device properties do not include DBus connection.";
    return;
  }

  std::string object_path_string;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kDBusObjectProperty, &object_path_string)) {
    LOG(WARNING) << "Modem device properties do not include DBus object.";
    return;
  }
  object_path_ = dbus::ObjectPath(object_path_string);

  DBusThreadManager::Get()->GetGsmSMSClient()->SetSmsReceivedHandler(
      dbus_connection_,
      object_path_,
      base::Bind(&SMSWatcher::OnSmsReceived, weak_ptr_factory_.GetWeakPtr()));

  DBusThreadManager::Get()->GetGsmSMSClient()->List(
      dbus_connection_, object_path_,
      base::Bind(&SMSWatcher::ListSMSCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SMSWatcher::OnSmsReceived(uint32 index, bool complete) {
  // Only handle complete messages.
  if (!complete)
    return;
  DBusThreadManager::Get()->GetGsmSMSClient()->Get(
      dbus_connection_, object_path_, index,
      base::Bind(&SMSWatcher::GetSMSCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 index));
}

void SMSWatcher::RunCallbackWithSMS(
    const base::DictionaryValue& sms_dictionary) {
  SMS sms;

  std::string number;
  if (!sms_dictionary.GetStringWithoutPathExpansion(kNumberKey, &number))
    LOG(WARNING) << "SMS did not contain a number";
  sms.number = number.c_str();

  std::string text;
  if (!sms_dictionary.GetStringWithoutPathExpansion(kTextKey, &text)) {
    LOG(WARNING) << "SMS did not contain message text";
  }
  sms.text = text.c_str();

  std::string sms_timestamp;
  if (sms_dictionary.GetStringWithoutPathExpansion(kTimestampKey,
                                                   &sms_timestamp)) {
    base::Time::Exploded exp;
    exp.year = decode_bcd(&sms_timestamp[0]);
    if (exp.year > 95)
      exp.year += 1900;
    else
      exp.year += 2000;
    exp.month = decode_bcd(&sms_timestamp[2]);
    exp.day_of_month = decode_bcd(&sms_timestamp[4]);
    exp.hour = decode_bcd(&sms_timestamp[6]);
    exp.minute = decode_bcd(&sms_timestamp[8]);
    exp.second = decode_bcd(&sms_timestamp[10]);
    exp.millisecond = 0;
    sms.timestamp = base::Time::FromUTCExploded(exp);
    int hours = decode_bcd(&sms_timestamp[13]);
    if (sms_timestamp[12] == '-')
      hours = -hours;
    sms.timestamp -= base::TimeDelta::FromHours(hours);
  } else {
    LOG(WARNING) << "SMS did not contain a timestamp";
    sms.timestamp = base::Time();
  }

  std::string smsc;
  if (!sms_dictionary.GetStringWithoutPathExpansion(kSmscKey, &smsc))
    sms.smsc = NULL;
  else
    sms.smsc = smsc.c_str();

  double validity = 0;
  if (!sms_dictionary.GetDoubleWithoutPathExpansion(kValidityKey, &validity))
    validity = -1;
  sms.validity = validity;

  double msgclass = 0;
  if (!sms_dictionary.GetDoubleWithoutPathExpansion(kClassKey, &msgclass))
    msgclass = -1;
  sms.msgclass = msgclass;

  callback_(object_, device_path_.c_str(), &sms);
}

void SMSWatcher::GetSMSCallback(uint32 index,
                                const base::DictionaryValue& sms_dictionary) {
  RunCallbackWithSMS(sms_dictionary);

  DBusThreadManager::Get()->GetGsmSMSClient()->Delete(
      dbus_connection_, object_path_, index, base::Bind(&DeleteSMSCallback));
}

void SMSWatcher::ListSMSCallback(const base::ListValue& result) {
  // List() is called only once and no one touches delete_queue_ before List().
  CHECK(delete_queue_.empty());
  for (size_t i = 0; i != result.GetSize(); ++i) {
    base::DictionaryValue* sms_dictionary = NULL;
    if (!result.GetDictionary(i, &sms_dictionary)) {
      LOG(ERROR) << "result[" << i << "] is not a dictionary.";
      continue;
    }
    RunCallbackWithSMS(*sms_dictionary);
    double index = 0;
    if (sms_dictionary->GetDoubleWithoutPathExpansion(kIndexKey, &index))
      delete_queue_.push_back(index);
  }
  DeleteSMSInChain();
}

void SMSWatcher::DeleteSMSInChain() {
  if (!delete_queue_.empty()) {
    DBusThreadManager::Get()->GetGsmSMSClient()->Delete(
        dbus_connection_, object_path_, delete_queue_.back(),
        base::Bind(&SMSWatcher::DeleteSMSInChain,
                   weak_ptr_factory_.GetWeakPtr()));
    delete_queue_.pop_back();
  }
}

}  // namespace chromeos
