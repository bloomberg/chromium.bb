// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usb/web_usb_chooser_service_android.h"

#include <utility>

#include "chrome/browser/ui/android/usb_chooser_dialog_android.h"
#include "content/public/browser/browser_thread.h"

WebUsbChooserServiceAndroid::WebUsbChooserServiceAndroid(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host_);
}

WebUsbChooserServiceAndroid::~WebUsbChooserServiceAndroid() {}

void WebUsbChooserServiceAndroid::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  usb_chooser_dialog_android_.push_back(
      std::make_unique<UsbChooserDialogAndroid>(
          std::move(device_filters), render_frame_host_, std::move(callback)));
}

void WebUsbChooserServiceAndroid::Bind(
    mojo::InterfaceRequest<device::mojom::UsbChooserService> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bindings_.AddBinding(this, std::move(request));
}
