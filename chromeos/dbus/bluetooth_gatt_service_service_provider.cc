// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_gatt_service_service_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_service_provider.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {
const char kErrorInvalidArgs[] =
    "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorPropertyReadOnly[] =
    "org.freedesktop.DBus.Error.PropertyReadOnly";
}  // namespace

// The BluetoothGattServiceServiceProvider implementation used in production.
class BluetoothGattServiceServiceProviderImpl
    : public BluetoothGattServiceServiceProvider {
 public:
  BluetoothGattServiceServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      const std::string& uuid,
      const std::vector<dbus::ObjectPath>& includes)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        uuid_(uuid),
        includes_(includes),
        bus_(bus),
        object_path_(object_path),
        weak_ptr_factory_(this) {
    VLOG(1) << "Creating Bluetooth GATT service: " << object_path_.value()
            << " UUID: " << uuid;
    DCHECK(!uuid_.empty());
    DCHECK(object_path_.IsValid());
    DCHECK(bus_);

    exported_object_ = bus_->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesGet,
        base::Bind(&BluetoothGattServiceServiceProviderImpl::Get,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothGattServiceServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesSet,
        base::Bind(&BluetoothGattServiceServiceProviderImpl::Set,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothGattServiceServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesGetAll,
        base::Bind(&BluetoothGattServiceServiceProviderImpl::GetAll,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothGattServiceServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~BluetoothGattServiceServiceProviderImpl() {
    VLOG(1) << "Cleaning up Bluetooth GATT service: " << object_path_.value();
    bus_->UnregisterExportedObject(object_path_);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when the Bluetooth daemon fetches a single property of
  // the service.
  void Get(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothGattServiceServiceProvider::Get: "
            << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    std::string property_name;
    if (!reader.PopString(&interface_name) ||
        !reader.PopString(&property_name) ||
        reader.HasMoreData()) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs, "Expected 'ss'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Only the GATT service interface is allowed.
    if (interface_name !=
        bluetooth_gatt_service::kBluetoothGattServiceInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Return error if |property_name| is unknown.
    if (property_name != bluetooth_gatt_service::kUUIDProperty &&
        property_name != bluetooth_gatt_service::kIncludesProperty) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such property: '" + property_name + "'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    if (property_name == bluetooth_gatt_service::kUUIDProperty) {
      writer.OpenVariant("s", &variant_writer);
      variant_writer.AppendString(uuid_);
      writer.CloseContainer(&variant_writer);
    } else {
      writer.OpenVariant("ao", &variant_writer);
      variant_writer.AppendArrayOfObjectPaths(includes_);
      writer.CloseContainer(&variant_writer);
    }

    response_sender.Run(response.Pass());
  }

  // Called by dbus:: when the Bluetooth daemon sets a single property of the
  // service.
  void Set(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothGattServiceServiceProvider::Set: "
            << object_path_.value();
    DCHECK(OnOriginThread());

    // All of the properties on this interface are read-only, so just return
    // error.
    scoped_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorPropertyReadOnly,
            "All properties are read-only.");
    response_sender.Run(error_response.PassAs<dbus::Response>());
  }

  // Called by dbus:: when the Bluetooth daemon fetches all properties of the
  // service.
  void GetAll(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothGattServiceServiceProvider::GetAll: "
            << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    if (!reader.PopString(&interface_name) || reader.HasMoreData()) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs, "Expected 's'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Only the GATT service interface is allowed.
    if (interface_name !=
        bluetooth_gatt_service::kBluetoothGattServiceInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(NULL);
    dbus::MessageWriter dict_entry_writer(NULL);
    dbus::MessageWriter variant_writer(NULL);

    writer.OpenArray("{sv}", &array_writer);

    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(bluetooth_gatt_service::kUUIDProperty);
    dict_entry_writer.AppendVariantOfString(uuid_);
    array_writer.CloseContainer(&dict_entry_writer);

    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(bluetooth_gatt_service::kIncludesProperty);
    dict_entry_writer.OpenVariant("ao", &variant_writer);
    variant_writer.AppendArrayOfObjectPaths(includes_);
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer.CloseContainer(&dict_entry_writer);

    writer.CloseContainer(&array_writer);

    response_sender.Run(response.Pass());
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // 128-bit service UUID of this object.
  std::string uuid_;

  // List of object paths that represent other exported GATT services that are
  // included from this service.
  std::vector<dbus::ObjectPath> includes_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  dbus::Bus* bus_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattServiceServiceProviderImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattServiceServiceProviderImpl);
};

BluetoothGattServiceServiceProvider::BluetoothGattServiceServiceProvider() {
}

BluetoothGattServiceServiceProvider::~BluetoothGattServiceServiceProvider() {
}

// static
BluetoothGattServiceServiceProvider*
BluetoothGattServiceServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    const std::vector<dbus::ObjectPath>& includes) {
  if (!DBusThreadManager::Get()->IsUsingStub(DBusClientBundle::BLUETOOTH)) {
    return new BluetoothGattServiceServiceProviderImpl(
        bus, object_path, uuid, includes);
  }
  return new FakeBluetoothGattServiceServiceProvider(
      object_path, uuid, includes);
}

}  // namespace chromeos
