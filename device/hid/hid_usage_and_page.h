// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_USAGE_AND_PAGE_H_
#define DEVICE_HID_HID_USAGE_AND_PAGE_H_

#include "base/basictypes.h"

namespace device {

struct HidUsageAndPage {
  enum Page {
    kPageUndefined = 0x00,
    kPageGenericDesktop = 0x01,
    kPageSimulation = 0x02,
    kPageVirtualReality = 0x03,
    kPageSport = 0x04,
    kPageGame = 0x05,
    kPageKeyboard = 0x07,
    kPageLed = 0x08,
    kPageButton = 0x09,
    kPageOrdinal = 0x0A,
    kPageTelephony = 0x0B,
    kPageConsumer = 0x0C,
    kPageDigitizer = 0x0D,
    kPagePidPage = 0x0F,
    kPageUnicode = 0x10,
    kPageAlphanumericDisplay = 0x14,
    kPageMedicalInstruments = 0x40,
    kPageMonitor0 = 0x80,
    kPageMonitor1 = 0x81,
    kPageMonitor2 = 0x82,
    kPageMonitor3 = 0x83,
    kPagePower0 = 0x84,
    kPagePower1 = 0x85,
    kPagePower2 = 0x86,
    kPagePower3 = 0x87,
    kPageBarCodeScanner = 0x8C,
    kPageScale = 0x8D,
    kPageMagneticStripeReader = 0x8E,
    kPageReservedPointOfSale = 0x8F,
    kPageCameraControl = 0x90,
    kPageArcade = 0x91,
    kPageVendor = 0xFF00,
    kPageMediaCenter = 0xFFBC
  };

  HidUsageAndPage(uint16_t usage, Page usage_page)
      : usage(usage), usage_page(usage_page) {}
  ~HidUsageAndPage() {}

  uint16_t usage;
  Page usage_page;

  bool operator==(const HidUsageAndPage& other) const;
};

}  // namespace device

#endif  // DEVICE_HID_HID_USAGE_AND_PAGE_H_
