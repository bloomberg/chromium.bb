// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_profile_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The FlimflamProfileClient implementation.
class FlimflamProfileClientImpl : public FlimflamProfileClient {
 public:
  explicit FlimflamProfileClientImpl(dbus::Bus* bus);

  // FlimflamProfileClient overrides:
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE;
  virtual void ResetPropertyChangedHandler() OVERRIDE;
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE;
  virtual void GetEntry(const dbus::ObjectPath& path,
                        const DictionaryValueCallback& callback) OVERRIDE;
  virtual void DeleteEntry(const dbus::ObjectPath& path,
                           const VoidCallback& callback) OVERRIDE;

 private:
  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success);
  // Handles PropertyChanged signal.
  void OnPropertyChanged(dbus::Signal* signal);
  // Handles responses for methods without results.
  void OnVoidMethod(const VoidCallback& callback, dbus::Response* response);
  // Handles responses for methods with DictionaryValue results.
  void OnDictionaryValueMethod(const DictionaryValueCallback& callback,
                               dbus::Response* response);

  dbus::ObjectProxy* proxy_;
  FlimflamClientHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamProfileClientImpl);
};

FlimflamProfileClientImpl::FlimflamProfileClientImpl(dbus::Bus* bus)
    : proxy_(bus->GetObjectProxy(
        flimflam::kFlimflamServiceName,
        dbus::ObjectPath(flimflam::kFlimflamServicePath))),
      helper_(proxy_) {
  helper_.MonitorPropertyChanged(flimflam::kFlimflamProfileInterface);
}

void FlimflamProfileClientImpl::SetPropertyChangedHandler(
    const PropertyChangedHandler& handler) {
  helper_.SetPropertyChangedHandler(handler);
}

void FlimflamProfileClientImpl::ResetPropertyChangedHandler() {
  helper_.ResetPropertyChangedHandler();
}

void FlimflamProfileClientImpl::GetProperties(
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamProfileInterface,
                               flimflam::kGetPropertiesFunction);
  helper_.CallDictionaryValueMethod(&method_call, callback);
}

void FlimflamProfileClientImpl::GetEntry(
    const dbus::ObjectPath& path,
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamProfileInterface,
                               flimflam::kGetEntryFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendObjectPath(path);
  helper_.CallDictionaryValueMethod(&method_call, callback);
}

void FlimflamProfileClientImpl::DeleteEntry(const dbus::ObjectPath& path,
                                            const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamProfileInterface,
                               flimflam::kDeleteEntryFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendObjectPath(path);
  helper_.CallVoidMethod(&method_call, callback);
}

// A stub implementation of FlimflamProfileClient.
class FlimflamProfileClientStubImpl : public FlimflamProfileClient {
 public:
  FlimflamProfileClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~FlimflamProfileClientStubImpl() {}

  // FlimflamProfileClient override.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamProfileClient override.
  virtual void ResetPropertyChangedHandler() OVERRIDE {}

  // FlimflamProfileClient override.
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FlimflamProfileClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // FlimflamProfileClient override.
  virtual void GetEntry(const dbus::ObjectPath& path,
                        const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FlimflamProfileClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // FlimflamProfileClient override.
  virtual void DeleteEntry(const dbus::ObjectPath& path,
                           const VoidCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback,
                                                DBUS_METHOD_CALL_SUCCESS));
  }

 private:
  void PassEmptyDictionaryValue(const DictionaryValueCallback& callback) const {
    base::DictionaryValue dictionary;
    callback.Run(DBUS_METHOD_CALL_SUCCESS, dictionary);
  }

  base::WeakPtrFactory<FlimflamProfileClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamProfileClientStubImpl);
};

}  // namespace

FlimflamProfileClient::FlimflamProfileClient() {}

FlimflamProfileClient::~FlimflamProfileClient() {}

// static
FlimflamProfileClient* FlimflamProfileClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new FlimflamProfileClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FlimflamProfileClientStubImpl();
}

}  // namespace chromeos
