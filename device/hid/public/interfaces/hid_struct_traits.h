// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_PUBLIC_INTERFACES_HID_STRUCT_TRAITS_H_
#define DEVICE_HID_PUBLIC_INTERFACES_HID_STRUCT_TRAITS_H_

#include <stddef.h>

#include "device/hid/hid_collection_info.h"
#include "device/hid/hid_usage_and_page.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "mojo/public/cpp/bindings/array_traits_stl.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::HidUsageAndPageDataView,
                    device::HidUsageAndPage> {
  static uint16_t usage(const device::HidUsageAndPage& r) { return r.usage; }
  static uint16_t usage_page(const device::HidUsageAndPage& r) {
    return r.usage_page;
  }
  static bool Read(device::mojom::HidUsageAndPageDataView data,
                   device::HidUsageAndPage* out);
};

template <>
struct StructTraits<device::mojom::HidCollectionInfoDataView,
                    device::HidCollectionInfo> {
  static const device::HidUsageAndPage& usage(
      const device::HidCollectionInfo& r) {
    return r.usage;
  }

  static const std::set<int>& report_ids(const device::HidCollectionInfo& r) {
    return r.report_ids;
  }

  static bool Read(device::mojom::HidCollectionInfoDataView data,
                   device::HidCollectionInfo* out);
};

}  // namespace mojo

#endif  // DEVICE_HID_PUBLIC_INTERFACES_HID_STRUCT_TRAITS_H_
