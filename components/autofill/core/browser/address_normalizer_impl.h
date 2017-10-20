// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill/core/browser/address_normalizer.h"

namespace i18n {
namespace addressinput {
class Source;
class Storage;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {

class AutofillProfile;

// A class used to normalize addresses.
class AddressNormalizerImpl : public AddressNormalizer {
 public:
  AddressNormalizerImpl(std::unique_ptr<::i18n::addressinput::Source> source,
                        std::unique_ptr<::i18n::addressinput::Storage> storage);
  ~AddressNormalizerImpl() override;

  // AddressNormalizer implementation.
  void LoadRulesForRegion(const std::string& region_code) override;
  bool AreRulesLoadedForRegion(const std::string& region_code) override;
  void NormalizeAddress(
      const AutofillProfile& profile,
      const std::string& region_code,
      int timeout_seconds,
      AddressNormalizer::NormalizationCallback callback) override;

 private:
  // Called when the validation rules for the |region_code| have finished
  // loading. Implementation of the LoadRulesListener interface.
  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override;

  // Map associating a region code to pending normalizations.
  class NormalizationRequest;
  std::map<std::string, std::vector<std::unique_ptr<NormalizationRequest>>>
      pending_normalization_;

  // The address validator used to normalize addresses.
  AddressValidator address_validator_;

  DISALLOW_COPY_AND_ASSIGN(AddressNormalizerImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_
