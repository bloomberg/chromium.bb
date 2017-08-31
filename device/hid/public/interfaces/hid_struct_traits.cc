// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/public/interfaces/hid_struct_traits.h"

namespace mojo {

// static
device::mojom::HidPage
EnumTraits<device::mojom::HidPage, device::HidUsageAndPage::Page>::ToMojom(
    device::HidUsageAndPage::Page input) {
  switch (input) {
    case device::HidUsageAndPage::Page::kPageUndefined:
      return device::mojom::HidPage::PageUndefined;
    case device::HidUsageAndPage::Page::kPageGenericDesktop:
      return device::mojom::HidPage::PageGenericDesktop;
    case device::HidUsageAndPage::Page::kPageSimulation:
      return device::mojom::HidPage::PageSimulation;
    case device::HidUsageAndPage::Page::kPageVirtualReality:
      return device::mojom::HidPage::PageVirtualReality;
    case device::HidUsageAndPage::Page::kPageSport:
      return device::mojom::HidPage::PageSport;
    case device::HidUsageAndPage::Page::kPageGame:
      return device::mojom::HidPage::PageGame;
    case device::HidUsageAndPage::Page::kPageKeyboard:
      return device::mojom::HidPage::PageKeyboard;
    case device::HidUsageAndPage::Page::kPageLed:
      return device::mojom::HidPage::PageLed;
    case device::HidUsageAndPage::Page::kPageButton:
      return device::mojom::HidPage::PageButton;
    case device::HidUsageAndPage::Page::kPageOrdinal:
      return device::mojom::HidPage::PageOrdinal;
    case device::HidUsageAndPage::Page::kPageTelephony:
      return device::mojom::HidPage::PageTelephony;
    case device::HidUsageAndPage::Page::kPageConsumer:
      return device::mojom::HidPage::PageConsumer;
    case device::HidUsageAndPage::Page::kPageDigitizer:
      return device::mojom::HidPage::PageDigitizer;
    case device::HidUsageAndPage::Page::kPagePidPage:
      return device::mojom::HidPage::PagePidPage;
    case device::HidUsageAndPage::Page::kPageUnicode:
      return device::mojom::HidPage::PageUnicode;
    case device::HidUsageAndPage::Page::kPageAlphanumericDisplay:
      return device::mojom::HidPage::PageAlphanumericDisplay;
    case device::HidUsageAndPage::Page::kPageMedicalInstruments:
      return device::mojom::HidPage::PageMedicalInstruments;
    case device::HidUsageAndPage::Page::kPageMonitor0:
      return device::mojom::HidPage::PageMonitor0;
    case device::HidUsageAndPage::Page::kPageMonitor1:
      return device::mojom::HidPage::PageMonitor1;
    case device::HidUsageAndPage::Page::kPageMonitor2:
      return device::mojom::HidPage::PageMonitor2;
    case device::HidUsageAndPage::Page::kPageMonitor3:
      return device::mojom::HidPage::PageMonitor3;
    case device::HidUsageAndPage::Page::kPagePower0:
      return device::mojom::HidPage::PagePower0;
    case device::HidUsageAndPage::Page::kPagePower1:
      return device::mojom::HidPage::PagePower1;
    case device::HidUsageAndPage::Page::kPagePower2:
      return device::mojom::HidPage::PagePower2;
    case device::HidUsageAndPage::Page::kPagePower3:
      return device::mojom::HidPage::PagePower3;
    case device::HidUsageAndPage::Page::kPageBarCodeScanner:
      return device::mojom::HidPage::PageBarCodeScanner;
    case device::HidUsageAndPage::Page::kPageScale:
      return device::mojom::HidPage::PageScale;
    case device::HidUsageAndPage::Page::kPageMagneticStripeReader:
      return device::mojom::HidPage::PageMagneticStripeReader;
    case device::HidUsageAndPage::Page::kPageReservedPointOfSale:
      return device::mojom::HidPage::PageReservedPointOfSale;
    case device::HidUsageAndPage::Page::kPageCameraControl:
      return device::mojom::HidPage::PageCameraControl;
    case device::HidUsageAndPage::Page::kPageArcade:
      return device::mojom::HidPage::PageArcade;
    case device::HidUsageAndPage::Page::kPageVendor:
      return device::mojom::HidPage::PageVendor;
    case device::HidUsageAndPage::Page::kPageMediaCenter:
      return device::mojom::HidPage::PageMediaCenter;
  }

  NOTREACHED();
  return device::mojom::HidPage::PageUndefined;
}

// static
bool EnumTraits<device::mojom::HidPage, device::HidUsageAndPage::Page>::
    FromMojom(device::mojom::HidPage input,
              device::HidUsageAndPage::Page* output) {
  switch (input) {
    case device::mojom::HidPage::PageUndefined:
      *output = device::HidUsageAndPage::Page::kPageUndefined;
      return true;
    case device::mojom::HidPage::PageGenericDesktop:
      *output = device::HidUsageAndPage::Page::kPageGenericDesktop;
      return true;
    case device::mojom::HidPage::PageSimulation:
      *output = device::HidUsageAndPage::Page::kPageSimulation;
      return true;
    case device::mojom::HidPage::PageVirtualReality:
      *output = device::HidUsageAndPage::Page::kPageVirtualReality;
      return true;
    case device::mojom::HidPage::PageSport:
      *output = device::HidUsageAndPage::Page::kPageSport;
      return true;
    case device::mojom::HidPage::PageGame:
      *output = device::HidUsageAndPage::Page::kPageGame;
      return true;
    case device::mojom::HidPage::PageKeyboard:
      *output = device::HidUsageAndPage::Page::kPageKeyboard;
      return true;
    case device::mojom::HidPage::PageLed:
      *output = device::HidUsageAndPage::Page::kPageLed;
      return true;
    case device::mojom::HidPage::PageButton:
      *output = device::HidUsageAndPage::Page::kPageButton;
      return true;
    case device::mojom::HidPage::PageOrdinal:
      *output = device::HidUsageAndPage::Page::kPageOrdinal;
      return true;
    case device::mojom::HidPage::PageTelephony:
      *output = device::HidUsageAndPage::Page::kPageTelephony;
      return true;
    case device::mojom::HidPage::PageConsumer:
      *output = device::HidUsageAndPage::Page::kPageConsumer;
      return true;
    case device::mojom::HidPage::PageDigitizer:
      *output = device::HidUsageAndPage::Page::kPageDigitizer;
      return true;
    case device::mojom::HidPage::PagePidPage:
      *output = device::HidUsageAndPage::Page::kPagePidPage;
      return true;
    case device::mojom::HidPage::PageUnicode:
      *output = device::HidUsageAndPage::Page::kPageUnicode;
      return true;
    case device::mojom::HidPage::PageAlphanumericDisplay:
      *output = device::HidUsageAndPage::Page::kPageAlphanumericDisplay;
      return true;
    case device::mojom::HidPage::PageMedicalInstruments:
      *output = device::HidUsageAndPage::Page::kPageMedicalInstruments;
      return true;
    case device::mojom::HidPage::PageMonitor0:
      *output = device::HidUsageAndPage::Page::kPageMonitor0;
      return true;
    case device::mojom::HidPage::PageMonitor1:
      *output = device::HidUsageAndPage::Page::kPageMonitor1;
      return true;
    case device::mojom::HidPage::PageMonitor2:
      *output = device::HidUsageAndPage::Page::kPageMonitor2;
      return true;
    case device::mojom::HidPage::PageMonitor3:
      *output = device::HidUsageAndPage::Page::kPageMonitor3;
      return true;
    case device::mojom::HidPage::PagePower0:
      *output = device::HidUsageAndPage::Page::kPagePower0;
      return true;
    case device::mojom::HidPage::PagePower1:
      *output = device::HidUsageAndPage::Page::kPagePower1;
      return true;
    case device::mojom::HidPage::PagePower2:
      *output = device::HidUsageAndPage::Page::kPagePower2;
      return true;
    case device::mojom::HidPage::PagePower3:
      *output = device::HidUsageAndPage::Page::kPagePower3;
      return true;
    case device::mojom::HidPage::PageBarCodeScanner:
      *output = device::HidUsageAndPage::Page::kPageBarCodeScanner;
      return true;
    case device::mojom::HidPage::PageScale:
      *output = device::HidUsageAndPage::Page::kPageScale;
      return true;
    case device::mojom::HidPage::PageMagneticStripeReader:
      *output = device::HidUsageAndPage::Page::kPageMagneticStripeReader;
      return true;
    case device::mojom::HidPage::PageReservedPointOfSale:
      *output = device::HidUsageAndPage::Page::kPageReservedPointOfSale;
      return true;
    case device::mojom::HidPage::PageCameraControl:
      *output = device::HidUsageAndPage::Page::kPageCameraControl;
      return true;
    case device::mojom::HidPage::PageArcade:
      *output = device::HidUsageAndPage::Page::kPageArcade;
      return true;
    case device::mojom::HidPage::PageVendor:
      *output = device::HidUsageAndPage::Page::kPageVendor;
      return true;
    case device::mojom::HidPage::PageMediaCenter:
      *output = device::HidUsageAndPage::Page::kPageMediaCenter;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<
    device::mojom::HidUsageAndPageDataView,
    device::HidUsageAndPage>::Read(device::mojom::HidUsageAndPageDataView data,
                                   device::HidUsageAndPage* out) {
  out->usage = data.usage();

  if (!data.ReadUsagePage(&out->usage_page))
    return false;

  return true;
}

// static
bool StructTraits<device::mojom::HidCollectionInfoDataView,
                  device::HidCollectionInfo>::
    Read(device::mojom::HidCollectionInfoDataView data,
         device::HidCollectionInfo* out) {
  if (!data.ReadUsage(&out->usage))
    return false;

  std::vector<int> vec;
  if (!data.ReadReportIds(&vec))
    return false;

  out->report_ids.clear();
  out->report_ids.insert(vec.begin(), vec.end());
  return true;
}

}  // namespace mojo
