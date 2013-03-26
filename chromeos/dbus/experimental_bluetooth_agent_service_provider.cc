// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/experimental_bluetooth_agent_service_provider.h"

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

namespace chromeos {

// The ExperimentalBluetoothAgentServiceProvider implementation used in
// production.
class ExperimentalBluetoothAgentServiceProviderImpl
    : public ExperimentalBluetoothAgentServiceProvider {
 public:
  ExperimentalBluetoothAgentServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        delegate_(delegate),
        object_path_(object_path),
        weak_ptr_factory_(this) {
    VLOG(1) << "Creating Bluetooth Agent: " << object_path_.value();

    exported_object_ = bus_->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kRelease,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::Release,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kRequestPinCode,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::RequestPinCode,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kDisplayPinCode,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::DisplayPinCode,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kRequestPasskey,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::RequestPasskey,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kDisplayPasskey,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::DisplayPasskey,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kRequestConfirmation,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::RequestConfirmation,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kRequestAuthorization,
        base::Bind(
           &ExperimentalBluetoothAgentServiceProviderImpl::RequestAuthorization,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kAuthorizeService,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::AuthorizeService,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kExperimentalBluetoothAgentInterface,
        bluetooth_agent::kCancel,
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::Cancel,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(
            &ExperimentalBluetoothAgentServiceProviderImpl::OnExported,
            weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~ExperimentalBluetoothAgentServiceProviderImpl() {
    VLOG(1) << "Cleaning up Bluetooth Agent: " << object_path_.value();

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

    response_sender.Run(dbus::Response::FromMethodCall(method_call));
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
        &ExperimentalBluetoothAgentServiceProviderImpl::OnPinCode,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestPinCode(device_path, callback);
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

    response_sender.Run(dbus::Response::FromMethodCall(method_call));
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
        &ExperimentalBluetoothAgentServiceProviderImpl::OnPasskey,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestPasskey(device_path, callback);
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
    int16 entered;
    if (!reader.PopObjectPath(&device_path) ||
        !reader.PopUint32(&passkey)) {
      LOG(WARNING) << "DisplayPasskey called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    // This wasn't always provided, play it safe...
    if (!reader.PopInt16(&entered))
      entered = 0;

    delegate_->DisplayPasskey(device_path, passkey, entered);

    response_sender.Run(dbus::Response::FromMethodCall(method_call));
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
        &ExperimentalBluetoothAgentServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestConfirmation(device_path, passkey, callback);
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm an incoming just-works pairing.
  void RequestAuthorization(
        dbus::MethodCall* method_call,
        dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestAuthorization called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback = base::Bind(
        &ExperimentalBluetoothAgentServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->RequestAuthorization(device_path, callback);
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm that that a remote device is authorized to connect to a service
  // UUID.
  void AuthorizeService(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    std::string uuid;
    if (!reader.PopObjectPath(&device_path) ||
        !reader.PopString(&uuid)) {
      LOG(WARNING) << "AuthorizeService called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback = base::Bind(
        &ExperimentalBluetoothAgentServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(),
        method_call,
        response_sender);

    delegate_->AuthorizeService(device_path, uuid, callback);
  }

  // Called by dbus:: when the request failed before a reply was returned
  // from the device.
  void Cancel(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Cancel();

    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
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
        scoped_ptr<dbus::Response> response(
            dbus::Response::FromMethodCall(method_call));
        dbus::MessageWriter writer(response.get());
        writer.AppendString(pincode);
        response_sender.Run(response.Pass());
        break;
      }
      case Delegate::REJECTED: {
        response_sender.Run(
            dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorRejected, "rejected")
            .PassAs<dbus::Response>());
        break;
      }
      case Delegate::CANCELLED: {
        response_sender.Run(
            dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorCanceled, "canceled")
            .PassAs<dbus::Response>());
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
        scoped_ptr<dbus::Response> response(
            dbus::Response::FromMethodCall(method_call));
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(passkey);
        response_sender.Run(response.Pass());
        break;
      }
      case Delegate::REJECTED: {
        response_sender.Run(
            dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorRejected, "rejected")
            .PassAs<dbus::Response>());
        break;
      }
      case Delegate::CANCELLED: {
        response_sender.Run(
            dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorCanceled, "canceled")
            .PassAs<dbus::Response>());
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
        response_sender.Run(dbus::Response::FromMethodCall(method_call));
        break;
      }
      case Delegate::REJECTED: {
        response_sender.Run(
            dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorRejected, "rejected")
            .PassAs<dbus::Response>());
        break;
      }
      case Delegate::CANCELLED: {
        response_sender.Run(
            dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorCanceled, "canceled")
            .PassAs<dbus::Response>());
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
  base::WeakPtrFactory<ExperimentalBluetoothAgentServiceProviderImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothAgentServiceProviderImpl);
};

// The ExperimentalBluetoothAgentServiceProvider implementation used on Linux
// desktop, which does nothing.
class ExperimentalBluetoothAgentServiceProviderStubImpl
    : public ExperimentalBluetoothAgentServiceProvider {
 public:
  explicit ExperimentalBluetoothAgentServiceProviderStubImpl(
      Delegate* delegate) {
  }

  virtual ~ExperimentalBluetoothAgentServiceProviderStubImpl() {
  }
};

ExperimentalBluetoothAgentServiceProvider::
    ExperimentalBluetoothAgentServiceProvider() {
}

ExperimentalBluetoothAgentServiceProvider::
    ~ExperimentalBluetoothAgentServiceProvider() {
}

// static
ExperimentalBluetoothAgentServiceProvider*
    ExperimentalBluetoothAgentServiceProvider::Create(
        dbus::Bus* bus,
        const dbus::ObjectPath& object_path,
        Delegate* delegate) {
  if (base::chromeos::IsRunningOnChromeOS()) {
    return new ExperimentalBluetoothAgentServiceProviderImpl(
        bus, object_path, delegate);
  } else {
    return new ExperimentalBluetoothAgentServiceProviderStubImpl(delegate);
  }
}

}  // namespace chromeos
