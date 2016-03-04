// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_permission_provider.h"

#include <stddef.h>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/usb/public/interfaces/device.mojom.h"

using content::WebContents;
using device::usb::WebUsbDescriptorSet;
using device::usb::WebUsbConfigurationSubsetPtr;
using device::usb::WebUsbFunctionSubsetPtr;

namespace {

bool FindOriginInDescriptorSet(const WebUsbDescriptorSet* set,
                               const GURL& origin,
                               const uint8_t* configuration_value,
                               const uint8_t* interface_number) {
  if (!set)
    return false;
  for (size_t i = 0; i < set->origins.size(); ++i)
    if (origin.spec() == set->origins[i])
      return true;
  for (size_t i = 0; i < set->configurations.size(); ++i) {
    const WebUsbConfigurationSubsetPtr& config = set->configurations[i];
    if (configuration_value &&
        *configuration_value != config->configuration_value)
      continue;
    for (size_t j = 0; i < config->origins.size(); ++j)
      if (origin.spec() == config->origins[j])
        return true;
    for (size_t j = 0; j < config->functions.size(); ++j) {
      const WebUsbFunctionSubsetPtr& function = config->functions[j];
      // TODO(reillyg): Implement support for Interface Association Descriptors
      // so that this check will match associated interfaces.
      if (interface_number && *interface_number != function->first_interface)
        continue;
      for (size_t k = 0; k < function->origins.size(); ++k)
        if (origin.spec() == function->origins[k])
          return true;
    }
  }
  return false;
}

}  // namespace

WebUSBPermissionProvider::WebUSBPermissionProvider(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host_);
}

WebUSBPermissionProvider::~WebUSBPermissionProvider() {}

base::WeakPtr<device::usb::PermissionProvider>
WebUSBPermissionProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WebUSBPermissionProvider::HasDevicePermission(
    const device::usb::DeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);
  GURL embedding_origin =
      web_contents->GetMainFrame()->GetLastCommittedURL().GetOrigin();
  GURL requesting_origin =
      render_frame_host_->GetLastCommittedURL().GetOrigin();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  UsbChooserContext* chooser_context =
      UsbChooserContextFactory::GetForProfile(profile);

  return FindOriginInDescriptorSet(device_info.webusb_allowed_origins.get(),
                                   requesting_origin, nullptr, nullptr) &&
         chooser_context->HasDevicePermission(requesting_origin,
                                              embedding_origin, device_info);
}

bool WebUSBPermissionProvider::HasConfigurationPermission(
    uint8_t requested_configuration_value,
    const device::usb::DeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return FindOriginInDescriptorSet(
      device_info.webusb_allowed_origins.get(),
      render_frame_host_->GetLastCommittedURL().GetOrigin(),
      &requested_configuration_value, nullptr);
}

bool WebUSBPermissionProvider::HasInterfacePermission(
    uint8_t requested_interface,
    uint8_t configuration_value,
    const device::usb::DeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return FindOriginInDescriptorSet(
      device_info.webusb_allowed_origins.get(),
      render_frame_host_->GetLastCommittedURL().GetOrigin(),
      &configuration_value, &requested_interface);
}
