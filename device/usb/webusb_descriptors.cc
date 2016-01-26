// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <iterator>

#include "base/logging.h"
#include "device/usb/webusb_descriptors.h"

namespace device {

namespace {

// These constants are defined by the Universal Serial Device 3.0 Specification
// Revision 1.0.
const uint8_t kBosDescriptorType = 0x0F;
const uint8_t kDeviceCapabilityDescriptorType = 0x10;

const uint8_t kPlatformDevCapabilityType = 0x05;

// These constants are defined by the WebUSB specification:
// http://wicg.github.io/webusb/
const uint8_t kWebUsbCapabilityUUID[16] = {
    // Little-endian encoding of {3408b638-09a9-47a0-8bfd-a0768815b665}.
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65};

const uint8_t kDescriptorSetDescriptorType = 0x00;
const uint8_t kConfigurationSubsetDescriptorType = 0x01;
const uint8_t kFunctionSubsetDescriptorType = 0x02;
const uint8_t kUrlDescriptorType = 0x03;

bool ParseUrl(GURL* url,
              std::vector<uint8_t>::const_iterator* it,
              std::vector<uint8_t>::const_iterator end) {
  // These conditions must be guaranteed by the caller.
  DCHECK(*it != end);
  uint8_t length = (*it)[0];
  DCHECK_LE(length, std::distance(*it, end));
  DCHECK_GE(length, 2);
  DCHECK_EQ((*it)[1], kUrlDescriptorType);

  if (length == 2) {
    return false;
  }

  const char* str = reinterpret_cast<const char*>(&(*it)[2]);
  *url = GURL(std::string(str, length - 2));
  if (!url->is_valid()) {
    return false;
  }

  std::advance(*it, length);
  return true;
}

bool ParseFunction(WebUsbFunctionSubset* function,
                   std::vector<uint8_t>::const_iterator* it,
                   std::vector<uint8_t>::const_iterator end) {
  // These conditions must be guaranteed by the caller.
  DCHECK(*it != end);
  uint8_t length = (*it)[0];
  DCHECK_LE(length, std::distance(*it, end));
  DCHECK_GE(length, 2);
  DCHECK_EQ((*it)[1], kFunctionSubsetDescriptorType);

  if (length != 5) {
    return false;
  }

  function->first_interface = (*it)[2];

  // Validate the Function Subset header.
  uint16_t total_length = (*it)[3] + ((*it)[4] << 8);
  if (length > total_length || total_length > std::distance(*it, end)) {
    return false;
  }

  end = *it + total_length;
  std::advance(*it, length);

  while (*it != end) {
    uint8_t length = (*it)[0];
    if (length < 2 || std::distance(*it, end) < length) {
      return false;
    }

    uint8_t type = (*it)[1];
    if (type == kUrlDescriptorType) {
      GURL url;
      if (!ParseUrl(&url, it, end)) {
        return false;
      }
      function->origins.push_back(url.GetOrigin());
    } else {
      return false;
    }
  }

  return true;
}

bool ParseConfiguration(WebUsbConfigurationSubset* configuration,
                        std::vector<uint8_t>::const_iterator* it,
                        std::vector<uint8_t>::const_iterator end) {
  // These conditions must be guaranteed by the caller.
  DCHECK(*it != end);
  uint8_t length = (*it)[0];
  DCHECK_LE(length, std::distance(*it, end));
  DCHECK_GE(length, 2);
  DCHECK_EQ((*it)[1], kConfigurationSubsetDescriptorType);

  if (length != 5) {
    return false;
  }

  configuration->configuration_value = (*it)[2];

  // Validate the Configuration Subset header.
  uint16_t total_length = (*it)[3] + ((*it)[4] << 8);
  if (length > total_length || total_length > std::distance(*it, end)) {
    return false;
  }

  end = *it + total_length;
  std::advance(*it, length);

  while (*it != end) {
    uint8_t length = (*it)[0];
    if (length < 2 || std::distance(*it, end) < length) {
      return false;
    }

    uint8_t type = (*it)[1];
    if (type == kFunctionSubsetDescriptorType) {
      WebUsbFunctionSubset function;
      if (!ParseFunction(&function, it, end)) {
        return false;
      }
      configuration->functions.push_back(function);
    } else if (type == kUrlDescriptorType) {
      GURL url;
      if (!ParseUrl(&url, it, end)) {
        return false;
      }
      configuration->origins.push_back(url.GetOrigin());
    } else {
      return false;
    }
  }

  return true;
}

}  // namespace

WebUsbFunctionSubset::WebUsbFunctionSubset() : first_interface(0) {}

WebUsbFunctionSubset::~WebUsbFunctionSubset() {}

WebUsbConfigurationSubset::WebUsbConfigurationSubset()
    : configuration_value(0) {}

WebUsbConfigurationSubset::~WebUsbConfigurationSubset() {}

WebUsbDescriptorSet::WebUsbDescriptorSet() {}

WebUsbDescriptorSet::~WebUsbDescriptorSet() {}

bool WebUsbDescriptorSet::Parse(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 4) {
    return false;
  }

  // Validate the descriptor set header.
  uint16_t total_length = bytes[2] + (bytes[3] << 8);
  if (bytes[0] != 4 ||                                    // bLength
      bytes[1] != kDescriptorSetDescriptorType ||         // bDescriptorType
      4 > total_length || total_length > bytes.size()) {  // wTotalLength
    return false;
  }

  std::vector<uint8_t>::const_iterator it = bytes.begin();
  std::vector<uint8_t>::const_iterator end = it + total_length;
  std::advance(it, 4);

  while (it != bytes.end()) {
    uint8_t length = it[0];
    if (length < 2 || std::distance(it, end) < length) {
      return false;
    }

    uint8_t type = it[1];
    if (type == kConfigurationSubsetDescriptorType) {
      WebUsbConfigurationSubset configuration;
      if (!ParseConfiguration(&configuration, &it, end)) {
        return false;
      }
      configurations.push_back(configuration);
    } else if (type == kUrlDescriptorType) {
      GURL url;
      if (!ParseUrl(&url, &it, end)) {
        return false;
      }
      origins.push_back(url.GetOrigin());
    } else {
      return false;
    }
  }

  return true;
}

WebUsbPlatformCapabilityDescriptor::WebUsbPlatformCapabilityDescriptor()
    : version(0), vendor_code(0) {}

WebUsbPlatformCapabilityDescriptor::~WebUsbPlatformCapabilityDescriptor() {}

bool WebUsbPlatformCapabilityDescriptor::ParseFromBosDescriptor(
    const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 5) {
    // Too short for the BOS descriptor header.
    return false;
  }

  // Validate the BOS descriptor, defined in Table 9-12 of the Universal Serial
  // Bus 3.1 Specification, Revision 1.0.
  uint16_t total_length = bytes[2] + (bytes[3] << 8);
  if (bytes[0] != 5 ||                                    // bLength
      bytes[1] != kBosDescriptorType ||                   // bDescriptorType
      5 > total_length || total_length > bytes.size()) {  // wTotalLength
    return false;
  }

  uint8_t num_device_caps = bytes[4];
  std::vector<uint8_t>::const_iterator it = bytes.begin();
  std::vector<uint8_t>::const_iterator end = it + total_length;
  std::advance(it, 5);

  uint8_t length = 0;
  bool found_vendor_code = false;
  for (size_t i = 0; i < num_device_caps; ++i, std::advance(it, length)) {
    if (it == end) {
      return false;
    }

    // Validate the Device Capability descriptor, defined in Table 9-13 of the
    // Universal Serial Bus 3.1 Specification, Revision 1.0.
    length = it[0];
    if (length < 3 || std::distance(it, end) < length ||  // bLength
        it[1] != kDeviceCapabilityDescriptorType) {       // bDescriptorType
      return false;
    }

    if (it[2] != kPlatformDevCapabilityType) {  // bDevCapabilityType
      continue;
    }

    // Validate the Platform Capability Descriptor, defined in Table 9-18 of the
    // Universal Serial Bus 3.1 Specification, Revision 1.0.
    if (length < 20) {
      // Platform capability descriptors must be at least 20 bytes.
      return false;
    }

    if (memcmp(&it[4], kWebUsbCapabilityUUID, sizeof(kWebUsbCapabilityUUID)) !=
        0) {  // PlatformCapabilityUUID
      continue;
    }

    if (length < 23) {
      // The WebUSB capability descriptor must be at least 23 bytes (to allow
      // for future versions).
      return false;
    }

    version = it[20] + (it[21] << 8);  // bcdVersion
    if (version < 0x0100) {
      continue;
    }

    // Version 1.0 only defines a single field, bVendorCode.
    vendor_code = it[22];
    found_vendor_code = true;
  }

  return found_vendor_code;
}

bool ParseWebUsbUrlDescriptor(const std::vector<uint8_t>& bytes, GURL* output) {
  if (bytes.size() < 2) {
    return false;
  }
  uint8_t length = bytes[0];
  if (length != bytes.size() || bytes[1] != kUrlDescriptorType) {
    return false;
  }
  std::vector<uint8_t>::const_iterator it = bytes.begin();
  return ParseUrl(output, &it, bytes.end());
}

}  // namespace device
