// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/device_manager_impl.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "device/core/device_client.h"
#include "device/usb/device_impl.h"
#include "device/usb/public/cpp/device_manager_delegate.h"
#include "device/usb/public/cpp/device_manager_factory.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/type_converters.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_service.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

// static
void DeviceManagerFactory::Build(mojo::InterfaceRequest<DeviceManager> request,
                                 scoped_ptr<DeviceManagerDelegate> delegate) {
  // Owned by its MessagePipe.
  new DeviceManagerImpl(request.Pass(), delegate.Pass());
}

DeviceManagerImpl::DeviceManagerImpl(
    mojo::InterfaceRequest<DeviceManager> request,
    scoped_ptr<DeviceManagerDelegate> delegate)
    : binding_(this, request.Pass()),
      delegate_(delegate.Pass()),
      weak_factory_(this) {
}

DeviceManagerImpl::~DeviceManagerImpl() {
}

void DeviceManagerImpl::GetDevices(EnumerationOptionsPtr options,
                                   const GetDevicesCallback& callback) {
  DCHECK(DeviceClient::Get());
  UsbService* usb_service = DeviceClient::Get()->GetUsbService();
  if (!usb_service) {
    mojo::Array<EnumerationResultPtr> results(0);
    callback.Run(results.Pass());
    return;
  }
  std::vector<UsbDeviceFilter> filters =
      options->filters.To<std::vector<UsbDeviceFilter>>();
  usb_service->GetDevices(base::Bind(&DeviceManagerImpl::OnGetDevices,
                                     weak_factory_.GetWeakPtr(), callback,
                                     filters));
}

void DeviceManagerImpl::OnGetDevices(
    const GetDevicesCallback& callback,
    const std::vector<UsbDeviceFilter>& filters,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  mojo::Array<EnumerationResultPtr> results(0);
  for (size_t i = 0; i < devices.size(); ++i) {
    DeviceInfoPtr device_info = DeviceInfo::From(*devices[i]);
    if (UsbDeviceFilter::MatchesAny(devices[i], filters) &&
        delegate_->IsDeviceAllowed(*device_info)) {
      EnumerationResultPtr result = EnumerationResult::New();
      DevicePtr device;
      // Owned by its message pipe.
      new DeviceImpl(devices[i], mojo::GetProxy(&device));
      result->device = device.Pass();
      results.push_back(result.Pass());
    }
  }
  callback.Run(results.Pass());
}

}  // namespace usb
}  // namespace device
