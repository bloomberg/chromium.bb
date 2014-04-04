// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_chromeos.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothProfile;
using device::BluetoothSocket;

namespace {

// Check the validity of a file descriptor received from D-Bus. Must be run
// on a thread where i/o is permitted.
scoped_ptr<dbus::FileDescriptor> CheckValidity(
    scoped_ptr<dbus::FileDescriptor> fd) {
  base::ThreadRestrictions::AssertIOAllowed();
  fd->CheckValidity();
  return fd.Pass();
}

}  // namespace


namespace chromeos {

BluetoothProfileChromeOS::BluetoothProfileChromeOS()
    : weak_ptr_factory_(this) {
}

BluetoothProfileChromeOS::~BluetoothProfileChromeOS() {
  DCHECK(object_path_.value().empty());
  DCHECK(profile_.get() == NULL);

  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothProfileChromeOS::Init(
    const device::BluetoothUUID& uuid,
    const device::BluetoothProfile::Options& options,
    const ProfileCallback& callback) {
  DCHECK(object_path_.value().empty());
  DCHECK(profile_.get() == NULL);

  if (!uuid.IsValid()) {
    callback.Run(NULL);
    return;
  }

  uuid_ = uuid;

  options_.name = options.name;
  options_.service = uuid.canonical_value();
  options_.channel = options.channel;
  options_.psm = options.psm;
  options_.require_authentication = options.require_authentication;
  options_.require_authorization = options.require_authorization;
  options_.auto_connect = options.auto_connect;
  options_.version = options.version;
  options_.features = options.features;

  // The object path is relatively meaningless, but has to be unique, so we
  // use the UUID of the profile.
  std::string uuid_path;
  base::ReplaceChars(uuid.canonical_value(), ":-", "_", &uuid_path);

  object_path_ = dbus::ObjectPath("/org/chromium/bluetooth_profile/" +
                                  uuid_path);

  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
  profile_.reset(BluetoothProfileServiceProvider::Create(
      system_bus, object_path_, this));
  DCHECK(profile_.get());

  // Now the profile object is registered we need an adapter to register it
  // with.
  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothProfileChromeOS::OnGetAdapter,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void BluetoothProfileChromeOS::Unregister() {
  DCHECK(!object_path_.value().empty());
  DCHECK(profile_.get());

  profile_.reset();

  VLOG(1) << object_path_.value() << ": Unregister profile";
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->
      UnregisterProfile(
          object_path_,
          base::Bind(&BluetoothProfileChromeOS::OnUnregisterProfile,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BluetoothProfileChromeOS::OnUnregisterProfileError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothProfileChromeOS::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

void BluetoothProfileChromeOS::AdapterPresentChanged(BluetoothAdapter* adapter,
                                                     bool present) {
  if (!present)
    return;

  VLOG(1) << object_path_.value() << ": Register profile";
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->
      RegisterProfile(
          object_path_,
          uuid_.canonical_value(),
          options_,
          base::Bind(&BluetoothProfileChromeOS::OnInternalRegisterProfile,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BluetoothProfileChromeOS::OnInternalRegisterProfileError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothProfileChromeOS::OnGetAdapter(
    const ProfileCallback& callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(!adapter_.get());
  adapter_ = adapter;
  adapter_->AddObserver(this);

  VLOG(1) << object_path_.value() << ": Register profile";
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->
      RegisterProfile(
          object_path_,
          uuid_.canonical_value(),
          options_,
          base::Bind(&BluetoothProfileChromeOS::OnRegisterProfile,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback),
          base::Bind(&BluetoothProfileChromeOS::OnRegisterProfileError,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback));
}

void BluetoothProfileChromeOS::Release() {
  VLOG(1) << object_path_.value() << ": Release";
}

void BluetoothProfileChromeOS::NewConnection(
    const dbus::ObjectPath& device_path,
    scoped_ptr<dbus::FileDescriptor> fd,
    const BluetoothProfileServiceProvider::Delegate::Options& options,
    const ConfirmationCallback& callback) {
  VLOG(1) << object_path_.value() << ": New connection from device: "
          << device_path.value();
  if (connection_callback_.is_null()) {
    callback.Run(REJECTED);
    return;
  }

  // Punt descriptor validity check to a worker thread where i/o is permitted;
  // on return we'll call the connection callback.
  //
  // base::Passed is used to take ownership of the file descriptor during the
  // CheckValidity() call and pass that ownership to callback.
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false).get(),
      FROM_HERE,
      base::Bind(&CheckValidity, base::Passed(&fd)),
      base::Bind(&BluetoothProfileChromeOS::OnCheckValidity,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_path,
                 options,
                 callback));
}

void BluetoothProfileChromeOS::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  VLOG(1) << object_path_.value() << ": Request disconnection";
  callback.Run(SUCCESS);
}

void BluetoothProfileChromeOS::Cancel() {
  VLOG(1) << object_path_.value() << ": Cancel";
}

void BluetoothProfileChromeOS::OnInternalRegisterProfile() {
  VLOG(1) << object_path_.value() << ": Profile registered";
}

void BluetoothProfileChromeOS::OnInternalRegisterProfileError(
    const std::string& error_name,
    const std::string& error_message) {
  // It's okay if the profile already exists, it means we registered it on
  // initialization.
  if (error_name == bluetooth_profile_manager::kErrorAlreadyExists)
    return;

  LOG(WARNING) << object_path_.value() << ": Failed to register profile: "
               << error_name << ": " << error_message;
}

void BluetoothProfileChromeOS::OnRegisterProfile(
    const ProfileCallback& callback) {
  VLOG(1) << object_path_.value() << ": Profile registered";
  callback.Run(this);
}

void BluetoothProfileChromeOS::OnRegisterProfileError(
    const ProfileCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  // It's okay if the profile already exists, it means we registered it when
  // we first saw the adapter.
  if (error_name == bluetooth_profile_manager::kErrorAlreadyExists)
    return;

  LOG(WARNING) << object_path_.value() << ": Failed to register profile: "
               << error_name << ": " << error_message;
  callback.Run(NULL);
}

void BluetoothProfileChromeOS::OnUnregisterProfile() {
  VLOG(1) << object_path_.value() << ": Profile unregistered";
  object_path_ = dbus::ObjectPath("");
  delete this;
}

void BluetoothProfileChromeOS::OnUnregisterProfileError(
    const std::string& error_name,
    const std::string& error_message) {
  // It's okay if the profile didn't exist, it means we never saw an adapter.
  if (error_name == bluetooth_profile_manager::kErrorDoesNotExist)
    return;

  LOG(WARNING) << object_path_.value() << ": Failed to unregister profile: "
               << error_name << ": " << error_message;
  object_path_ = dbus::ObjectPath("");
  delete this;
}

void BluetoothProfileChromeOS::OnCheckValidity(
      const dbus::ObjectPath& device_path,
      const BluetoothProfileServiceProvider::Delegate::Options& options,
      const ConfirmationCallback& callback,
      scoped_ptr<dbus::FileDescriptor> fd) {
  VLOG(1) << object_path_.value() << ": Validity check complete";
  if (!fd->is_valid()) {
    callback.Run(REJECTED);
    return;
  }

  callback.Run(SUCCESS);

  BluetoothDeviceChromeOS* device =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get())->
          GetDeviceWithPath(device_path);
  DCHECK(device);

  scoped_refptr<BluetoothSocket> socket((
      BluetoothSocketChromeOS::Create(fd.get())));
  connection_callback_.Run(device, socket);
}

}  // namespace chromeos
