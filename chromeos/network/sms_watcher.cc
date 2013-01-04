// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/sms_watcher.h"

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/sms_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

int decode_bcd(const char *s) {
  return (s[0] - '0') * 10 + s[1] - '0';
}

void decode_timestamp(const std::string& sms_timestamp, SMS *sms) {
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
  sms->timestamp = base::Time::FromUTCExploded(exp);
  int hours = decode_bcd(&sms_timestamp[13]);
  if (sms_timestamp[12] == '-')
    hours = -hours;
  sms->timestamp -= base::TimeDelta::FromHours(hours);
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

const char SMSWatcher::kModemManager1NumberKey[] = "Number";
const char SMSWatcher::kModemManager1TextKey[] = "Text";
const char SMSWatcher::kModemManager1TimestampKey[] = "Timestamp";
const char SMSWatcher::kModemManager1SmscKey[] = "Smsc";
const char SMSWatcher::kModemManager1ValidityKey[] = "Validity";
const char SMSWatcher::kModemManager1ClassKey[] = "Class";
const char SMSWatcher::kModemManager1IndexKey[] = "Index";

class SMSWatcher::WatcherBase {
 public:
  WatcherBase(const std::string& device_path,
              MonitorSMSCallback callback,
              const std::string& dbus_connection,
              const dbus::ObjectPath& object_path) :
      device_path_(device_path),
      callback_(callback),
      dbus_connection_(dbus_connection),
      object_path_(object_path) {}

  virtual ~WatcherBase() {}

 protected:
  const std::string device_path_;
  MonitorSMSCallback callback_;
  const std::string dbus_connection_;
  const dbus::ObjectPath object_path_;

  DISALLOW_COPY_AND_ASSIGN(WatcherBase);
};

namespace {

class GsmWatcher : public SMSWatcher::WatcherBase {
 public:
  GsmWatcher(const std::string& device_path,
             MonitorSMSCallback callback,
             const std::string& dbus_connection,
             const dbus::ObjectPath& object_path)
      : WatcherBase(device_path, callback, dbus_connection, object_path),
        weak_ptr_factory_(this) {
    DBusThreadManager::Get()->GetGsmSMSClient()->SetSmsReceivedHandler(
        dbus_connection_,
        object_path_,
        base::Bind(&GsmWatcher::OnSmsReceived, weak_ptr_factory_.GetWeakPtr()));

    DBusThreadManager::Get()->GetGsmSMSClient()->List(
        dbus_connection_, object_path_,
        base::Bind(&GsmWatcher::ListSMSCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~GsmWatcher() {
    DBusThreadManager::Get()->GetGsmSMSClient()->ResetSmsReceivedHandler(
        dbus_connection_, object_path_);
  }

 private:
  // Callback for SmsReceived signal from ModemManager.Modem.Gsm.SMS
  void OnSmsReceived(uint32 index, bool complete) {
    // Only handle complete messages.
    if (!complete)
      return;
    DBusThreadManager::Get()->GetGsmSMSClient()->Get(
        dbus_connection_, object_path_, index,
        base::Bind(&GsmWatcher::GetSMSCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   index));
  }

  // Runs |callback_| with a SMS.
  void RunCallbackWithSMS(const base::DictionaryValue& sms_dictionary) {
    SMS sms;

    if (!sms_dictionary.GetStringWithoutPathExpansion(SMSWatcher::kNumberKey,
                                                      &sms.number))
      LOG(WARNING) << "SMS did not contain a number";

    if (!sms_dictionary.GetStringWithoutPathExpansion(SMSWatcher::kTextKey,
                                                      &sms.text))
      LOG(WARNING) << "SMS did not contain message text";

    std::string sms_timestamp;
    if (sms_dictionary.GetStringWithoutPathExpansion(SMSWatcher::kTimestampKey,
                                                     &sms_timestamp)) {
      decode_timestamp(sms_timestamp, &sms);
    } else {
      LOG(WARNING) << "SMS did not contain a timestamp";
      sms.timestamp = base::Time();
    }

    sms_dictionary.GetStringWithoutPathExpansion(SMSWatcher::kSmscKey,
                                                 &sms.smsc);

    double validity = 0;
    if (!sms_dictionary.GetDoubleWithoutPathExpansion(SMSWatcher::kValidityKey,
                                                      &validity)) {
      validity = -1;
    }
    sms.validity = validity;

    double msgclass = 0;
    if (!sms_dictionary.GetDoubleWithoutPathExpansion(SMSWatcher::kClassKey,
                                                      &msgclass)) {
      msgclass = -1;
    }
    sms.msgclass = msgclass;

    callback_.Run(device_path_, sms);
  }

  // Callback for Get() method from ModemManager.Modem.Gsm.SMS
  void GetSMSCallback(uint32 index,
                      const base::DictionaryValue& sms_dictionary) {
    RunCallbackWithSMS(sms_dictionary);

    DBusThreadManager::Get()->GetGsmSMSClient()->Delete(
        dbus_connection_, object_path_, index, base::Bind(&DeleteSMSCallback));
  }

  // Callback for List() method.
  void ListSMSCallback(const base::ListValue& result) {
    // List() is called only once; no one touches delete_queue_ before List().
    CHECK(delete_queue_.empty());
    for (size_t i = 0; i != result.GetSize(); ++i) {
      const base::DictionaryValue* sms_dictionary = NULL;
      if (!result.GetDictionary(i, &sms_dictionary)) {
        LOG(ERROR) << "result[" << i << "] is not a dictionary.";
        continue;
      }
      RunCallbackWithSMS(*sms_dictionary);
      double index = 0;
      if (sms_dictionary->GetDoubleWithoutPathExpansion(SMSWatcher::kIndexKey,
                                                        &index)) {
        delete_queue_.push_back(index);
      }
    }
    DeleteSMSInChain();
  }

  // Deletes SMSs in the queue.
  void DeleteSMSInChain() {
    if (!delete_queue_.empty()) {
      DBusThreadManager::Get()->GetGsmSMSClient()->Delete(
          dbus_connection_, object_path_, delete_queue_.back(),
          base::Bind(&GsmWatcher::DeleteSMSInChain,
                     weak_ptr_factory_.GetWeakPtr()));
      delete_queue_.pop_back();
    }
  }

  base::WeakPtrFactory<GsmWatcher> weak_ptr_factory_;
  std::vector<uint32> delete_queue_;

  DISALLOW_COPY_AND_ASSIGN(GsmWatcher);
};

class ModemManager1Watcher : public SMSWatcher::WatcherBase {
 public:
  ModemManager1Watcher(const std::string& device_path,
                       MonitorSMSCallback callback,
                       const std::string& dbus_connection,
                       const dbus::ObjectPath& object_path)
      : WatcherBase(device_path, callback, dbus_connection, object_path),
        weak_ptr_factory_(this),
        deleting_messages_(false),
        retrieving_messages_(false) {
    DBusThreadManager::Get()->GetModemMessagingClient()->SetSmsReceivedHandler(
        dbus_connection_,
        object_path_,
        base::Bind(&ModemManager1Watcher::OnSmsReceived,
                   weak_ptr_factory_.GetWeakPtr()));

    DBusThreadManager::Get()->GetModemMessagingClient()->List(
        dbus_connection_, object_path_,
        base::Bind(&ModemManager1Watcher::ListSMSCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~ModemManager1Watcher() {
    DBusThreadManager::Get()->GetModemMessagingClient()
        ->ResetSmsReceivedHandler(dbus_connection_, object_path_);
  }

 private:
  void ListSMSCallback(
      const std::vector<dbus::ObjectPath>& paths) {
    // This receives all messages, so clear any pending gets and deletes.
    retrieval_queue_.clear();
    delete_queue_.clear();

    retrieval_queue_.resize(paths.size());
    std::copy(paths.begin(), paths.end(), retrieval_queue_.begin());
    if (!retrieving_messages_)
      GetMessages();
  }

  // Messages must be deleted one at a time, since we can not
  // guarantee the order the deletion will be executed in. Delete
  // messages from the back of the list so that the indices are
  // valid.
  void DeleteMessages() {
    if (delete_queue_.empty()) {
      deleting_messages_ = false;
      return;
    }
    deleting_messages_ = true;
    dbus::ObjectPath sms_path = delete_queue_.back();
    delete_queue_.pop_back();
    DBusThreadManager::Get()->GetModemMessagingClient()->Delete(
        dbus_connection_, object_path_, sms_path,
        base::Bind(&ModemManager1Watcher::DeleteMessages,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Messages must be fetched one at a time, so that we do not queue too
  // many requests to a single threaded server.
  void GetMessages() {
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
        dbus_connection_, sms_path,
        base::Bind(&ModemManager1Watcher::GetCallback,
                   weak_ptr_factory_.GetWeakPtr()));
    delete_queue_.push_back(sms_path);
  }

  // Handles arrival of a new SMS message.
  void OnSmsReceived(const dbus::ObjectPath& sms_path, bool complete) {
    // Only handle complete messages.
    if (!complete)
      return;
    retrieval_queue_.push_back(sms_path);
    if (!retrieving_messages_)
      GetMessages();
  }

  // Runs |callback_| with a SMS.
  void RunCallbackWithSMS(const base::DictionaryValue& sms_dictionary) {
    SMS sms;

    if (!sms_dictionary.GetStringWithoutPathExpansion(
            SMSWatcher::kModemManager1NumberKey, &sms.number))
      LOG(WARNING) << "SMS did not contain a number";

    if (!sms_dictionary.GetStringWithoutPathExpansion(
            SMSWatcher::kModemManager1TextKey, &sms.text))
      LOG(WARNING) << "SMS did not contain message text";

    std::string sms_timestamp;
    if (sms_dictionary.GetStringWithoutPathExpansion(
            SMSWatcher::kModemManager1TimestampKey, &sms_timestamp)) {
      decode_timestamp(sms_timestamp, &sms);
    } else {
      LOG(WARNING) << "SMS did not contain a timestamp";
      sms.timestamp = base::Time();
    }

    sms_dictionary.GetStringWithoutPathExpansion(
        SMSWatcher::kModemManager1SmscKey, &sms.smsc);

    double validity = 0;
    if (!sms_dictionary.GetDoubleWithoutPathExpansion(
            SMSWatcher::kModemManager1ValidityKey, &validity)) {
      validity = -1;
    }
    sms.validity = validity;

    double msgclass = 0;
    if (!sms_dictionary.GetDoubleWithoutPathExpansion(
            SMSWatcher::kModemManager1ClassKey, &msgclass)) {
      msgclass = -1;
    }
    sms.msgclass = msgclass;

    callback_.Run(device_path_, sms);
  }

  void GetCallback(const base::DictionaryValue& dictionary) {
    RunCallbackWithSMS(dictionary);
    GetMessages();
  }

  base::WeakPtrFactory<ModemManager1Watcher> weak_ptr_factory_;
  bool deleting_messages_;
  bool retrieving_messages_;
  std::vector<dbus::ObjectPath> delete_queue_;
  std::deque<dbus::ObjectPath> retrieval_queue_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager1Watcher);
};

}  // namespace

SMSWatcher::SMSWatcher(const std::string& modem_device_path,
                       MonitorSMSCallback callback)
    : weak_ptr_factory_(this),
      device_path_(modem_device_path),
      callback_(callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
      dbus::ObjectPath(modem_device_path),
      base::Bind(&SMSWatcher::DevicePropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

SMSWatcher::~SMSWatcher() {
}

void SMSWatcher::DevicePropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS)
    return;

  std::string dbus_connection;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kDBusConnectionProperty, &dbus_connection)) {
    LOG(WARNING) << "Modem device properties do not include DBus connection.";
    return;
  }

  std::string object_path_string;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kDBusObjectProperty, &object_path_string)) {
    LOG(WARNING) << "Modem device properties do not include DBus object.";
    return;
  }

  if (object_path_string.compare(
          0, sizeof(modemmanager::kModemManager1ServicePath) - 1,
          modemmanager::kModemManager1ServicePath) == 0) {
    watcher_.reset(
        new ModemManager1Watcher(device_path_, callback_, dbus_connection,
                                 dbus::ObjectPath(object_path_string)));
  } else {
    watcher_.reset(
        new GsmWatcher(device_path_, callback_, dbus_connection,
                       dbus::ObjectPath(object_path_string)));
  }
}

}  // namespace chromeos
