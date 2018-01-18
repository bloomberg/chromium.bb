// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_UTIL_H_
#define CHROME_BROWSER_USB_USB_UTIL_H_

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"

class Browser;

namespace device {
class UsbDevice;
}

base::string16 FormatUsbDeviceName(scoped_refptr<device::UsbDevice> device);

Browser* GetBrowser();

#endif  // CHROME_BROWSER_USB_USB_UTIL_H_
