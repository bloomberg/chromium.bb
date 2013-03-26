// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/experimental_bluetooth_profile_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char ExperimentalBluetoothProfileManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";


ExperimentalBluetoothProfileManagerClient::Options::Options()
    : role(SYMMETRIC),
      require_authentication(false),
      require_authorization(true),
      auto_connect(true) {
}

ExperimentalBluetoothProfileManagerClient::Options::~Options() {
}

// The ExperimentalBluetoothProfileManagerClient implementation used in
// production.
class ExperimentalBluetoothProfileManagerClientImpl
    : public ExperimentalBluetoothProfileManagerClient {
 public:
  explicit ExperimentalBluetoothProfileManagerClientImpl(dbus::Bus* bus)
      : bus_(bus),
        weak_ptr_factory_(this) {
    DCHECK(bus_);
    object_proxy_ = bus_->GetObjectProxy(
        bluetooth_profile_manager::kBluetoothProfileManagerServiceName,
        dbus::ObjectPath(
     bluetooth_profile_manager::kExperimentalBluetoothProfileManagerInterface));
  }

  virtual ~ExperimentalBluetoothProfileManagerClientImpl() {
  }

  // ExperimentalBluetoothProfileManagerClient override.
  virtual void RegisterProfile(const dbus::ObjectPath& profile_path,
                               const std::string& uuid,
                               const Options& options,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
    bluetooth_profile_manager::kExperimentalBluetoothProfileManagerInterface,
    bluetooth_profile_manager::kRegisterProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);
    writer.AppendString(uuid);

    dbus::MessageWriter array_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);

    dbus::MessageWriter dict_writer(NULL);

    // Always send Name, even if empty string.
    array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(bluetooth_profile_manager::kNameOption);
    dict_writer.AppendVariantOfString(options.name);
    array_writer.CloseContainer(&dict_writer);

    // Don't send Service if not provided.
    if (options.service.length()) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kServiceOption);
      dict_writer.AppendVariantOfString(options.service);
      array_writer.CloseContainer(&dict_writer);
    }

    // Don't send the default Role since there's no value for it.
    if (options.role != SYMMETRIC) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kRoleOption);
      if (options.role == CLIENT)
        dict_writer.AppendVariantOfString(
            bluetooth_profile_manager::kClientRoleOption);
      else if (options.role == SERVER)
        dict_writer.AppendVariantOfString(
            bluetooth_profile_manager::kServerRoleOption);
      else
        dict_writer.AppendVariantOfString("");
      array_writer.CloseContainer(&dict_writer);
    }

    // Don't send Channel unless given.
    if (options.channel) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kChannelOption);
      dict_writer.AppendVariantOfUint16(options.channel);
      array_writer.CloseContainer(&dict_writer);
    }

    // Don't send PSM unless given.
    if (options.psm) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kPSMOption);
      dict_writer.AppendVariantOfUint16(options.psm);
      array_writer.CloseContainer(&dict_writer);
    }

    // Always send RequireAuthentication, RequireAuthorization and AutoConnect.
    array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(
        bluetooth_profile_manager::kRequireAuthenticationOption);
    dict_writer.AppendVariantOfBool(options.require_authentication);
    array_writer.CloseContainer(&dict_writer);

    array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(
        bluetooth_profile_manager::kRequireAuthorizationOption);
    dict_writer.AppendVariantOfBool(options.require_authorization);
    array_writer.CloseContainer(&dict_writer);

    array_writer.OpenDictEntry(&dict_writer);
    dict_writer.AppendString(
        bluetooth_profile_manager::kAutoConnectOption);
    dict_writer.AppendVariantOfBool(options.auto_connect);
    array_writer.CloseContainer(&dict_writer);

    // Don't send ServiceRecord if not provided.
    if (options.service_record.length()) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kServiceRecordOption);
      dict_writer.AppendVariantOfString(options.service_record);
      array_writer.CloseContainer(&dict_writer);
    }

    // Don't send Version if not provided.
    if (options.version) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kVersionOption);
      dict_writer.AppendVariantOfUint16(options.version);
      array_writer.CloseContainer(&dict_writer);
    }

    // Don't send Features if not provided.
    if (options.features) {
      dbus::MessageWriter dict_writer(NULL);
      array_writer.OpenDictEntry(&dict_writer);
      dict_writer.AppendString(bluetooth_profile_manager::kFeaturesOption);
      dict_writer.AppendVariantOfUint16(options.features);
      array_writer.CloseContainer(&dict_writer);
    }

    writer.CloseContainer(&array_writer);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothProfileManagerClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothProfileManagerClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // ExperimentalBluetoothProfileManagerClient override.
  virtual void UnregisterProfile(const dbus::ObjectPath& profile_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
       bluetooth_profile_manager::kExperimentalBluetoothProfileManagerInterface,
        bluetooth_profile_manager::kUnregisterProfile);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);

    object_proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&ExperimentalBluetoothProfileManagerClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&ExperimentalBluetoothProfileManagerClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 private:
  // Called when a response for successful method call is received.
  void OnSuccess(const base::Closure& callback,
                 dbus::Response* response) {
    DCHECK(response);
    callback.Run();
  }

  // Called when a response for a failed method call is received.
  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
      error_message = "";
    }
    error_callback.Run(error_name, error_message);
  }

  dbus::Bus* bus_;
  dbus::ObjectProxy* object_proxy_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ExperimentalBluetoothProfileManagerClientImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothProfileManagerClientImpl);
};

// The ExperimentalBluetoothProfileManagerClient implementation used on Linux
// desktop, which does nothing.
class ExperimentalBluetoothProfileManagerClientStubImpl
    : public ExperimentalBluetoothProfileManagerClient {
 public:
  ExperimentalBluetoothProfileManagerClientStubImpl() {
  }

  // ExperimentalBluetoothProfileManagerClient override.
  virtual void RegisterProfile(const dbus::ObjectPath& profile_path,
                               const std::string& uuid,
                               const Options& options,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) OVERRIDE {
    VLOG(1) << "RegisterProfile: " << profile_path.value() << ": " << uuid;
    error_callback.Run(kNoResponseError, "");
  }

  // ExperimentalBluetoothProfileManagerClient override.
  virtual void UnregisterProfile(const dbus::ObjectPath& profile_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE {
    VLOG(1) << "UnregisterAgent: " << profile_path.value();
    error_callback.Run(kNoResponseError, "");
  }
};

ExperimentalBluetoothProfileManagerClient::
    ExperimentalBluetoothProfileManagerClient() {
}

ExperimentalBluetoothProfileManagerClient::
    ~ExperimentalBluetoothProfileManagerClient() {
}

ExperimentalBluetoothProfileManagerClient*
    ExperimentalBluetoothProfileManagerClient::Create(
        DBusClientImplementationType type,
        dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ExperimentalBluetoothProfileManagerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ExperimentalBluetoothProfileManagerClientStubImpl();
}

}  // namespace chromeos
