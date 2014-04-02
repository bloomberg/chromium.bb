// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_gatt_characteristic_service_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "chromeos/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {
const char kErrorInvalidArgs[] =
    "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorPropertyReadOnly[] =
    "org.freedesktop.DBus.Error.PropertyReadOnly";
const char kErrorFailed[] =
    "org.freedesktop.DBus.Error.Failed";
}  // namespace

// The BluetoothGattCharacteristicServiceProvider implementation used in
// production.
class BluetoothGattCharacteristicServiceProviderImpl
    : public BluetoothGattCharacteristicServiceProvider {
 public:
  BluetoothGattCharacteristicServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const std::vector<std::string>& permissions,
      const dbus::ObjectPath& service_path)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        uuid_(uuid),
        bus_(bus),
        delegate_(delegate),
        object_path_(object_path),
        service_path_(service_path),
        weak_ptr_factory_(this) {
    VLOG(1) << "Created Bluetooth GATT characteristic: " << object_path.value()
            << " UUID: " << uuid;
    DCHECK(bus_);
    DCHECK(delegate_);
    DCHECK(!uuid_.empty());
    DCHECK(object_path_.IsValid());
    DCHECK(service_path_.IsValid());
    DCHECK(StartsWithASCII(
        object_path_.value(), service_path_.value() + "/", true));

    exported_object_ = bus_->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesGet,
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::Get,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesSet,
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::Set,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesGetAll,
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::GetAll,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~BluetoothGattCharacteristicServiceProviderImpl() {
    VLOG(1) << "Cleaning up Bluetooth GATT characteristic: "
            << object_path_.value();
    bus_->UnregisterExportedObject(object_path_);
  }

  // BluetoothGattCharacteristicServiceProvider override.
  virtual void SendValueChanged(const std::vector<uint8>& value) OVERRIDE {
    VLOG(2) << "Emitting a PropertiesChanged signal for characteristic value.";
    dbus::Signal signal(
        dbus::kDBusPropertiesInterface,
        dbus::kDBusPropertiesChangedSignal);
    dbus::MessageWriter writer(&signal);
    dbus::MessageWriter array_writer(NULL);
    dbus::MessageWriter dict_entry_writer(NULL);
    dbus::MessageWriter variant_writer(NULL);

    // interface_name
    writer.AppendString(
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);

    // changed_properties
    writer.OpenArray("{sv}", &array_writer);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_gatt_characteristic::kValueProperty);
    dict_entry_writer.OpenVariant("ay", &variant_writer);
    variant_writer.AppendArrayOfBytes(value.data(), value.size());
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer.CloseContainer(&dict_entry_writer);
    writer.CloseContainer(&array_writer);

    // invalidated_properties.
    writer.OpenArray("s", &array_writer);
    writer.CloseContainer(&array_writer);

    exported_object_->SendSignal(&signal);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when the Bluetooth daemon fetches a single property of
  // the characteristic.
  void Get(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothGattCharacteristicServiceProvider::Get: "
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

    // Only the GATT characteristic interface is supported.
    if (interface_name !=
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // If getting the "Value" property, obtain the value from the delegate.
    if (property_name == bluetooth_gatt_characteristic::kValueProperty) {
      DCHECK(delegate_);
      delegate_->GetCharacteristicValue(
          base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnGet,
                     weak_ptr_factory_.GetWeakPtr(),
                     method_call, response_sender),
          base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     method_call, response_sender));
      return;
    }

    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    // TODO(armansito): Process the "Flags" and "Permissions" properties below.
    if (property_name == bluetooth_gatt_characteristic::kUUIDProperty) {
      writer.OpenVariant("s", &variant_writer);
      variant_writer.AppendString(uuid_);
      writer.CloseContainer(&variant_writer);
    } else if (property_name ==
               bluetooth_gatt_characteristic::kServiceProperty) {
      writer.OpenVariant("o", &variant_writer);
      variant_writer.AppendObjectPath(service_path_);
      writer.CloseContainer(&variant_writer);
    } else {
      response = dbus::ErrorResponse::FromMethodCall(
          method_call, kErrorInvalidArgs,
          "No such property: '" + property_name + "'.")
          .PassAs<dbus::Response>();
    }

    response_sender.Run(response.Pass());
  }

  // Called by dbus:: when the Bluetooth daemon sets a single property of the
  // characteristic.
  void Set(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothGattCharacteristicServiceProvider::Set: "
            << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    std::string property_name;
    dbus::MessageReader variant_reader(NULL);
    if (!reader.PopString(&interface_name) ||
        !reader.PopString(&property_name) ||
        !reader.PopVariant(&variant_reader) ||
        reader.HasMoreData()) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs, "Expected 'ssv'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Only the GATT characteristic interface is allowed.
    if (interface_name !=
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Only the "Value" property is writeable.
    if (property_name != bluetooth_gatt_characteristic::kValueProperty) {
      std::string error_name;
      std::string error_message;
      if (property_name == bluetooth_gatt_characteristic::kUUIDProperty ||
          property_name == bluetooth_gatt_characteristic::kServiceProperty) {
        error_name = kErrorPropertyReadOnly;
        error_message = "Read-only property: '" + property_name + "'.";
      } else {
        error_name = kErrorInvalidArgs;
        error_message = "No such property: '" + property_name + "'.";
      }
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, error_name, error_message);
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Obtain the value.
    const uint8* bytes = NULL;
    size_t length = 0;
    if (!variant_reader.PopArrayOfBytes(&bytes, &length)) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "Property '" + property_name + "' has type 'ay'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Pass the set request onto the delegate.
    std::vector<uint8> value(bytes, bytes + length);
    DCHECK(delegate_);
    delegate_->SetCharacteristicValue(
        value,
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnSet,
                   weak_ptr_factory_.GetWeakPtr(),
                   method_call, response_sender),
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnFailure,
                   weak_ptr_factory_.GetWeakPtr(),
                   method_call, response_sender));
  }

  // Called by dbus:: when the Bluetooth daemon fetches all properties of the
  // characteristic.
  void GetAll(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothGattCharacteristicServiceProvider::GetAll: "
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

    // Only the GATT characteristic interface is supported.
    if (interface_name !=
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.PassAs<dbus::Response>());
      return;
    }

    // Try to obtain the value from the delegate. We will construct the
    // response in the success callback.
    DCHECK(delegate_);
    delegate_->GetCharacteristicValue(
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnGetAll,
                   weak_ptr_factory_.GetWeakPtr(),
                   method_call, response_sender),
        base::Bind(&BluetoothGattCharacteristicServiceProviderImpl::OnFailure,
                   weak_ptr_factory_.GetWeakPtr(),
                   method_call, response_sender));
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by the Delegate in response to a method to call to get all
  // properties, in which the delegate has successfully returned the
  // characteristic value.
  void OnGetAll(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender,
                const std::vector<uint8>& value) {
    VLOG(2) << "Characteristic value obtained from delegate. Responding to "
            << "GetAll.";

    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(NULL);
    dbus::MessageWriter dict_entry_writer(NULL);
    dbus::MessageWriter variant_writer(NULL);

    writer.OpenArray("{sv}", &array_writer);

    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_gatt_characteristic::kUUIDProperty);
    dict_entry_writer.AppendVariantOfString(uuid_);
    array_writer.CloseContainer(&dict_entry_writer);

    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_gatt_characteristic::kServiceProperty);
    dict_entry_writer.AppendVariantOfObjectPath(service_path_);
    array_writer.CloseContainer(&dict_entry_writer);

    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_gatt_characteristic::kValueProperty);
    dict_entry_writer.OpenVariant("ay", &variant_writer);
    variant_writer.AppendArrayOfBytes(value.data(), value.size());
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer.CloseContainer(&dict_entry_writer);

    // TODO(armansito): Process Flags & Permissions properties.

    writer.CloseContainer(&array_writer);

    response_sender.Run(response.Pass());
  }

  // Called by the Delegate in response to a successful method call to get the
  // characteristic value.
  void OnGet(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender,
             const std::vector<uint8>& value) {
    VLOG(2) << "Returning characteristic value obtained from delegate.";
    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    writer.OpenVariant("ay", &variant_writer);
    variant_writer.AppendArrayOfBytes(value.data(), value.size());
    writer.CloseContainer(&variant_writer);

    response_sender.Run(response.Pass());
  }

  // Called by the Delegate in response to a successful method call to set the
  // characteristic value.
  void OnSet(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "Successfully set characteristic value. Return success.";
    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by the Delegate in response to a failed method call to get or set
  // the characteristic value.
  void OnFailure(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "Failed to get/set characteristic value. Report error.";
    scoped_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorFailed,
            "Failed to get/set characteristic value.");
    response_sender.Run(error_response.PassAs<dbus::Response>());
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // 128-bit characteristic UUID of this object.
  std::string uuid_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  dbus::Bus* bus_;

  // Incoming methods to get and set the "Value" property are passed on to the
  // delegate and callbacks passed to generate a reply. |delegate_| is generally
  // the object that owns this one and must outlive it.
  Delegate* delegate_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

  // Object path of the GATT service that the exported characteristic belongs
  // to.
  dbus::ObjectPath service_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattCharacteristicServiceProviderImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattCharacteristicServiceProviderImpl);
};

BluetoothGattCharacteristicServiceProvider::
    BluetoothGattCharacteristicServiceProvider() {
}

BluetoothGattCharacteristicServiceProvider::
    ~BluetoothGattCharacteristicServiceProvider() {
}

// static
BluetoothGattCharacteristicServiceProvider*
BluetoothGattCharacteristicServiceProvider::Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const std::vector<std::string>& permissions,
      const dbus::ObjectPath& service_path) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    return new BluetoothGattCharacteristicServiceProviderImpl(
        bus, object_path, delegate, uuid, flags, permissions, service_path);
  }
  return new FakeBluetoothGattCharacteristicServiceProvider(
      object_path, delegate, uuid, flags, permissions, service_path);
}

}  // namespace chromeos
