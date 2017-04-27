// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_TEST_ADDRESS_NORMALIZER_H_
#define COMPONENTS_PAYMENTS_CORE_TEST_ADDRESS_NORMALIZER_H_

#include "components/payments/core/address_normalizer.h"

#include "components/autofill/core/browser/autofill_profile.h"

namespace payments {

// A simpler version of the address normalizer to be used in tests. Can be set
// to normalize instantaneously or to wait for a call.
class TestAddressNormalizer : public AddressNormalizer {
 public:
  TestAddressNormalizer() {}

  void LoadRulesForRegion(const std::string& region_code) override {}

  bool AreRulesLoadedForRegion(const std::string& region_code) override;

  void StartAddressNormalization(
      const autofill::AutofillProfile& profile,
      const std::string& region_code,
      int timeout_seconds,
      AddressNormalizer::Delegate* requester) override;

  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override {}

  void DelayNormalization();

  void CompleteAddressNormalization();

 private:
  autofill::AutofillProfile profile_;
  AddressNormalizer::Delegate* requester_;

  bool instantaneous_normalization_ = true;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_TEST_ADDRESS_NORMALIZER_H_