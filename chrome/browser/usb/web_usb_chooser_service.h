// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_CHOOSER_SERVICE_H_
#define CHROME_BROWSER_USB_WEB_USB_CHOOSER_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "components/bubble/bubble_reference.h"
#include "device/usb/public/interfaces/chooser_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {
class RenderFrameHost;
}

// Implementation of the public device::usb::ChooserService interface.
// This interface can be used by a webpage to request permission from user
// to access a certain device.
class WebUsbChooserService : public device::usb::ChooserService {
 public:
  explicit WebUsbChooserService(content::RenderFrameHost* render_frame_host);

  ~WebUsbChooserService() override;

  // device::usb::ChooserService:
  void GetPermission(const std::vector<device::UsbDeviceFilter>& device_filters,
                     const GetPermissionCallback& callback) override;

  void Bind(mojo::InterfaceRequest<device::usb::ChooserService> request);

 private:
  content::RenderFrameHost* const render_frame_host_;
  mojo::BindingSet<device::usb::ChooserService> bindings_;
  std::vector<BubbleReference> bubbles_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbChooserService);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_CHOOSER_SERVICE_H_
