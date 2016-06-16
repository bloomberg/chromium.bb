// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_permission_provider.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/usb/usb_device.h"
#include "device/usb/webusb_descriptors.h"

using content::WebContents;

namespace {

bool FindOriginInDescriptorSet(const device::WebUsbAllowedOrigins* set,
                               const GURL& origin,
                               const uint8_t* configuration_value,
                               const uint8_t* first_interface) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebUsbSecurity))
    return true;

  if (!set)
    return false;
  if (ContainsValue(set->origins, origin))
    return true;
  for (const auto& configuration : set->configurations) {
    if (configuration_value &&
        *configuration_value != configuration.configuration_value)
      continue;
    if (ContainsValue(configuration.origins, origin))
      return true;
    for (const auto& function : configuration.functions) {
      if (first_interface && *first_interface != function.first_interface)
        continue;
      if (ContainsValue(function.origins, origin))
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
    scoped_refptr<const device::UsbDevice> device) const {
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

  if (!chooser_context->HasDevicePermission(requesting_origin, embedding_origin,
                                            device)) {
    return false;
  }

  // On Android it is not possible to read the WebUSB descriptors until Chrome
  // has been granted permission to open it. Instead we grant provisional access
  // to the device and perform the allowed origins check when the client tries
  // to open it.
  if (!device->permission_granted())
    return true;

  return FindOriginInDescriptorSet(device->webusb_allowed_origins(),
                                   requesting_origin, nullptr, nullptr);
}

bool WebUSBPermissionProvider::HasConfigurationPermission(
    uint8_t requested_configuration_value,
    scoped_refptr<const device::UsbDevice> device) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return FindOriginInDescriptorSet(
      device->webusb_allowed_origins(),
      render_frame_host_->GetLastCommittedURL().GetOrigin(),
      &requested_configuration_value, nullptr);
}

bool WebUSBPermissionProvider::HasFunctionPermission(
    uint8_t requested_function,
    uint8_t configuration_value,
    scoped_refptr<const device::UsbDevice> device) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return FindOriginInDescriptorSet(
      device->webusb_allowed_origins(),
      render_frame_host_->GetLastCommittedURL().GetOrigin(),
      &configuration_value, &requested_function);
}

void WebUSBPermissionProvider::IncrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->IncrementConnectionCount(render_frame_host_);
}

void WebUSBPermissionProvider::DecrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->DecrementConnectionCount(render_frame_host_);
}
