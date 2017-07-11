// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ADDRESS_NORMALIZATION_MANAGER_H_
#define COMPONENTS_PAYMENTS_CORE_ADDRESS_NORMALIZATION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "components/payments/core/address_normalizer.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

class AddressNormalizer;

// Class to handle multiple concurrent address normalization requests. This
// class is not thread-safe.
class AddressNormalizationManager {
 public:
  // Initializes an AddressNormalizationManager. |default_country_code| will be
  // used if the country code in an AutofillProfile to normalize is not valid.
  // The AddressNormalizationManager does not own |address_normalizer|.
  AddressNormalizationManager(AddressNormalizer* address_normalizer,
                              const std::string& default_country_code);

  ~AddressNormalizationManager();

  // Stops accepting normalization requests until all pending requests complete.
  // If all the address normalization requests have already completed,
  // |completion_callback| will be called before this method returns. Otherwise,
  // it will be called as soon as the last pending request completes.
  void FinalizePendingRequestsWithCompletionCallback(
      base::OnceClosure completion_callback);

  // Normalizes the address in |profile|. This may or may not happen
  // asynchronously. On completion, the address in |profile| will be updated
  // with the normalized address.
  void StartNormalizingAddress(autofill::AutofillProfile* profile);

 private:
  // Implements the payments::AddressNormalizer::Delegate interface, and
  // notifies its parent AddressNormalizationManager when normalization has
  // completed.
  class NormalizerDelegate : public AddressNormalizer::Delegate {
   public:
    // |owner| is the parent AddressNormalizationManager, |address_normalizer|
    // is a pointer to an instance of AddressNormalizer which will handle
    // normalization of |profile|. |profile| will be updated when normalization
    // is complete.
    NormalizerDelegate(AddressNormalizationManager* owner,
                       AddressNormalizer* address_normalizer,
                       autofill::AutofillProfile* profile);

    // Returns whether this delegate has completed or not.
    bool has_completed() const { return has_completed_; }

    // payments::AddressNormalizer::Delegate:
    void OnAddressNormalized(
        const autofill::AutofillProfile& normalized_profile) override;
    void OnCouldNotNormalize(const autofill::AutofillProfile& profile) override;

   private:
    // Helper method that handles when normalization has completed.
    void OnCompletion(const autofill::AutofillProfile& profile);

    bool has_completed_ = false;
    AddressNormalizationManager* owner_ = nullptr;
    autofill::AutofillProfile* profile_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(NormalizerDelegate);
  };

  friend class NormalizerDelegate;

  // Runs the completion callback if all the delegates have completed.
  void MaybeRunCompletionCallback();

  // Whether the AddressNormalizationManager is still accepting requests or not.
  bool accepting_requests_ = true;

  // The default country code to use if a profile does not have a valid country.
  const std::string default_country_code_;

  // The callback to execute when all addresses have been normalized.
  base::OnceClosure completion_callback_;

  // Storage for all the delegates that handle the normalization requests.
  std::vector<std::unique_ptr<NormalizerDelegate>> delegates_;

  // An unowned raw pointer to the AddressNormalizer to use.
  AddressNormalizer* address_normalizer_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(AddressNormalizationManager);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ADDRESS_NORMALIZATION_MANAGER_H_
