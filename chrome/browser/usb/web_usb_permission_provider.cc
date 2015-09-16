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

namespace {

bool FindOriginInDescriptorSet(const device::usb::WebUsbDescriptorSet* set,
                               const GURL& origin) {
  if (!set)
    return false;
  for (size_t i = 0; i < set->origins.size(); ++i) {
    if (origin.spec() == set->origins[i])
      return true;
  }
  for (size_t i = 0; i < set->configurations.size(); ++i) {
    const device::usb::WebUsbConfigurationSubsetPtr& config =
        set->configurations[i];
    for (size_t j = 0; i < config->origins.size(); ++j) {
      if (origin.spec() == config->origins[j])
        return true;
    }
    for (size_t j = 0; j < config->functions.size(); ++j) {
      const device::usb::WebUsbFunctionSubsetPtr& function =
          config->functions[j];
      for (size_t k = 0; k < function->origins.size(); ++k) {
        if (origin.spec() == function->origins[k])
          return true;
      }
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
    mojo::Array<device::usb::DeviceInfoPtr> requested_devices,
    const HasDevicePermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL origin = render_frame_host_->GetLastCommittedURL().GetOrigin();

  mojo::Array<mojo::String> allowed_guids(0);
  for (size_t i = 0; i < requested_devices.size(); ++i) {
    const device::usb::DeviceInfoPtr& device = requested_devices[i];
    if (FindOriginInDescriptorSet(device->webusb_allowed_origins.get(),
                                  origin) &&
        EnableWebUsbOnAnyOrigin())
      allowed_guids.push_back(device->guid);
  }
  callback.Run(allowed_guids.Pass());
}
