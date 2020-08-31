// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/fake_bluetooth_delegate.h"

#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "url/origin.h"

using blink::WebBluetoothDeviceId;
using device::BluetoothDevice;
using device::BluetoothUUID;

namespace content {

// public
FakeBluetoothDelegate::FakeBluetoothDelegate() = default;
FakeBluetoothDelegate::~FakeBluetoothDelegate() = default;

WebBluetoothDeviceId FakeBluetoothDelegate::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  auto& device_address_to_id_map = GetAddressToIdMapForOrigin(frame);
  auto it = device_address_to_id_map.find(device_address);
  if (it != device_address_to_id_map.end())
    return it->second;
  return {};
}

std::string FakeBluetoothDelegate::GetDeviceAddress(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  auto& device_address_to_id_map = GetAddressToIdMapForOrigin(frame);
  for (auto& entry : device_address_to_id_map) {
    if (entry.second == device_id)
      return entry.first;
  }
  return std::string();
}

blink::WebBluetoothDeviceId FakeBluetoothDelegate::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return GetOrCreateDeviceIdForDeviceAddress(frame, device_address);
}

WebBluetoothDeviceId FakeBluetoothDelegate::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  WebBluetoothDeviceId device_id =
      GetOrCreateDeviceIdForDeviceAddress(frame, device->GetAddress());
  device_id_to_name_map_[device_id] =
      device->GetName() ? *device->GetName() : std::string();
  GrantUnionOfServicesForDevice(device_id, options);
  return device_id;
}

bool FakeBluetoothDelegate::HasDevicePermission(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return base::Contains(device_id_to_services_map_, device_id);
}

bool FakeBluetoothDelegate::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  auto id_to_services_it = device_id_to_services_map_.find(device_id);
  if (id_to_services_it == device_id_to_services_map_.end())
    return false;

  return base::Contains(id_to_services_it->second, service);
}

bool FakeBluetoothDelegate::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  auto id_to_services_it = device_id_to_services_map_.find(device_id);
  if (id_to_services_it == device_id_to_services_map_.end())
    return false;

  return !id_to_services_it->second.empty();
}

std::vector<blink::mojom::WebBluetoothDevicePtr>
FakeBluetoothDelegate::GetPermittedDevices(RenderFrameHost* frame) {
  std::vector<blink::mojom::WebBluetoothDevicePtr> permitted_devices;
  auto& device_address_to_id_map = GetAddressToIdMapForOrigin(frame);
  for (const auto& entry : device_address_to_id_map) {
    auto permitted_device = blink::mojom::WebBluetoothDevice::New();
    WebBluetoothDeviceId device_id = entry.second;
    permitted_device->id = device_id;
    permitted_device->name = device_id_to_name_map_[device_id];
    permitted_devices.push_back(std::move(permitted_device));
  }
  return permitted_devices;
}

WebBluetoothDeviceId FakeBluetoothDelegate::GetOrCreateDeviceIdForDeviceAddress(
    RenderFrameHost* frame,
    const std::string& device_address) {
  WebBluetoothDeviceId device_id;
  auto& device_address_to_id_map = GetAddressToIdMapForOrigin(frame);
  auto it = device_address_to_id_map.find(device_address);
  if (it != device_address_to_id_map.end()) {
    device_id = it->second;
  } else {
    device_id = WebBluetoothDeviceId::Create();
    device_address_to_id_map[device_address] = device_id;
  }
  return device_id;
}

void FakeBluetoothDelegate::GrantUnionOfServicesForDevice(
    const WebBluetoothDeviceId& device_id,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  if (!options)
    return;

  // Create an entry for |device_id| in |device_id_to_services_map_| to indicate
  // that the site can attempt to GATT connect even if |options| does not
  // contain any services.
  base::flat_set<BluetoothUUID>& granted_services =
      device_id_to_services_map_[device_id];
  if (options->filters) {
    for (const blink::mojom::WebBluetoothLeScanFilterPtr& filter :
         options->filters.value()) {
      if (!filter->services)
        continue;

      for (const BluetoothUUID& uuid : filter->services.value())
        granted_services.insert(uuid);
    }
  }

  for (const BluetoothUUID& uuid : options->optional_services)
    granted_services.insert(uuid);
}

FakeBluetoothDelegate::AddressToIdMap&
FakeBluetoothDelegate::GetAddressToIdMapForOrigin(RenderFrameHost* frame) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  auto origin_pair =
      std::make_pair(frame->GetLastCommittedOrigin(),
                     web_contents->GetMainFrame()->GetLastCommittedOrigin());
  return device_address_to_id_map_for_origin_[origin_pair];
}

}  // namespace content
