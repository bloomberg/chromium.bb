// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/sms_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// SMSClient is used to communicate with the
// org.freedesktop.ModemManager1.SMS service.  All methods should be
// called from the origin thread (UI thread) which initializes the
// DBusThreadManager instance.
class SMSClientImpl : public SMSClient {
 public:
  SMSClientImpl() : bus_(NULL), weak_ptr_factory_(this) {}

  virtual ~SMSClientImpl() {}

  // Calls GetAll method.  |callback| is called after the method call succeeds.
  virtual void GetAll(const std::string& service_name,
                      const dbus::ObjectPath& object_path,
                      const GetAllCallback& callback) OVERRIDE {
    dbus::ObjectProxy *proxy = bus_->GetObjectProxy(service_name, object_path);
    dbus::MethodCall method_call(dbus::kDBusPropertiesInterface,
                                 dbus::kDBusPropertiesGetAll);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(modemmanager::kModemManager1SmsInterface);
    proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&SMSClientImpl::OnGetAll,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    bus_ = bus;
  }

 private:
  // Handles responses of GetAll method calls.
  void OnGetAll(const GetAllCallback& callback, dbus::Response* response) {
    if (!response) {
      // Must invoke the callback, even if there is no message.
      base::DictionaryValue empty_dictionary;
      callback.Run(empty_dictionary);
      return;
    }
    dbus::MessageReader reader(response);
    scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
    base::DictionaryValue* dictionary_value = NULL;
    if (!value.get() || !value->GetAsDictionary(&dictionary_value)) {
      LOG(WARNING) << "Invalid response: " << response->ToString();
      base::DictionaryValue empty_dictionary;
      callback.Run(empty_dictionary);
      return;
    }
    callback.Run(*dictionary_value);
  }

  dbus::Bus* bus_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SMSClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SMSClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SMSClient

SMSClient::SMSClient() {}

SMSClient::~SMSClient() {}


// static
SMSClient* SMSClient::Create() {
  return new SMSClientImpl();
}

}  // namespace chromeos
