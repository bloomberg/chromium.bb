// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_tab_helper.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/usb/web_usb_permission_bubble.h"
#include "chrome/browser/usb/web_usb_permission_provider.h"
#include "device/devices_app/usb/device_manager_impl.h"

using content::RenderFrameHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(UsbTabHelper);

struct FrameUsbServices {
  scoped_ptr<WebUSBPermissionProvider> permission_provider;
  scoped_ptr<ChromeWebUsbPermissionBubble> permission_bubble;
};

// static
UsbTabHelper* UsbTabHelper::GetOrCreateForWebContents(
    WebContents* web_contents) {
  UsbTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper) {
    CreateForWebContents(web_contents);
    tab_helper = FromWebContents(web_contents);
  }
  return tab_helper;
}

UsbTabHelper::~UsbTabHelper() {}

void UsbTabHelper::CreateDeviceManager(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<device::usb::DeviceManager> request) {
  DCHECK(WebContents::FromRenderFrameHost(render_frame_host) == web_contents());
  device::usb::PermissionProviderPtr permission_provider;
  GetPermissionProvider(render_frame_host,
                        mojo::GetProxy(&permission_provider));
  device::usb::DeviceManagerImpl::Create(std::move(permission_provider),
                                         std::move(request));
}

#if !defined(OS_ANDROID)
void UsbTabHelper::CreatePermissionBubble(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<webusb::WebUsbPermissionBubble> request) {
  GetPermissionBubble(render_frame_host, std::move(request));
}
#endif  // !defined(OS_ANDROID)

UsbTabHelper::UsbTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void UsbTabHelper::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  frame_usb_services_.erase(render_frame_host);
}

FrameUsbServices* UsbTabHelper::GetFrameUsbService(
    content::RenderFrameHost* render_frame_host) {
  FrameUsbServicesMap::const_iterator it =
      frame_usb_services_.find(render_frame_host);
  if (it == frame_usb_services_.end()) {
    scoped_ptr<FrameUsbServices> frame_usb_services(new FrameUsbServices());
    it = (frame_usb_services_.insert(
              std::make_pair(render_frame_host, std::move(frame_usb_services))))
             .first;
  }
  return it->second.get();
}

void UsbTabHelper::GetPermissionProvider(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<device::usb::PermissionProvider> request) {
  FrameUsbServices* frame_usb_services = GetFrameUsbService(render_frame_host);
  if (!frame_usb_services->permission_provider) {
    frame_usb_services->permission_provider.reset(
        new WebUSBPermissionProvider(render_frame_host));
  }
  frame_usb_services->permission_provider->Bind(std::move(request));
}

#if !defined(OS_ANDROID)
void UsbTabHelper::GetPermissionBubble(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<webusb::WebUsbPermissionBubble> request) {
  FrameUsbServices* frame_usb_services = GetFrameUsbService(render_frame_host);
  if (!frame_usb_services->permission_bubble) {
    frame_usb_services->permission_bubble.reset(
        new ChromeWebUsbPermissionBubble(render_frame_host));
  }
  frame_usb_services->permission_bubble->Bind(std::move(request));
}
#endif  // !defined(OS_ANDROID)
