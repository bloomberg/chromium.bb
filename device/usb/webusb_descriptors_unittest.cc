// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "device/usb/webusb_descriptors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

const uint8_t kExampleBosDescriptor[] = {
    // BOS descriptor.
    0x05, 0x0F, 0x4C, 0x00, 0x03,

    // Container ID descriptor.
    0x14, 0x10, 0x04, 0x00, 0x2A, 0xF9, 0xF6, 0xC2, 0x98, 0x10, 0x2B, 0x49,
    0x8E, 0x64, 0xFF, 0x01, 0x0C, 0x7F, 0x94, 0xE1,

    // WebUSB Platform Capability descriptor.
    0x17, 0x10, 0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65, 0x00, 0x01, 0x42,

    // Microsoft OS 2.0 Platform Capability descriptor.
    0x1C, 0x10, 0x05, 0x00, 0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F, 0x00, 0x00, 0x03, 0x06,
    0x00, 0x00, 0x01, 0x00};

const uint8_t kExampleDescriptorSet[] = {
    // Descriptor set header.
    0x04, 0x00, 0x9E, 0x00,

    // URL descriptor: https://example.com:80
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x', 'a', 'm', 'p',
    'l', 'e', '.', 'c', 'o', 'm', ':', '8', '0',

    // Configuration subset header. {
    0x05, 0x01, 0x01, 0x6A, 0x00,

    //   URL descriptor: https://example.com:81
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x', 'a', 'm', 'p',
    'l', 'e', '.', 'c', 'o', 'm', ':', '8', '1',

    //   Function subset header. {
    0x05, 0x02, 0x01, 0x35, 0x00,

    //     URL descriptor: https://example.com:82
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x', 'a', 'm', 'p',
    'l', 'e', '.', 'c', 'o', 'm', ':', '8', '2',

    //     URL descriptor: https://example.com:83
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x', 'a', 'm', 'p',
    'l', 'e', '.', 'c', 'o', 'm', ':', '8', '3',

    //   }
    //   URL descriptor: https://example.com:84
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x', 'a', 'm', 'p',
    'l', 'e', '.', 'c', 'o', 'm', ':', '8', '4',

    // }
    // URL descriptor: https://example.com:85
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x', 'a', 'm', 'p',
    'l', 'e', '.', 'c', 'o', 'm', ':', '8', '5',
};

const uint8_t kExampleUrlDescriptor[] = {
    0x18, 0x03, 'h', 't', 't', 'p', 's', ':', '/', '/', 'e', 'x',
    'a',  'm',  'p', 'l', 'e', '.', 'c', 'o', 'm', ':', '8', '0'};

class WebUsbDescriptorsTest : public ::testing::Test {};

TEST_F(WebUsbDescriptorsTest, PlatformCapabilityDescriptor) {
  WebUsbPlatformCapabilityDescriptor descriptor;

  ASSERT_TRUE(descriptor.ParseFromBosDescriptor(std::vector<uint8_t>(
      kExampleBosDescriptor,
      kExampleBosDescriptor + sizeof(kExampleBosDescriptor))));
  EXPECT_EQ(0x0100, descriptor.version);
  EXPECT_EQ(0x42, descriptor.vendor_code);
}

TEST_F(WebUsbDescriptorsTest, ShortBosDescriptorHeader) {
  // This BOS descriptor is just too short.
  static const uint8_t kBuffer[] = {0x03, 0x0F, 0x03};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongBosDescriptorHeader) {
  // BOS descriptor's bLength is too large.
  static const uint8_t kBuffer[] = {0x06, 0x0F, 0x05, 0x00, 0x01};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, InvalidBosDescriptor) {
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(std::vector<uint8_t>(
      kExampleUrlDescriptor,
      kExampleUrlDescriptor + sizeof(kExampleUrlDescriptor))));
}

TEST_F(WebUsbDescriptorsTest, ShortBosDescriptor) {
  // wTotalLength is less than bLength. bNumDeviceCaps == 1 to expose buffer
  // length checking bugs.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x04, 0x00, 0x01};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongBosDescriptor) {
  // wTotalLength is too large. bNumDeviceCaps == 1 to expose buffer
  // length checking bugs.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x06, 0x00, 0x01};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, UnexpectedlyEmptyBosDescriptor) {
  // bNumDeviceCaps == 1 but there are no actual descriptors.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x05, 0x00, 0x01};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortCapabilityDescriptor) {
  // The single capability descriptor in the BOS descriptor is too short.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x06, 0x00, 0x01, 0x02, 0x10};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongCapabilityDescriptor) {
  // The bLength on a capability descriptor in the BOS descriptor is longer than
  // the remaining space defined by wTotalLength.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x08, 0x00,
                                    0x01, 0x04, 0x10, 0x05};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, NotACapabilityDescriptor) {
  // There is something other than a device capability descriptor in the BOS
  // descriptor.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x08, 0x00,
                                    0x01, 0x03, 0x0F, 0x05};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, NoPlatformCapabilityDescriptor) {
  // The BOS descriptor only contains a Container ID descriptor.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x19, 0x00, 0x01, 0x14, 0x10,
                                    0x04, 0x00, 0x2A, 0xF9, 0xF6, 0xC2, 0x98,
                                    0x10, 0x2B, 0x49, 0x8E, 0x64, 0xFF, 0x01,
                                    0x0C, 0x7F, 0x94, 0xE1};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortPlatformCapabilityDescriptor) {
  // The platform capability descriptor is too short to contain a UUID.
  static const uint8_t kBuffer[] = {
      0x05, 0x0F, 0x18, 0x00, 0x01, 0x13, 0x10, 0x05, 0x00, 0x2A, 0xF9, 0xF6,
      0xC2, 0x98, 0x10, 0x2B, 0x49, 0x8E, 0x64, 0xFF, 0x01, 0x0C, 0x7F, 0x94};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, NoWebUsbCapabilityDescriptor) {
  // The BOS descriptor only contains another kind of platform capability
  // descriptor.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x19, 0x00, 0x01, 0x14, 0x10,
                                    0x05, 0x00, 0x2A, 0xF9, 0xF6, 0xC2, 0x98,
                                    0x10, 0x2B, 0x49, 0x8E, 0x64, 0xFF, 0x01,
                                    0x0C, 0x7F, 0x94, 0xE1};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortWebUsbPlatformCapabilityDescriptor) {
  // The WebUSB Platform Capability Descriptor is too short.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x19, 0x00, 0x01, 0x14, 0x10,
                                    0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9,
                                    0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
                                    0x88, 0x15, 0xB6, 0x65};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, WebUsbPlatformCapabilityDescriptorOutOfDate) {
  // The WebUSB Platform Capability Descriptor is version 0.9 (too old).
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x1C, 0x00, 0x01, 0x17, 0x10,
                                    0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9,
                                    0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
                                    0x88, 0x15, 0xB6, 0x65, 0x90, 0x00, 0x01};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, DescriptorSet) {
  WebUsbDescriptorSet descriptor;

  ASSERT_TRUE(descriptor.Parse(std::vector<uint8_t>(
      kExampleDescriptorSet,
      kExampleDescriptorSet + sizeof(kExampleDescriptorSet))));
  EXPECT_EQ(2u, descriptor.origins.size());
  EXPECT_EQ(GURL("https://example.com:80"), descriptor.origins[0]);
  EXPECT_EQ(GURL("https://example.com:85"), descriptor.origins[1]);
  EXPECT_EQ(1u, descriptor.configurations.size());

  const WebUsbConfigurationSubset& config1 = descriptor.configurations[0];
  EXPECT_EQ(2u, config1.origins.size());
  EXPECT_EQ(GURL("https://example.com:81"), config1.origins[0]);
  EXPECT_EQ(GURL("https://example.com:84"), config1.origins[1]);
  EXPECT_EQ(1u, config1.functions.size());

  const WebUsbFunctionSubset& function1 = config1.functions[0];
  EXPECT_EQ(2u, function1.origins.size());
  EXPECT_EQ(GURL("https://example.com:82"), function1.origins[0]);
  EXPECT_EQ(GURL("https://example.com:83"), function1.origins[1]);
}

TEST_F(WebUsbDescriptorsTest, ShortDescriptorSetHeader) {
  // bLength is too short for a WebUSB Descriptor Set Header.
  static const uint8_t kBuffer[] = {0x03, 0x00, 0x03};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongDescriptorSetHeader) {
  // bLength is too long for a WebUSB DescriptorSet Header.
  static const uint8_t kBuffer[] = {0x05, 0x00, 0x04, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, UrlNotDescriptorSet) {
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(std::vector<uint8_t>(
      kExampleUrlDescriptor,
      kExampleUrlDescriptor + sizeof(kExampleUrlDescriptor))));
}

TEST_F(WebUsbDescriptorsTest, ShortDescriptorSet) {
  // wTotalLength is shorter than bLength, making the WebUSB Descriptor Set
  // Header inconsistent.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x03, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongDescriptorSet) {
  // The WebUSB Descriptor Set Header's wTotalLength is longer than the buffer.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x05, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortDescriptorInDescriptorSet) {
  // bLength for the descriptor within the descriptor set is too short.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x05, 0x00, 0x01};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongDescriptorInDescriptorSet) {
  // bLength for the descriptor within the descriptor set is longer than the
  // remaining portion of the buffer and the wTotalLength of the descriptor set.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x06, 0x00, 0x03, 0x03};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, InvalidDescriptorInDescriptorSet) {
  // A descriptor set cannot contain a descriptor with this bDescriptorType.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x06, 0x00, 0x02, 0x04};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, EmptyUrlInDescriptorSet) {
  // The URL in this descriptor set is the empty string.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x06, 0x00, 0x02, 0x03};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, InvalidUrlInDescriptorSet) {
  // The URL in this descriptor set is not a valid URL: "This is not a URL."
  static const uint8_t kBuffer[] = {
      0x04, 0x00, 0x18, 0x00, 0x14, 0x03, 'T', 'h', 'i', 's', ' ', 'i',
      's',  ' ',  'n',  'o',  't',  ' ',  'a', ' ', 'U', 'R', 'L', '.'};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortConfigurationSubsetHeader) {
  // bLength is too short for a WebUSB Configuration Subset Header.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x05, 0x00, 0x02, 0x01};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortConfigurationSubset) {
  // The configuration subset header's wTotalLength is shorter than its bLength,
  // making it inconsistent.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x09, 0x00, 0x05,
                                    0x01, 0x01, 0x04, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongConfigurationSubset) {
  // wTotalLength of the configuration subset header extends beyond wTotalLength
  // for the descriptor set and the length of the buffer.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x09, 0x00, 0x05,
                                    0x01, 0x01, 0x06, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortDescriptorInConfigurationSubset) {
  // bLength for the descriptor within the configuration subset is too short.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0A, 0x00, 0x05,
                                    0x01, 0x01, 0x06, 0x00, 0x01};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongDescriptorInConfigurationSubset) {
  // bLength for the descriptor within the configuration subset is longer than
  // the remaining portion of the buffer and the wTotalLength of the
  // configuration subset.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0B, 0x00, 0x05, 0x01,
                                    0x01, 0x07, 0x00, 0x03, 0x03};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, InvalidDescriptorInConfigurationSubset) {
  // A configuration subset cannot contain a descriptor with this
  // bDescriptorType.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0B, 0x00, 0x05, 0x01,
                                    0x01, 0x07, 0x00, 0x02, 0x01};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortFunctionSubsetHeader) {
  // bLength is too short for a WebUSB Function Subset Header.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0B, 0x00, 0x05, 0x01,
                                    0x01, 0x07, 0x00, 0x02, 0x02};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortFunctionSubset) {
  // The function subset header's wTotalLength is shorter than its bLength,
  // making it inconsistent.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0E, 0x00, 0x05, 0x01, 0x01,
                                    0x0A, 0x00, 0x05, 0x02, 0x01, 0x04, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongFunctionSubset) {
  // wTotalLength of the function subset header extends beyond wTotalLength for
  // for the configuration subset and the length of the buffer.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0E, 0x00, 0x05, 0x01, 0x01,
                                    0x0A, 0x00, 0x05, 0x02, 0x01, 0x06, 0x00};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortDescriptorInFunctionSubset) {
  // bLength for the descriptor within the function subset is too short.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x0F, 0x00, 0x05,
                                    0x01, 0x01, 0x0B, 0x00, 0x05,
                                    0x02, 0x01, 0x06, 0x00, 0x01};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongDescriptorInFunctionSubset) {
  // bLength for the descriptor within the function subset is longer than the
  // remaining portion of the buffer and the wTotalLength of the function
  // subset.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x10, 0x00, 0x05, 0x01,
                                    0x01, 0x0C, 0x00, 0x05, 0x02, 0x01,
                                    0x07, 0x00, 0x03, 0x03};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, InvalidDescriptorInFunctionSubset) {
  // A function subset cannot contain a descriptor with this bDescriptorType.
  static const uint8_t kBuffer[] = {0x04, 0x00, 0x10, 0x00, 0x05, 0x01,
                                    0x01, 0x0C, 0x00, 0x05, 0x02, 0x01,
                                    0x07, 0x00, 0x02, 0x02};
  WebUsbDescriptorSet descriptor;
  ASSERT_FALSE(descriptor.Parse(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, UrlDescriptor) {
  GURL url;
  ASSERT_TRUE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(
          kExampleUrlDescriptor,
          kExampleUrlDescriptor + sizeof(kExampleUrlDescriptor)),
      &url));
  EXPECT_EQ(GURL("https://example.com:80"), url);
}

TEST_F(WebUsbDescriptorsTest, ShortUrlDescriptorHeader) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, ShortUrlDescriptor) {
  // bLength is too short.
  static const uint8_t kBuffer[] = {0x01, 0x03};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, LongUrlDescriptor) {
  // bLength is too long.
  static const uint8_t kBuffer[] = {0x03, 0x03};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, EmptyUrl) {
  // The URL in this descriptor set is the empty string.
  static const uint8_t kBuffer[] = {0x02, 0x03};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, InvalidUrl) {
  // The URL in this descriptor set is not a valid URL: "This is not a URL."
  static const uint8_t kBuffer[] = {0x14, 0x03, 'T', 'h', 'i', 's', ' ',
                                    'i',  's',  ' ', 'n', 'o', 't', ' ',
                                    'a',  ' ',  'U', 'R', 'L', '.'};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

}  // namespace

}  // namespace device
