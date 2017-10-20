// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_H_

#include <string>

#include "base/callback_forward.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

namespace autofill {

class AutofillProfile;

// A class used to normalize addresses.
class AddressNormalizer : public autofill::LoadRulesListener {
 public:
  using NormalizationCallback =
      base::OnceCallback<void(bool /*success*/,
                              const AutofillProfile& /*normalized_profile*/)>;

  // Start loading the validation rules for the specified |region_code|.
  virtual void LoadRulesForRegion(const std::string& region_code) = 0;

  // Returns whether the rules for the specified |region_code| have finished
  // loading.
  virtual bool AreRulesLoadedForRegion(const std::string& region_code) = 0;

  // Starts the normalization of the |profile| based on the |region_code|. The
  // normalized profile will be returned to the |requester| possibly
  // asynchronously. If the normalization is not completed in |timeout_seconds|
  // |callback| will be called with success=false. |timeout_seconds| should be
  // greater or equal to 0, for which it means that the normalization should
  // happen synchronously, or not at all if the rules are not already loaded.
  // Will start loading the rules for the |region_code| if they had not started
  // loading.
  virtual void NormalizeAddress(const AutofillProfile& profile,
                                const std::string& region_code,
                                int timeout_seconds,
                                NormalizationCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_H_
