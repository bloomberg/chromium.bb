// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_ERROR_H_
#define COMPONENTS_USB_SERVICE_USB_ERROR_H_

#include <string>

namespace usb_service {

// A number of libusb functions which return a member of enum libusb_error
// return an int instead so for convenience this function takes an int.
std::string ConvertErrorToString(int errcode);

}  // namespace usb_service;

#endif  // COMPONENTS_USB_SERVICE_USB_ERROR_H_
