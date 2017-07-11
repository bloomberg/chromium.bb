// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/address_normalization_manager.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"

namespace payments {

namespace {
constexpr int kAddressNormalizationTimeoutSeconds = 5;
}  // namespace

AddressNormalizationManager::AddressNormalizationManager(
    AddressNormalizer* address_normalizer,
    const std::string& default_country_code)
    : default_country_code_(default_country_code),
      address_normalizer_(address_normalizer) {
  DCHECK(autofill::data_util::IsValidCountryCode(default_country_code));
  DCHECK(address_normalizer_);

  // Start loading rules for the default country code. This happens
  // asynchronously, and will speed up normalization later if the rules for the
  // address' region have already been loaded.
  address_normalizer_->LoadRulesForRegion(default_country_code);
}

AddressNormalizationManager::~AddressNormalizationManager() {}

void AddressNormalizationManager::FinalizePendingRequestsWithCompletionCallback(
    base::OnceClosure completion_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!completion_callback_);
  completion_callback_ = std::move(completion_callback);
  accepting_requests_ = false;
  MaybeRunCompletionCallback();
}

void AddressNormalizationManager::StartNormalizingAddress(
    autofill::AutofillProfile* profile) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(accepting_requests_) << "FinalizeWithCompletionCallback has been "
                                 "called, cannot normalize more addresses";

  delegates_.push_back(
      base::MakeUnique<NormalizerDelegate>(this, address_normalizer_, profile));
}

void AddressNormalizationManager::MaybeRunCompletionCallback() {
  if (accepting_requests_ || !completion_callback_)
    return;

  for (const auto& delegate : delegates_) {
    if (!delegate->has_completed())
      return;
  }

  // We're no longer accepting requests, and all the delegates have completed.
  // Now's the time to run the completion callback.
  std::move(completion_callback_).Run();

  // Start accepting requests after the completion callback has run.
  delegates_.clear();
  accepting_requests_ = true;
}

AddressNormalizationManager::NormalizerDelegate::NormalizerDelegate(
    AddressNormalizationManager* owner,
    AddressNormalizer* address_normalizer,
    autofill::AutofillProfile* profile)
    : owner_(owner), profile_(profile) {
  DCHECK(owner_);
  DCHECK(profile_);

  std::string country_code =
      base::UTF16ToUTF8(profile_->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (!autofill::data_util::IsValidCountryCode(country_code))
    country_code = owner_->default_country_code_;

  address_normalizer->StartAddressNormalization(
      *profile_, country_code, kAddressNormalizationTimeoutSeconds, this);
}

void AddressNormalizationManager::NormalizerDelegate::OnAddressNormalized(
    const autofill::AutofillProfile& normalized_profile) {
  OnCompletion(normalized_profile);
}

void AddressNormalizationManager::NormalizerDelegate::OnCouldNotNormalize(
    const autofill::AutofillProfile& profile) {
  // Since the phone number is formatted in either case, this profile should
  // be used.
  OnCompletion(profile);
}

void AddressNormalizationManager::NormalizerDelegate::OnCompletion(
    const autofill::AutofillProfile& profile) {
  DCHECK(!has_completed_);
  has_completed_ = true;
  *profile_ = profile;
  owner_->MaybeRunCompletionCallback();
}

}  // namespace payments
