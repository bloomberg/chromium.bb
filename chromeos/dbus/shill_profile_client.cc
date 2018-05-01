// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_profile_client.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kSharedProfilePath[] = "/profile/default";

class ShillProfileClientImpl : public ShillProfileClient {
 public:
  ShillProfileClientImpl();

  void AddPropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(profile_path)->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(profile_path)->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(const dbus::ObjectPath& profile_path,
                     const DictionaryValueCallbackWithoutStatus& callback,
                     const ErrorCallback& error_callback) override;
  void GetEntry(const dbus::ObjectPath& profile_path,
                const std::string& entry_path,
                const DictionaryValueCallbackWithoutStatus& callback,
                const ErrorCallback& error_callback) override;
  void DeleteEntry(const dbus::ObjectPath& profile_path,
                   const std::string& entry_path,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;

  TestInterface* GetTestInterface() override { return NULL; }

 protected:
  void Init(dbus::Bus* bus) override { bus_ = bus; }

 private:
  using HelperMap = std::map<std::string, std::unique_ptr<ShillClientHelper>>;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& profile_path);

  dbus::Bus* bus_;
  HelperMap helpers_;

  DISALLOW_COPY_AND_ASSIGN(ShillProfileClientImpl);
};

ShillProfileClientImpl::ShillProfileClientImpl() : bus_(NULL) {
}

ShillClientHelper* ShillProfileClientImpl::GetHelper(
    const dbus::ObjectPath& profile_path) {
  HelperMap::const_iterator it = helpers_.find(profile_path.value());
  if (it != helpers_.end())
    return it->second.get();

  // There is no helper for the profile, create it.
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(shill::kFlimflamServiceName, profile_path);
  std::unique_ptr<ShillClientHelper> helper(
      new ShillClientHelper(object_proxy));
  helper->MonitorPropertyChanged(shill::kFlimflamProfileInterface);
  ShillClientHelper* helper_ptr = helper.get();
  helpers_[profile_path.value()] = std::move(helper);
  return helper_ptr;
}

void ShillProfileClientImpl::GetProperties(
    const dbus::ObjectPath& profile_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  dbus::MethodCall method_call(shill::kFlimflamProfileInterface,
                               shill::kGetPropertiesFunction);
  GetHelper(profile_path)->CallDictionaryValueMethodWithErrorCallback(
      &method_call, callback, error_callback);
}

void ShillProfileClientImpl::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  dbus::MethodCall method_call(shill::kFlimflamProfileInterface,
                               shill::kGetEntryFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(entry_path);
  GetHelper(profile_path)->CallDictionaryValueMethodWithErrorCallback(
      &method_call, callback, error_callback);
}

void ShillProfileClientImpl::DeleteEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  dbus::MethodCall method_call(shill::kFlimflamProfileInterface,
                               shill::kDeleteEntryFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(entry_path);
  GetHelper(profile_path)->CallVoidMethodWithErrorCallback(
      &method_call, callback, error_callback);
}

}  // namespace

ShillProfileClient::ShillProfileClient() = default;

ShillProfileClient::~ShillProfileClient() = default;

// static
ShillProfileClient* ShillProfileClient::Create() {
  return new ShillProfileClientImpl();
}

// static
std::string ShillProfileClient::GetSharedProfilePath() {
  return std::string(kSharedProfilePath);
}

}  // namespace chromeos
