// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_H_

#include <string>

#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

namespace autofill {

class AutofillProfile;

// A class used to normalize addresses.
class AddressNormalizer : public autofill::LoadRulesListener {
 public:
  // The interface for the normalization delegates.
  class Delegate {
   public:
    virtual void OnAddressNormalized(
        const AutofillProfile& normalized_profile) = 0;

    virtual void OnCouldNotNormalize(const AutofillProfile& profile) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // The interface for the normalization request.
  class Request {
   public:
    virtual void OnRulesLoaded(bool success) = 0;
    virtual ~Request() {}
  };

  // Start loading the validation rules for the specified |region_code|.
  virtual void LoadRulesForRegion(const std::string& region_code) = 0;

  // Returns whether the rules for the specified |region_code| have finished
  // loading.
  virtual bool AreRulesLoadedForRegion(const std::string& region_code) = 0;

  // Starts the normalization of the |profile| based on the |region_code|. The
  // normalized profile will be returned to the |requester| possibly
  // asynchronously. If the normalization is not completed in |timeout_seconds|
  // the requester will be informed and the request cancelled. This value should
  // be greater or equal to 0, for which it means that the normalization should
  // happen synchronously, or not at all if the rules are not already loaded.
  // Will start loading the rules for the |region_code| if they had not started
  // loading.
  virtual void StartAddressNormalization(const AutofillProfile& profile,
                                         const std::string& region_code,
                                         int timeout_seconds,
                                         Delegate* requester) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_H_
