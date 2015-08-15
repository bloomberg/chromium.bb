// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_WEBUSB_DESCRIPTORS_H_
#define DEVICE_USB_WEBUSB_DESCRIPTORS_H_

#include <stdint.h>
#include <vector>

#include "url/gurl.h"

namespace device {

struct WebUsbFunctionSubset {
  WebUsbFunctionSubset();
  ~WebUsbFunctionSubset();

  uint8_t first_interface;
  std::vector<GURL> origins;
};

struct WebUsbConfigurationSubset {
  WebUsbConfigurationSubset();
  ~WebUsbConfigurationSubset();

  uint8_t configuration_value;
  std::vector<GURL> origins;
  std::vector<WebUsbFunctionSubset> functions;
};

struct WebUsbDescriptorSet {
  WebUsbDescriptorSet();
  ~WebUsbDescriptorSet();

  bool Parse(const std::vector<uint8_t>& bytes);

  std::vector<GURL> origins;
  std::vector<WebUsbConfigurationSubset> configurations;
};

struct WebUsbPlatformCapabilityDescriptor {
  WebUsbPlatformCapabilityDescriptor();
  ~WebUsbPlatformCapabilityDescriptor();

  bool ParseFromBosDescriptor(const std::vector<uint8_t>& bytes);

  uint16_t version;
  uint8_t vendor_code;
};

bool ParseWebUsbUrlDescriptor(const std::vector<uint8_t>& bytes, GURL* output);

}  // device

#endif  // DEVICE_USB_WEBUSB_DESCRIPTORS_H_
