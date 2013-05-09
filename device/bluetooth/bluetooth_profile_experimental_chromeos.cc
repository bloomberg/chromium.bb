// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_experimental_chromeos.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/experimental_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/experimental_bluetooth_profile_service_provider.h"
#include "dbus/bus.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_experimental_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_experimental_chromeos.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_experimental_chromeos.h"

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

BluetoothProfileExperimentalChromeOS::BluetoothProfileExperimentalChromeOS()
    : weak_ptr_factory_(this) {
}

BluetoothProfileExperimentalChromeOS::~BluetoothProfileExperimentalChromeOS() {
  DCHECK(object_path_.value().empty());
  DCHECK(profile_.get() == NULL);
}

void BluetoothProfileExperimentalChromeOS::Init(
    const std::string& uuid,
    const device::BluetoothProfile::Options& options,
    const ProfileCallback& callback) {
  DCHECK(object_path_.value().empty());
  DCHECK(profile_.get() == NULL);

  if (!BluetoothDevice::IsUUIDValid(uuid)) {
    callback.Run(NULL);
    return;
  }

  uuid_ = uuid;

  ExperimentalBluetoothProfileManagerClient::Options bluetooth_options;
  bluetooth_options.name = options.name;
  bluetooth_options.service = uuid;
  bluetooth_options.channel = options.channel;
  bluetooth_options.psm = options.psm;
  bluetooth_options.require_authentication = options.require_authentication;
  bluetooth_options.require_authorization = options.require_authorization;
  bluetooth_options.auto_connect = options.auto_connect;
  bluetooth_options.version = options.version;
  bluetooth_options.features = options.features;

  // The object path is relatively meaningless, but has to be unique, so we
  // use the UUID of the profile.
  std::string uuid_path;
  ReplaceChars(uuid, ":-", "_", &uuid_path);

  object_path_ = dbus::ObjectPath("/org/chromium/bluetooth_profile/" +
                                  uuid_path);

  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
  profile_.reset(ExperimentalBluetoothProfileServiceProvider::Create(
      system_bus, object_path_, this));
  DCHECK(profile_.get());

  VLOG(1) << object_path_.value() << ": Register profile";
  DBusThreadManager::Get()->GetExperimentalBluetoothProfileManagerClient()->
      RegisterProfile(
          object_path_,
          uuid,
          bluetooth_options,
          base::Bind(
              &BluetoothProfileExperimentalChromeOS::OnRegisterProfile,
              weak_ptr_factory_.GetWeakPtr(),
              callback),
          base::Bind(
              &BluetoothProfileExperimentalChromeOS::OnRegisterProfileError,
              weak_ptr_factory_.GetWeakPtr(),
              callback));
}

void BluetoothProfileExperimentalChromeOS::Unregister() {
  DCHECK(!object_path_.value().empty());
  DCHECK(profile_.get());

  profile_.reset();

  VLOG(1) << object_path_.value() << ": Unregister profile";
  DBusThreadManager::Get()->GetExperimentalBluetoothProfileManagerClient()->
      UnregisterProfile(
          object_path_,
          base::Bind(
              &BluetoothProfileExperimentalChromeOS::OnUnregisterProfile,
              weak_ptr_factory_.GetWeakPtr()),
          base::Bind(
              &BluetoothProfileExperimentalChromeOS::OnUnregisterProfileError,
              weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothProfileExperimentalChromeOS::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

void BluetoothProfileExperimentalChromeOS::Release() {
  VLOG(1) << object_path_.value() << ": Release";
}

void BluetoothProfileExperimentalChromeOS::NewConnection(
    const dbus::ObjectPath& device_path,
    scoped_ptr<dbus::FileDescriptor> fd,
    const ExperimentalBluetoothProfileServiceProvider::Delegate::Options&
        options,
    const ConfirmationCallback& callback) {
  VLOG(1) << object_path_.value() << ": New connection from device: "
          << device_path.value();;
  if (connection_callback_.is_null()) {
    callback.Run(REJECTED);
    return;
  }

  // Punt descriptor validity check to a worker thread where i/o is permitted;
  // on return we'll fetch the adapter and then call the connection callback.
  //
  // base::Passed is used to take ownership of the file descriptor during the
  // CheckValidity() call and pass that ownership to the GetAdapter() call.
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false),
      FROM_HERE,
      base::Bind(&CheckValidity, base::Passed(&fd)),
      base::Bind(&BluetoothProfileExperimentalChromeOS::GetAdapter,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_path,
                 options,
                 callback));
}

void BluetoothProfileExperimentalChromeOS::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  VLOG(1) << object_path_.value() << ": Request disconnection";
  callback.Run(SUCCESS);
}

void BluetoothProfileExperimentalChromeOS::Cancel() {
  VLOG(1) << object_path_.value() << ": Cancel";
}

void BluetoothProfileExperimentalChromeOS::OnRegisterProfile(
    const ProfileCallback& callback) {
  VLOG(1) << object_path_.value() << ": Profile registered";
  callback.Run(this);
}

void BluetoothProfileExperimentalChromeOS::OnRegisterProfileError(
    const ProfileCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to register profile: "
               << error_name << ": " << error_message;
  callback.Run(NULL);

  Unregister();
}

void BluetoothProfileExperimentalChromeOS::OnUnregisterProfile() {
  VLOG(1) << object_path_.value() << ": Profile unregistered";
  object_path_ = dbus::ObjectPath("");
  delete this;
}

void BluetoothProfileExperimentalChromeOS::OnUnregisterProfileError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to unregister profile: "
               << error_name << ": " << error_message;
  object_path_ = dbus::ObjectPath("");
  delete this;
}

void BluetoothProfileExperimentalChromeOS::GetAdapter(
      const dbus::ObjectPath& device_path,
      const ExperimentalBluetoothProfileServiceProvider::Delegate::Options&
          options,
      const ConfirmationCallback& callback,
      scoped_ptr<dbus::FileDescriptor> fd) {
  VLOG(1) << object_path_.value() << ": Validity check complete";
  if (!fd->is_valid()) {
    callback.Run(REJECTED);
    return;
  }

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothProfileExperimentalChromeOS::OnGetAdapter,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_path,
                 options,
                 callback,
                 base::Passed(&fd)));
}

void BluetoothProfileExperimentalChromeOS::OnGetAdapter(
    const dbus::ObjectPath& device_path,
    const ExperimentalBluetoothProfileServiceProvider::Delegate::Options&
        options,
    const ConfirmationCallback& callback,
    scoped_ptr<dbus::FileDescriptor> fd,
    scoped_refptr<BluetoothAdapter> adapter) {
  VLOG(1) << object_path_.value() << ": Obtained adapter reference";
  callback.Run(SUCCESS);

  BluetoothDeviceExperimentalChromeOS* device =
      static_cast<BluetoothAdapterExperimentalChromeOS*>(adapter.get())->
          GetDeviceWithPath(device_path);
  DCHECK(device);

  scoped_refptr<BluetoothSocket> socket((
      BluetoothSocketExperimentalChromeOS::Create(fd.get())));
  connection_callback_.Run(device, socket);
}

}  // namespace chromeos
