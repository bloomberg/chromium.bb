// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle.h"

#include "base/logging.h"

namespace device {

void UsbDeviceHandle::GenericTransfer(UsbEndpointDirection direction,
                                      uint8 endpoint,
                                      scoped_refptr<net::IOBuffer> buffer,
                                      size_t length,
                                      unsigned int timeout,
                                      const TransferCallback& callback) {
  NOTREACHED();
}

}  // namespace device
