// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_agent_service_provider.h"

#include <string>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Constants used by BlueZ for the ConfirmModeChange method.
const char kModeOff[] = "off";
const char kModeConnectable[] = "connectable";
const char kModeDiscoverable[] = "discoverable";

}  // namespace

namespace chromeos {

// The BluetoothAgentServiceProvider implementation used in production.
class BluetoothAgentServiceProviderImpl : public BluetoothAgentServiceProvider {
 public:
  BluetoothAgentServiceProviderImpl(dbus::Bus* bus,
                                    const dbus::ObjectPath& object_path,
                                    Delegate* delegate)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        delegate_(delegate),
        object_path_(object_path),
        weak_ptr_factory_(this) {
    DVLOG(1) << "Creating BluetoothAdapterClientImpl for "
             << object_path.value();

    exported_object_ = bus_->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRelease,
        base::Bind(&BluetoothAgentServiceProviderImpl::Release,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::ReleaseExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestPinCode,
        base::Bind(&BluetoothAgentServiceProviderImpl::RequestPinCode,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::RequestPinCodeExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestPasskey,
        base::Bind(&BluetoothAgentServiceProviderImpl::RequestPasskey,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::RequestPasskeyExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kDisplayPinCode,
        base::Bind(&BluetoothAgentServiceProviderImpl::DisplayPinCode,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::DisplayPinCodeExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kDisplayPasskey,
        base::Bind(&BluetoothAgentServiceProviderImpl::DisplayPasskey,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::DisplayPasskeyExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestConfirmation,
        base::Bind(&BluetoothAgentServiceProviderImpl::RequestConfirmation,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &BluetoothAgentServiceProviderImpl::RequestConfirmationExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kAuthorize,
        base::Bind(&BluetoothAgentServiceProviderImpl::Authorize,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::AuthorizeExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kConfirmModeChange,
        base::Bind(&BluetoothAgentServiceProviderImpl::ConfirmModeChange,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &BluetoothAgentServiceProviderImpl::ConfirmModeChangeExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kCancel,
        base::Bind(&BluetoothAgentServiceProviderImpl::Cancel,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAgentServiceProviderImpl::CancelExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~BluetoothAgentServiceProviderImpl() {
    // Unregister the object path so we can reuse with a new agent.
    bus_->UnregisterExportedObject(object_path_);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when the agent is unregistered from the Bluetooth
  // daemon, generally at the end of a pairing request.
  void Release(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Release();

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Called by dbus:: when the Release method is exported.
  void ReleaseExported(const std::string& interface_name,
                       const std::string& method_name,
                       bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires a PIN Code for
  // device authentication.
  void RequestPinCode(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestPinCode called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::PinCodeCallback callback = base::Bind(
        &BluetoothAgentServiceProviderImpl::OnPinCode,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestPinCode(device_path, callback);
  }

  // Called by dbus:: when the RequestPinCode method is exported.
  void RequestPinCodeExported(const std::string& interface_name,
                              const std::string& method_name,
                              bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires a Passkey for
  // device authentication.
  void RequestPasskey(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestPasskey called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::PasskeyCallback callback = base::Bind(
        &BluetoothAgentServiceProviderImpl::OnPasskey,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestPasskey(device_path, callback);
  }

  // Called by dbus:: when the RequestPasskey method is exported.
  void RequestPasskeyExported(const std::string& interface_name,
                              const std::string& method_name,
                              bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // enter a PIN Code into the remote device so that it may be
  // authenticated.
  void DisplayPinCode(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    std::string pincode;
    if (!reader.PopObjectPath(&device_path) ||
        !reader.PopString(&pincode)) {
      LOG(WARNING) << "DisplayPinCode called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    delegate_->DisplayPinCode(device_path, pincode);

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Called by dbus:: when the DisplayPinCode method is exported.
  void DisplayPinCodeExported(const std::string& interface_name,
                              const std::string& method_name,
                              bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // enter a Passkey into the remote device so that it may be
  // authenticated.
  void DisplayPasskey(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    uint32 passkey;
    if (!reader.PopObjectPath(&device_path) ||
        !reader.PopUint32(&passkey)) {
      LOG(WARNING) << "DisplayPasskey called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    delegate_->DisplayPasskey(device_path, passkey);

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Called by dbus:: when the DisplayPasskey method is exported.
  void DisplayPasskeyExported(const std::string& interface_name,
                              const std::string& method_name,
                              bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm that a Passkey is displayed on the screen of the remote
  // device so that it may be authenticated.
  void RequestConfirmation(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    uint32 passkey;
    if (!reader.PopObjectPath(&device_path) ||
        !reader.PopUint32(&passkey)) {
      LOG(WARNING) << "RequestConfirmation called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback = base::Bind(
        &BluetoothAgentServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestConfirmation(device_path, passkey, callback);
  }

  // Called by dbus:: when the RequestConfirmation method is exported.
  void RequestConfirmationExported(const std::string& interface_name,
                                   const std::string& method_name,
                                   bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm that that a remote device is authorized to connect to a service
  // UUID.
  void Authorize(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    std::string uuid;
    if (!reader.PopObjectPath(&device_path) ||
        !reader.PopString(&uuid)) {
      LOG(WARNING) << "Authorize called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback = base::Bind(
        &BluetoothAgentServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->Authorize(device_path, uuid, callback);
  }

  // Called by dbus:: when the Authorize method is exported.
  void AuthorizeExported(const std::string& interface_name,
                         const std::string& method_name,
                         bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm that the adapter may change mode.
  void ConfirmModeChange(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    std::string mode_str;
    if (!reader.PopString(&mode_str)) {
      LOG(WARNING) << "ConfirmModeChange called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::Mode mode;
    if (mode_str == kModeOff) {
      mode = Delegate::OFF;
    } else if (mode_str == kModeConnectable) {
      mode = Delegate::CONNECTABLE;
    } else if (mode_str == kModeDiscoverable) {
      mode = Delegate::DISCOVERABLE;
    } else {
      LOG(WARNING) << "ConfirmModeChange called with unknown mode: "
                   << mode_str;
      return;
    }

    Delegate::ConfirmationCallback callback = base::Bind(
        &BluetoothAgentServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->ConfirmModeChange(mode, callback);
  }

  // Called by dbus:: when the ConfirmModeChange method is exported.
  void ConfirmModeChangeExported(const std::string& interface_name,
                                 const std::string& method_name,
                                 bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by dbus:: when the request failed before a reply was returned
  // from the device.
  void Cancel(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Cancel();

    dbus::Response* response = dbus::Response::FromMethodCall(method_call);
    response_sender.Run(response);
  }

  // Called by dbus:: when the Cancel method is exported.
  void CancelExported(const std::string& interface_name,
                      const std::string& method_name,
                      bool success) {
    LOG_IF(WARNING, !success) << "Failed to export "
                              << interface_name << "." << method_name;
  }

  // Called by the Delegate to response to a method requesting a PIN code.
  void OnPinCode(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender,
                 Delegate::Status status,
                 const std::string& pincode) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        dbus::Response* response = dbus::Response::FromMethodCall(method_call);
        dbus::MessageWriter writer(response);
        writer.AppendString(pincode);
        response_sender.Run(response);
        break;
      }
      case Delegate::REJECTED: {
        dbus::ErrorResponse* response = dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_agent::kErrorRejected, "rejected");
        response_sender.Run(response);
        break;
      }
      case Delegate::CANCELLED: {
        dbus::ErrorResponse* response = dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_agent::kErrorCanceled, "canceled");
        response_sender.Run(response);
        break;
      }
      default:
        NOTREACHED() << "Unexpected status code from delegate: " << status;
    }
  }

  // Called by the Delegate to response to a method requesting a Passkey.
  void OnPasskey(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender,
                 Delegate::Status status,
                 uint32 passkey) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        dbus::Response* response = dbus::Response::FromMethodCall(method_call);
        dbus::MessageWriter writer(response);
        writer.AppendUint32(passkey);
        response_sender.Run(response);
        break;
      }
      case Delegate::REJECTED: {
        dbus::ErrorResponse* response = dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_agent::kErrorRejected, "rejected");
        response_sender.Run(response);
        break;
      }
      case Delegate::CANCELLED: {
        dbus::ErrorResponse* response = dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_agent::kErrorCanceled, "canceled");
        response_sender.Run(response);
        break;
      }
      default:
        NOTREACHED() << "Unexpected status code from delegate: " << status;
    }
  }

  // Called by the Delegate in response to a method requiring confirmation.
  void OnConfirmation(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender,
                      Delegate::Status status) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        dbus::Response* response = dbus::Response::FromMethodCall(method_call);
        response_sender.Run(response);
        break;
      }
      case Delegate::REJECTED: {
        dbus::ErrorResponse* response = dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_agent::kErrorRejected, "rejected");
        response_sender.Run(response);
        break;
      }
      case Delegate::CANCELLED: {
        dbus::ErrorResponse* response = dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_agent::kErrorCanceled, "canceled");
        response_sender.Run(response);
        break;
      }
      default:
        NOTREACHED() << "Unexpected status code from delegate: " << status;
    }
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  dbus::Bus* bus_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  Delegate* delegate_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAgentServiceProviderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAgentServiceProviderImpl);
};

// The BluetoothAgentServiceProvider implementation used on Linux desktop,
// which does nothing.
class BluetoothAgentServiceProviderStubImpl
    : public BluetoothAgentServiceProvider {
 public:
  explicit BluetoothAgentServiceProviderStubImpl(Delegate* delegate_) {
  }

  virtual ~BluetoothAgentServiceProviderStubImpl() {
  }
};

BluetoothAgentServiceProvider::BluetoothAgentServiceProvider() {
}

BluetoothAgentServiceProvider::~BluetoothAgentServiceProvider() {
}

// static
BluetoothAgentServiceProvider* BluetoothAgentServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    Delegate* delegate) {
  if (base::chromeos::IsRunningOnChromeOS()) {
    return new BluetoothAgentServiceProviderImpl(bus, object_path, delegate);
  } else {
    return new BluetoothAgentServiceProviderStubImpl(delegate);
  }
}

}  // namespace chromeos
