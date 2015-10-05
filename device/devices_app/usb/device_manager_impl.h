// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_DEVICE_MANAGER_IMPL_H_
#define DEVICE_USB_DEVICE_MANAGER_IMPL_H_

#include <queue>
#include <set>

#include "base/containers/scoped_ptr_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/devices_app/usb/public/interfaces/device_manager.mojom.h"
#include "device/devices_app/usb/public/interfaces/permission_provider.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class UsbDevice;
class UsbDeviceFilter;
class UsbDeviceHandle;

namespace usb {

class DeviceManagerDelegate;

// Implementation of the public DeviceManager interface. This interface can be
// requested from the devices app located at "mojo:devices", if available.
class DeviceManagerImpl : public DeviceManager {
 public:
  using DeviceList = std::vector<scoped_refptr<UsbDevice>>;
  using DeviceMap = std::map<std::string, scoped_refptr<device::UsbDevice>>;

  DeviceManagerImpl(
      mojo::InterfaceRequest<DeviceManager> request,
      PermissionProviderPtr permission_provider,
      scoped_refptr<base::SequencedTaskRunner> service_task_runner);
  ~DeviceManagerImpl() override;

  void set_connection_error_handler(const mojo::Closure& error_handler) {
    connection_error_handler_ = error_handler;
  }

 private:
  class ServiceThreadHelper;

  // DeviceManager implementation:
  void GetDevices(EnumerationOptionsPtr options,
                  const GetDevicesCallback& callback) override;
  void GetDeviceChanges(const GetDeviceChangesCallback& callback) override;
  void GetDevice(const mojo::String& guid,
                 mojo::InterfaceRequest<Device> device_request) override;

  // Callbacks to handle the async responses from the underlying UsbService.
  void OnGetDevice(mojo::InterfaceRequest<Device> device_request,
                   scoped_refptr<UsbDevice> device);
  void OnGetDevicePermissionCheckComplete(
      scoped_refptr<device::UsbDevice> device,
      mojo::InterfaceRequest<Device> device_request,
      mojo::Array<mojo::String> allowed_guids);
  void OnGetDevices(EnumerationOptionsPtr options,
                    const GetDevicesCallback& callback,
                    const DeviceList& devices);

  // Methods called by |helper_| when devices are added or removed.
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device);
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device);
  void MaybeRunDeviceChangesCallback();
  void OnEnumerationPermissionCheckComplete(
      const DeviceMap& devices_added,
      const DeviceMap& devices_removed,
      mojo::Array<mojo::String> allowed_guids);

  PermissionProviderPtr permission_provider_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  // If there are unfinished calls to GetDeviceChanges their callbacks
  // are stored in |device_change_callbacks_|. Otherwise device changes
  // are collected in |devices_added_| and |devices_removed_| until the
  // next call to GetDeviceChanges.
  std::queue<GetDeviceChangesCallback> device_change_callbacks_;
  DeviceMap devices_added_;
  DeviceMap devices_removed_;
  // To ensure that GetDeviceChangesCallbacks are called in the correct order
  // only perform a single request to |permission_provider_| at a time.
  bool permission_request_pending_ = false;

  // |helper_| is owned by the service thread and holds a weak reference
  // back to the device manager that created it.
  ServiceThreadHelper* helper_;

  mojo::Closure connection_error_handler_;

  mojo::Binding<DeviceManager> binding_;
  base::WeakPtrFactory<DeviceManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerImpl);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_DEVICE_MANAGER_IMPL_H_
