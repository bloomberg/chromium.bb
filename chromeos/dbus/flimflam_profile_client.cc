// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_profile_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
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
      const dbus::ObjectPath& profile_path,
      const PropertyChangedHandler& handler) OVERRIDE;
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& profile_path) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& profile_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual void GetEntry(const dbus::ObjectPath& profile_path,
                        const std::string& entry_path,
                        const DictionaryValueCallback& callback) OVERRIDE;
  virtual void DeleteEntry(const dbus::ObjectPath& profile_path,
                           const std::string& entry_path,
                           const VoidCallback& callback) OVERRIDE;

 private:
  typedef std::map<std::string, FlimflamClientHelper*> HelperMap;

  // Returns the corresponding FlimflamClientHelper for the profile.
  FlimflamClientHelper* GetHelper(const dbus::ObjectPath& profile_path);

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamProfileClientImpl);
};

FlimflamProfileClientImpl::FlimflamProfileClientImpl(dbus::Bus* bus)
    : bus_(bus),
      helpers_deleter_(&helpers_) {
}

FlimflamClientHelper* FlimflamProfileClientImpl::GetHelper(
    const dbus::ObjectPath& profile_path) {
  HelperMap::iterator it = helpers_.find(profile_path.value());
  if (it != helpers_.end())
    return it->second;

  // There is no helper for the profile, create it.
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(flimflam::kFlimflamServiceName, profile_path);
  FlimflamClientHelper* helper = new FlimflamClientHelper(bus_, object_proxy);
  helper->MonitorPropertyChanged(flimflam::kFlimflamProfileInterface);
  helpers_.insert(HelperMap::value_type(profile_path.value(), helper));
  return helper;
}

void FlimflamProfileClientImpl::SetPropertyChangedHandler(
    const dbus::ObjectPath& profile_path,
    const PropertyChangedHandler& handler) {
  GetHelper(profile_path)->SetPropertyChangedHandler(handler);
}

void FlimflamProfileClientImpl::ResetPropertyChangedHandler(
    const dbus::ObjectPath& profile_path) {
  GetHelper(profile_path)->ResetPropertyChangedHandler();
}

void FlimflamProfileClientImpl::GetProperties(
    const dbus::ObjectPath& profile_path,
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamProfileInterface,
                               flimflam::kGetPropertiesFunction);
  GetHelper(profile_path)->CallDictionaryValueMethod(&method_call, callback);
}

void FlimflamProfileClientImpl::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const DictionaryValueCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamProfileInterface,
                               flimflam::kGetEntryFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(entry_path);
  GetHelper(profile_path)->CallDictionaryValueMethod(&method_call, callback);
}

void FlimflamProfileClientImpl::DeleteEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const VoidCallback& callback) {
  dbus::MethodCall method_call(flimflam::kFlimflamProfileInterface,
                               flimflam::kDeleteEntryFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(entry_path);
  GetHelper(profile_path)->CallVoidMethod(&method_call, callback);
}

// A stub implementation of FlimflamProfileClient.
class FlimflamProfileClientStubImpl : public FlimflamProfileClient {
 public:
  FlimflamProfileClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~FlimflamProfileClientStubImpl() {}

  // FlimflamProfileClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& profile_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamProfileClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& profile_path) OVERRIDE {}

  // FlimflamProfileClient override.
  virtual void GetProperties(const dbus::ObjectPath& profile_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FlimflamProfileClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // FlimflamProfileClient override.
  virtual void GetEntry(const dbus::ObjectPath& profile_path,
                        const std::string& entry_path,
                        const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FlimflamProfileClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // FlimflamProfileClient override.
  virtual void DeleteEntry(const dbus::ObjectPath& profile_path,
                           const std::string& entry_path,
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
