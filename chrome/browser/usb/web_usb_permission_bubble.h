// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_PERMISSION_BUBBLE_H_
#define CHROME_BROWSER_USB_WEB_USB_PERMISSION_BUBBLE_H_

#include <vector>

#include "base/macros.h"
#include "components/bubble/bubble_reference.h"
#include "components/webusb/public/interfaces/webusb_permission_bubble.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class UsbDevice;
}

// Implementation of the public webusb::WebUsbPermissionBubble interface.
// This interface can be used by a webpage to request permission from user
// to access a certain device.
class ChromeWebUsbPermissionBubble : public webusb::WebUsbPermissionBubble {
 public:
  explicit ChromeWebUsbPermissionBubble(
      content::RenderFrameHost* render_frame_host);

  ~ChromeWebUsbPermissionBubble() override;

  // webusb::WebUsbPermissionBubble:
  void GetPermission(mojo::Array<device::usb::DeviceFilterPtr> device_filters,
                     const GetPermissionCallback& callback) override;
  void Bind(mojo::InterfaceRequest<webusb::WebUsbPermissionBubble> request);

 private:
  content::RenderFrameHost* const render_frame_host_;
  mojo::WeakBindingSet<webusb::WebUsbPermissionBubble> bindings_;
  std::vector<BubbleReference> bubbles_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUsbPermissionBubble);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_PERMISSION_BUBBLE_H_
