// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/public/interfaces/hid_struct_traits.h"

namespace mojo {

// static
bool StructTraits<
    device::mojom::HidUsageAndPageDataView,
    device::HidUsageAndPage>::Read(device::mojom::HidUsageAndPageDataView data,
                                   device::HidUsageAndPage* out) {
  out->usage = data.usage();
  out->usage_page = data.usage_page();

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
