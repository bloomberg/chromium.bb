// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_tab_helper.h"

#include "chrome/browser/usb/web_usb_permission_provider.h"
#include "device/devices_app/usb/device_manager_impl.h"

using content::RenderFrameHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(UsbTabHelper);

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
  device::usb::DeviceManagerImpl::Create(permission_provider.Pass(),
                                         request.Pass());
}

UsbTabHelper::UsbTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void UsbTabHelper::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  permission_provider_.erase(render_frame_host);
}

void UsbTabHelper::GetPermissionProvider(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<device::usb::PermissionProvider> request) {
  const auto it = permission_provider_.find(render_frame_host);
  if (it == permission_provider_.end()) {
    scoped_ptr<WebUSBPermissionProvider> permission_provider(
        new WebUSBPermissionProvider(render_frame_host));
    permission_provider->Bind(request.Pass());
    permission_provider_.set(render_frame_host, permission_provider.Pass());
  } else {
    it->second->Bind(request.Pass());
  }
}
