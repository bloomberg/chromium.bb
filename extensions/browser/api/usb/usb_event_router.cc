// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_event_router.h"

#include "device/core/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"

namespace usb = extensions::core_api::usb;

using content::BrowserThread;
using device::UsbDevice;
using device::UsbService;

namespace extensions {

namespace {

// Returns true iff the given extension has permission to receive events
// regarding this device.
bool WillDispatchDeviceEvent(scoped_refptr<UsbDevice> device,
                             const base::string16& serial_number,
                             content::BrowserContext* browser_context,
                             const Extension* extension,
                             base::ListValue* event_args) {
  // Check install-time and optional permissions.
  UsbDevicePermission::CheckParam param(
      device->vendor_id(), device->product_id(),
      UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (extension->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    return true;
  }

  // Check permissions granted through chrome.usb.getUserSelectedDevices.
  scoped_ptr<DevicePermissions> device_permissions =
      DevicePermissionsManager::Get(browser_context)
          ->GetForExtension(extension->id());
  if (device_permissions->FindEntry(device, serial_number).get()) {
    return true;
  }

  return false;
}

base::LazyInstance<BrowserContextKeyedAPIFactory<UsbEventRouter>>::Leaky
    g_event_router_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class UsbEventRouter::FileThreadHelper : public UsbService::Observer {
 public:
  FileThreadHelper(base::WeakPtr<UsbEventRouter> usb_event_router)
      : usb_event_router_(usb_event_router), observer_(this) {}
  virtual ~FileThreadHelper() {}

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    UsbService* service = device::DeviceClient::Get()->GetUsbService();
    if (service) {
      observer_.Add(service);
    }
  }

 private:
  // UsbService::Observer implementation.
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    base::string16 serial_number;
    device->GetSerialNumber(&serial_number);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&UsbEventRouter::DispatchEvent, usb_event_router_,
                   usb::OnDeviceAdded::kEventName, device, serial_number));
  }

  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    base::string16 serial_number;
    device->GetSerialNumber(&serial_number);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&UsbEventRouter::DispatchEvent, usb_event_router_,
                   usb::OnDeviceRemoved::kEventName, device, serial_number));
  }

  base::WeakPtr<UsbEventRouter> usb_event_router_;
  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;
};

// static
BrowserContextKeyedAPIFactory<UsbEventRouter>*
UsbEventRouter::GetFactoryInstance() {
  return g_event_router_factory.Pointer();
}

UsbEventRouter::UsbEventRouter(content::BrowserContext* browser_context)
    : browser_context_(browser_context), weak_factory_(this) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->RegisterObserver(this, usb::OnDeviceAdded::kEventName);
    event_router->RegisterObserver(this, usb::OnDeviceRemoved::kEventName);
  }
}

UsbEventRouter::~UsbEventRouter() {
}

void UsbEventRouter::Shutdown() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }
  helper_.reset(nullptr);
}

void UsbEventRouter::OnListenerAdded(const EventListenerInfo& details) {
  if (!helper_) {
    helper_.reset(new FileThreadHelper(weak_factory_.GetWeakPtr()));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&FileThreadHelper::Start, base::Unretained(helper_.get())));
  }
}

void UsbEventRouter::DispatchEvent(const std::string& event_name,
                                   scoped_refptr<UsbDevice> device,
                                   const base::string16& serial_number) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    usb::Device device_obj;
    device_obj.device = device->unique_id();
    device_obj.vendor_id = device->vendor_id();
    device_obj.product_id = device->product_id();

    scoped_ptr<Event> event;
    if (event_name == usb::OnDeviceAdded::kEventName) {
      event.reset(new Event(usb::OnDeviceAdded::kEventName,
                            usb::OnDeviceAdded::Create(device_obj)));
    } else {
      DCHECK(event_name == usb::OnDeviceRemoved::kEventName);
      event.reset(new Event(usb::OnDeviceRemoved::kEventName,
                            usb::OnDeviceRemoved::Create(device_obj)));
    }

    event->will_dispatch_callback =
        base::Bind(&WillDispatchDeviceEvent, device, serial_number);
    event_router->BroadcastEvent(event.Pass());
  }
}

}  // extensions
