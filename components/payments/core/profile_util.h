// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PROFILE_UTIL_H_
#define COMPONENTS_PAYMENTS_CONTENT_PROFILE_UTIL_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/autofill_profile_comparator.h"

// Utility functions used for processing and filtering address profiles
// (AutofillProfile).

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

class PaymentOptionsProvider;

namespace profile_util {

// Returns profiles for contact info, ordered by completeness and deduplicated.
// |profiles| should be passed in order of frecency, and this order will be
// preserved among equally-complete profiles. Deduplication here means that
// profiles returned are excluded if they are a subset of a more complete or
// more frecent profile. Completeness here refers only to the presence of the
// fields requested per the request_payer_* fields in |options|.
std::vector<autofill::AutofillProfile*> FilterProfilesForContact(
    const std::vector<autofill::AutofillProfile*>& profiles,
    const std::string& app_locale,
    const PaymentOptionsProvider& options);

// Helper class which evaluates profiles for similarity and completeness.
class PaymentsProfileComparator : public autofill::AutofillProfileComparator {
 public:
  PaymentsProfileComparator(const std::string& app_locale,
                            const PaymentOptionsProvider& options);
  ~PaymentsProfileComparator();

  // Returns true iff all of the contact info in |sub| also appears in |super|.
  // Only operates on fields requested in |options|.
  bool IsContactEqualOrSuperset(const autofill::AutofillProfile& super,
                                const autofill::AutofillProfile& sub);

  // Returns the number of contact fields requested in |options| which are
  // nonempty in |profile|.
  int GetContactCompletenessScore(const autofill::AutofillProfile* profile);

  // Returns true iff every contact field requested in |options| is nonempty in
  // |profile|.
  bool IsContactInfoComplete(const autofill::AutofillProfile* profile);

  // Comparison function suitable for sorting profiles by contact completeness
  // score with std::sort.
  bool IsContactMoreComplete(const autofill::AutofillProfile* p1,
                             const autofill::AutofillProfile* p2);

 private:
  const PaymentOptionsProvider& options_;
};

}  // namespace profile_util
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PROFILE_UTIL_H_