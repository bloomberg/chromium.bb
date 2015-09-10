// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_permission_provider.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"
#include "device/usb/webusb_descriptors.h"

namespace {

bool FindOriginInDescriptorSet(const device::WebUsbDescriptorSet* set,
                               const GURL& origin) {
  if (!set)
    return false;
  if (ContainsValue(set->origins, origin))
    return true;
  for (const device::WebUsbConfigurationSubset& config_subset :
       set->configurations) {
    if (ContainsValue(config_subset.origins, origin))
      return true;
    for (const device::WebUsbFunctionSubset& function_subset :
         config_subset.functions) {
      if (ContainsValue(function_subset.origins, origin))
        return true;
    }
  }
  return false;
}

bool EnableWebUsbOnAnyOrigin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebUsbOnAnyOrigin);
}

}  // namespace

// static
void WebUSBPermissionProvider::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<PermissionProvider> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);

  // The created object is strongly bound to (and owned by) the pipe.
  new WebUSBPermissionProvider(render_frame_host, request.Pass());
}

WebUSBPermissionProvider::~WebUSBPermissionProvider() {}

WebUSBPermissionProvider::WebUSBPermissionProvider(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<PermissionProvider> request)
    : binding_(this, request.Pass()),
      render_frame_host_(render_frame_host) {}

void WebUSBPermissionProvider::HasDevicePermission(
    mojo::Array<mojo::String> requested_guids,
    const HasDevicePermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  GURL origin = render_frame_host_->GetLastCommittedURL().GetOrigin();

  mojo::Array<mojo::String> allowed_guids(0);
  for (size_t i = 0; i < requested_guids.size(); ++i) {
    const mojo::String& guid = requested_guids[i];
    scoped_refptr<device::UsbDevice> device = usb_service->GetDevice(guid);
    if (FindOriginInDescriptorSet(device->webusb_allowed_origins(), origin) &&
        EnableWebUsbOnAnyOrigin())
      allowed_guids.push_back(guid);
  }
  callback.Run(allowed_guids.Pass());
}
