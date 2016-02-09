// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_BUBBLE_DELEGATE_H_

#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "components/bubble/bubble_reference.h"
#include "components/webusb/public/interfaces/webusb_permission_bubble.mojom.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/array.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class UsbDevice;
class UsbDeviceFilter;
}

class Browser;

// UsbChooserBubbleDelegate creates a chooser bubble for WebUsb.
class UsbChooserBubbleDelegate : public ChooserBubbleDelegate,
                                 public device::UsbService::Observer {
 public:
  UsbChooserBubbleDelegate(
      content::RenderFrameHost* owner,
      mojo::Array<device::usb::DeviceFilterPtr> device_filters,
      content::RenderFrameHost* render_frame_host,
      const webusb::WebUsbPermissionBubble::GetPermissionCallback& callback);
  ~UsbChooserBubbleDelegate() override;

  // ChooserBubbleDelegate:
  size_t NumOptions() const override;
  const base::string16& GetOption(size_t index) const override;
  void Select(size_t index) override;
  void Cancel() override;
  void Close() override;

  // device::UsbService::Observer:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  void GotUsbDeviceList(
      const std::vector<scoped_refptr<device::UsbDevice>>& devices);

  void set_bubble_controller(BubbleReference bubble_controller);

 private:
  content::RenderFrameHost* const render_frame_host_;
  webusb::WebUsbPermissionBubble::GetPermissionCallback callback_;
  ScopedObserver<device::UsbService, device::UsbService::Observer>
      usb_service_observer_;
  std::vector<device::UsbDeviceFilter> filters_;
  // Each pair is a (device, device name).
  std::vector<std::pair<scoped_refptr<device::UsbDevice>, base::string16>>
      devices_;
  BubbleReference bubble_controller_;
  base::WeakPtrFactory<UsbChooserBubbleDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbChooserBubbleDelegate);
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_BUBBLE_DELEGATE_H_
