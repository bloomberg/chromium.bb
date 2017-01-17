// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_USB_WEB_USB_CHOOSER_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_USB_WEB_USB_CHOOSER_SERVICE_ANDROID_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "device/usb/public/interfaces/chooser_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"

class UsbChooserDialogAndroid;

namespace content {
class RenderFrameHost;
}

// Implementation of the public device::usb::ChooserService interface.
// This interface can be used by a webpage to request permission from user
// to access a certain device.
class WebUsbChooserServiceAndroid : public device::usb::ChooserService {
 public:
  explicit WebUsbChooserServiceAndroid(
      content::RenderFrameHost* render_frame_host);

  ~WebUsbChooserServiceAndroid() override;

  // device::usb::ChooserService:
  void GetPermission(const std::vector<device::UsbDeviceFilter>& device_filters,
                     const GetPermissionCallback& callback) override;

  void Bind(mojo::InterfaceRequest<device::usb::ChooserService> request);

 private:
  content::RenderFrameHost* const render_frame_host_;
  mojo::BindingSet<device::usb::ChooserService> bindings_;
  std::vector<std::unique_ptr<UsbChooserDialogAndroid>>
      usb_chooser_dialog_android_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbChooserServiceAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_USB_WEB_USB_CHOOSER_SERVICE_ANDROID_H_
