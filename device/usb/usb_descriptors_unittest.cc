// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "device/usb/usb_descriptors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class UsbDescriptorsTest : public ::testing::Test {};

TEST_F(UsbDescriptorsTest, StringDescriptor) {
  static const uint8_t kBuffer[] = {0x1a, 0x03, 'H', 0, 'e', 0, 'l', 0, 'l', 0,
                                    'o',  0,    ' ', 0, 'w', 0, 'o', 0, 'r', 0,
                                    'l',  0,    'd', 0, '!', 0};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::ASCIIToUTF16("Hello world!"), string);
}

TEST_F(UsbDescriptorsTest, ShortStringDescriptorHeader) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01};
  base::string16 string;
  ASSERT_FALSE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
}

TEST_F(UsbDescriptorsTest, ShortStringDescriptor) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01, 0x03};
  base::string16 string;
  ASSERT_FALSE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
}

TEST_F(UsbDescriptorsTest, OddLengthStringDescriptor) {
  // There's an extra byte at the end of the string.
  static const uint8_t kBuffer[] = {0x0d, 0x03, 'H', 0,   'e', 0,  'l',
                                    0,    'l',  0,   'o', 0,   '!'};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::ASCIIToUTF16("Hello"), string);
}

TEST_F(UsbDescriptorsTest, EmptyStringDescriptor) {
  // The string is empty.
  static const uint8_t kBuffer[] = {0x02, 0x03};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::string16(), string);
}

TEST_F(UsbDescriptorsTest, OneByteStringDescriptor) {
  // The string is only one byte.
  static const uint8_t kBuffer[] = {0x03, 0x03, '?'};
  base::string16 string;
  ASSERT_TRUE(ParseUsbStringDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &string));
  EXPECT_EQ(base::string16(), string);
}

}  // namespace

}  // namespace device
