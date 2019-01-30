// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_device_manager.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "device/usb/public/mojom/device_enumeration_options.mojom.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace usb = extensions::api::usb;

using content::BrowserThread;

namespace extensions {

namespace {

// Returns true if the given extension has permission to receive events
// regarding this device.
bool WillDispatchDeviceEvent(const device::mojom::UsbDeviceInfo& device_info,
                             content::BrowserContext* browser_context,
                             const Extension* extension,
                             Event* event,
                             const base::DictionaryValue* listener_filter) {
  // Check install-time and optional permissions.
  std::unique_ptr<UsbDevicePermission::CheckParam> param =
      UsbDevicePermission::CheckParam::ForUsbDevice(extension, device_info);
  if (extension->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, param.get())) {
    return true;
  }

  // Check permissions granted through chrome.usb.getUserSelectedDevices.
  DevicePermissions* device_permissions =
      DevicePermissionsManager::Get(browser_context)
          ->GetForExtension(extension->id());
  if (device_permissions->FindUsbDeviceEntry(device_info).get()) {
    return true;
  }

  return false;
}

base::LazyInstance<BrowserContextKeyedAPIFactory<UsbDeviceManager>>::Leaky
    g_event_router_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
UsbDeviceManager* UsbDeviceManager::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<UsbDeviceManager>::Get(browser_context);
}

// static
BrowserContextKeyedAPIFactory<UsbDeviceManager>*
UsbDeviceManager::GetFactoryInstance() {
  return g_event_router_factory.Pointer();
}

void UsbDeviceManager::Observer::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbDeviceManager::Observer::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbDeviceManager::Observer::OnDeviceRemovedCleanup(
    const device::mojom::UsbDeviceInfo& device_info) {}

void UsbDeviceManager::Observer::OnDeviceManagerConnectionError() {}

UsbDeviceManager::UsbDeviceManager(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      client_binding_(this),
      weak_factory_(this) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->RegisterObserver(this, usb::OnDeviceAdded::kEventName);
    event_router->RegisterObserver(this, usb::OnDeviceRemoved::kEventName);
  }
}

UsbDeviceManager::~UsbDeviceManager() {}

void UsbDeviceManager::AddObserver(Observer* observer) {
  EnsureConnectionWithDeviceManager();
  observer_list_.AddObserver(observer);
}

void UsbDeviceManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

int UsbDeviceManager::GetIdFromGuid(const std::string& guid) {
  auto iter = guid_to_id_map_.find(guid);
  if (iter == guid_to_id_map_.end()) {
    auto result = guid_to_id_map_.insert(std::make_pair(guid, next_id_++));
    DCHECK(result.second);
    iter = result.first;
    id_to_guid_map_.insert(std::make_pair(iter->second, guid));
  }
  return iter->second;
}

bool UsbDeviceManager::GetGuidFromId(int id, std::string* guid) {
  auto iter = id_to_guid_map_.find(id);
  if (iter == id_to_guid_map_.end())
    return false;
  *guid = iter->second;
  return true;
}

void UsbDeviceManager::GetApiDevice(
    const device::mojom::UsbDeviceInfo& device_in,
    api::usb::Device* device_out) {
  device_out->device = GetIdFromGuid(device_in.guid);
  device_out->vendor_id = device_in.vendor_id;
  device_out->product_id = device_in.product_id;
  device_out->version = device_in.device_version_major << 8 |
                        device_in.device_version_minor << 4 |
                        device_in.device_version_subminor;
  device_out->product_name =
      base::UTF16ToUTF8(device_in.product_name.value_or(base::string16()));
  device_out->manufacturer_name =
      base::UTF16ToUTF8(device_in.manufacturer_name.value_or(base::string16()));
  device_out->serial_number =
      base::UTF16ToUTF8(device_in.serial_number.value_or(base::string16()));
}

void UsbDeviceManager::GetDevices(
    device::mojom::UsbDeviceManager::GetDevicesCallback callback) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevices(/*options=*/nullptr, std::move(callback));
}

void UsbDeviceManager::GetDevice(
    const std::string& guid,
    device::mojom::UsbDeviceRequest device_request,
    device::mojom::UsbDeviceClientPtr device_client) {
  EnsureConnectionWithDeviceManager();
  device_manager_->GetDevice(guid, std::move(device_request),
                             std::move(device_client));
}

#if defined(OS_CHROMEOS)
void UsbDeviceManager::CheckAccess(
    const std::string& guid,
    device::mojom::UsbDeviceManager::CheckAccessCallback callback) {
  EnsureConnectionWithDeviceManager();
  device_manager_->CheckAccess(guid, std::move(callback));
}
#endif  // defined(OS_CHROMEOS)

void UsbDeviceManager::Shutdown() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }
}

void UsbDeviceManager::OnListenerAdded(const EventListenerInfo& details) {
  EnsureConnectionWithDeviceManager();
}

void UsbDeviceManager::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);

  DispatchEvent(usb::OnDeviceAdded::kEventName, *device_info);

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(*device_info);
}

void UsbDeviceManager::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK(device_info);

  DispatchEvent(usb::OnDeviceRemoved::kEventName, *device_info);

  // Notify all observers for OnDeviceRemoved event.
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(*device_info);

  auto iter = guid_to_id_map_.find(device_info->guid);
  if (iter != guid_to_id_map_.end()) {
    int id = iter->second;
    guid_to_id_map_.erase(iter);
    id_to_guid_map_.erase(id);
  }

  // Notify all observers for OnDeviceRemovedCleanup event.
  for (auto& observer : observer_list_)
    observer.OnDeviceRemovedCleanup(*device_info);
}

void UsbDeviceManager::EnsureConnectionWithDeviceManager() {
  if (device_manager_)
    return;

  // Request UsbDeviceManagerPtr from DeviceService.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName,
                      mojo::MakeRequest(&device_manager_));

  SetUpDeviceManagerConnection();
}

void UsbDeviceManager::SetUpDeviceManagerConnection() {
  DCHECK(device_manager_);
  device_manager_.set_connection_error_handler(
      base::BindOnce(&UsbDeviceManager::OnDeviceManagerConnectionError,
                     base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_binding_);
  device::mojom::UsbDeviceManagerClientAssociatedPtrInfo client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  device_manager_->SetClient(std::move(client));
}

void UsbDeviceManager::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_binding_.Close();

  guid_to_id_map_.clear();
  id_to_guid_map_.clear();

  // Notify all observers.
  for (auto& observer : observer_list_)
    observer.OnDeviceManagerConnectionError();
}

void UsbDeviceManager::DispatchEvent(
    const std::string& event_name,
    const device::mojom::UsbDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    usb::Device device_obj;
    GetApiDevice(device_info, &device_obj);

    std::unique_ptr<Event> event;
    if (event_name == usb::OnDeviceAdded::kEventName) {
      event.reset(new Event(events::USB_ON_DEVICE_ADDED,
                            usb::OnDeviceAdded::kEventName,
                            usb::OnDeviceAdded::Create(device_obj)));
    } else {
      DCHECK(event_name == usb::OnDeviceRemoved::kEventName);
      event.reset(new Event(events::USB_ON_DEVICE_REMOVED,
                            usb::OnDeviceRemoved::kEventName,
                            usb::OnDeviceRemoved::Create(device_obj)));
    }

    event->will_dispatch_callback = base::BindRepeating(
        &WillDispatchDeviceEvent, base::ConstRef(device_info));
    event_router->BroadcastEvent(std::move(event));
  }
}

template <>
void BrowserContextKeyedAPIFactory<
    UsbDeviceManager>::DeclareFactoryDependencies() {
  DependsOn(DevicePermissionsManagerFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
}

}  // namespace extensions
