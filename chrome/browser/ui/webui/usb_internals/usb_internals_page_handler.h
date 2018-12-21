// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "device/usb/public/mojom/device_manager_test.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

class UsbInternalsPageHandler : public mojom::UsbInternalsPageHandler {
 public:
  explicit UsbInternalsPageHandler(
      mojom::UsbInternalsPageHandlerRequest request);
  ~UsbInternalsPageHandler() override;

  // mojom::UsbInternalsPageHandler overrides:
  void BindTestInterface(
      device::mojom::UsbDeviceManagerTestRequest request) override;

 private:
  mojo::Binding<mojom::UsbInternalsPageHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(UsbInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_PAGE_HANDLER_H_
