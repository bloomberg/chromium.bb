// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/autofill/core/browser/test_address_normalizer.h"

namespace autofill {

TestAddressNormalizer::TestAddressNormalizer() {}
TestAddressNormalizer::~TestAddressNormalizer() {}

bool TestAddressNormalizer::AreRulesLoadedForRegion(
    const std::string& region_code) {
  return true;
}

void TestAddressNormalizer::NormalizeAddress(
    const AutofillProfile& profile,
    const std::string& region_code,
    int timeout_seconds,
    AddressNormalizer::NormalizationCallback callback) {
  if (instantaneous_normalization_) {
    std::move(callback).Run(/*success=*/true, profile);
    return;
  }

  // Setup the necessary variables for the delayed normalization.
  profile_ = profile;
  callback_ = std::move(callback);
}

void TestAddressNormalizer::DelayNormalization() {
  instantaneous_normalization_ = false;
}

void TestAddressNormalizer::CompleteAddressNormalization() {
  DCHECK(instantaneous_normalization_ == false);
  std::move(callback_).Run(/*success=*/true, profile_);
}

}  // namespace autofill
