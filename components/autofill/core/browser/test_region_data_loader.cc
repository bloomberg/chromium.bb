// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_region_data_loader.h"

#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"

namespace autofill {

TestRegionDataLoader::TestRegionDataLoader() {}

TestRegionDataLoader::~TestRegionDataLoader() {}

void TestRegionDataLoader::LoadRegionData(
    const std::string& country_code,
    autofill::RegionDataLoader::RegionDataLoaded callback,
    int64_t unused_timeout_ms) {
  if (synchronous_callback_) {
    callback.Run(std::vector<const ::i18n::addressinput::RegionData*>());
  } else {
    country_code_ = country_code;
    callback_ = callback;
  }
}

void TestRegionDataLoader::ClearCallback() {
  callback_.Reset();
}

void TestRegionDataLoader::SendAsynchronousData(
    const std::vector<std::pair<std::string, std::string>>& regions) {
  // Can not be both synchronous and asynchronous.
  DCHECK(!synchronous_callback_);

  // Don't bother if the callback was cleared.
  if (callback_.is_null())
    return;

  ::i18n::addressinput::RegionData root_region("");
  for (const auto& region : regions) {
    root_region.AddSubRegion(region.first, region.second);
  }
  callback_.Run(root_region.sub_regions());
  callback_.Reset();
}

}  // namespace autofill
