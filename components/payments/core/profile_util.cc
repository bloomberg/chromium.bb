// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/profile_util.h"

#include <algorithm>

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/payments/core/payment_options_provider.h"

namespace payments {
namespace profile_util {

std::vector<autofill::AutofillProfile*> FilterProfilesForContact(
    const std::vector<autofill::AutofillProfile*>& profiles,
    const std::string& app_locale,
    const PaymentOptionsProvider& options) {
  // We will be removing profiles, so we operate on a copy.
  std::vector<autofill::AutofillProfile*> processed = profiles;

  PaymentsProfileComparator comparator(app_locale, options);

  // Stable sort, since profiles are expected to be passed in frecency order.
  std::stable_sort(
      processed.begin(), processed.end(),
      std::bind(&PaymentsProfileComparator::IsContactMoreComplete, &comparator,
                std::placeholders::_1, std::placeholders::_2));

  auto it = processed.begin();
  while (it != processed.end()) {
    if (comparator.GetContactCompletenessScore(*it) == 0) {
      // Since profiles are sorted by completeness, this and any further
      // profiles can be discarded.
      processed.erase(it, processed.end());
      break;
    }

    // Attempt to find a matching element in the vector before the current.
    // This is quadratic, but the number of elements is generally small
    // (< 10), so a more complicated algorithm would be overkill.
    if (std::find_if(processed.begin(), it,
                     [&](autofill::AutofillProfile* prior) {
                       return comparator.IsContactEqualOrSuperset(*prior, **it);
                     }) != it) {
      // Remove the subset profile. |it| will point to the next element after
      // erasure.
      it = processed.erase(it);
    } else {
      it++;
    }
  }

  return processed;
}

PaymentsProfileComparator::PaymentsProfileComparator(
    const std::string& app_locale,
    const PaymentOptionsProvider& options)
    : autofill::AutofillProfileComparator(app_locale), options_(options) {}

PaymentsProfileComparator::~PaymentsProfileComparator() {}

bool PaymentsProfileComparator::IsContactEqualOrSuperset(
    const autofill::AutofillProfile& super,
    const autofill::AutofillProfile& sub) {
  if (options_.request_payer_name()) {
    if (sub.HasInfo(autofill::NAME_FULL) &&
        !super.HasInfo(autofill::NAME_FULL)) {
      return false;
    }
    if (!HaveMergeableNames(super, sub))
      return false;
  }
  if (options_.request_payer_phone()) {
    if (sub.HasInfo(autofill::PHONE_HOME_WHOLE_NUMBER) &&
        !super.HasInfo(autofill::PHONE_HOME_WHOLE_NUMBER)) {
      return false;
    }
    if (!HaveMergeablePhoneNumbers(super, sub))
      return false;
  }
  if (options_.request_payer_email()) {
    if (sub.HasInfo(autofill::EMAIL_ADDRESS) &&
        !super.HasInfo(autofill::EMAIL_ADDRESS)) {
      return false;
    }
    if (!HaveMergeableEmailAddresses(super, sub))
      return false;
  }
  return true;
}

int PaymentsProfileComparator::GetContactCompletenessScore(
    const autofill::AutofillProfile* profile) {
  if (!profile)
    return 0;

  return (options_.request_payer_name() &&
          profile->HasInfo(autofill::NAME_FULL)) +
         (options_.request_payer_phone() &&
          profile->HasInfo(autofill::PHONE_HOME_WHOLE_NUMBER)) +
         (options_.request_payer_email() &&
          profile->HasInfo(autofill::EMAIL_ADDRESS));
}

bool PaymentsProfileComparator::IsContactInfoComplete(
    const autofill::AutofillProfile* profile) {
  int desired_score = options_.request_payer_name() +
                      options_.request_payer_phone() +
                      options_.request_payer_email();
  return GetContactCompletenessScore(profile) == desired_score;
}

bool PaymentsProfileComparator::IsContactMoreComplete(
    const autofill::AutofillProfile* p1,
    const autofill::AutofillProfile* p2) {
  return GetContactCompletenessScore(p1) > GetContactCompletenessScore(p2);
}

}  // namespace profile_util
}  // namespace payments
