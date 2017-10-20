// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_normalizer_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

namespace {

using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

}  // namespace

class AddressNormalizerImpl::NormalizationRequest {
 public:
  // |address_validator| needs to outlive this Request.
  NormalizationRequest(const AutofillProfile& profile,
                       const std::string& region_code,
                       int timeout_seconds,
                       AddressNormalizer::NormalizationCallback callback,
                       AddressValidator* address_validator)
      : profile_(profile),
        region_code_(region_code),
        callback_(std::move(callback)),
        address_validator_(address_validator),
        has_responded_(false),
        on_timeout_(base::Bind(&NormalizationRequest::OnRulesLoaded,
                               base::Unretained(this),
                               false)) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_timeout_.callback(),
        base::TimeDelta::FromSeconds(timeout_seconds));
  }

  ~NormalizationRequest() {}

  void OnRulesLoaded(bool success) {
    on_timeout_.Cancel();

    // Check if the timeout happened before the rules were loaded.
    if (has_responded_)
      return;
    has_responded_ = true;

    // In either case, format the phone number.
    FormatPhoneNumberForResponse();

    if (!success) {
      std::move(callback_).Run(/*success=*/false, profile_);
      return;
    }

    // The rules should be loaded.
    DCHECK(address_validator_->AreRulesLoadedForRegion(region_code_));

    // Create the AddressData from the profile.
    ::i18n::addressinput::AddressData address_data =
        *autofill::i18n::CreateAddressDataFromAutofillProfile(profile_,
                                                              region_code_);

    // Normalize the address.
    if (address_validator_ &&
        address_validator_->NormalizeAddress(&address_data)) {
      profile_.SetRawInfo(ADDRESS_HOME_STATE,
                          base::UTF8ToUTF16(address_data.administrative_area));
      profile_.SetRawInfo(ADDRESS_HOME_CITY,
                          base::UTF8ToUTF16(address_data.locality));
      profile_.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                          base::UTF8ToUTF16(address_data.dependent_locality));
    }

    std::move(callback_).Run(/*success=*/true, profile_);
  }

 private:
  // Tries to format the phone number to the E.164 format to send in the Payment
  // Response, as defined in the Payment Request spec. Keeps the original
  // if it cannot be formatted. More info at:
  // https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm
  void FormatPhoneNumberForResponse() {
    const std::string original_number = base::UTF16ToUTF8(
        profile_.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), region_code_));

    std::string formatted_number =
        autofill::i18n::FormatPhoneForResponse(original_number, region_code_);

    profile_.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                        base::UTF8ToUTF16(formatted_number));
  }

  AutofillProfile profile_;
  std::string region_code_;
  AddressNormalizer::NormalizationCallback callback_;
  AddressValidator* address_validator_;

  bool has_responded_;
  base::CancelableCallback<void()> on_timeout_;

  DISALLOW_COPY_AND_ASSIGN(NormalizationRequest);
};

AddressNormalizerImpl::AddressNormalizerImpl(std::unique_ptr<Source> source,
                                             std::unique_ptr<Storage> storage)
    : address_validator_(std::move(source), std::move(storage), this) {}

AddressNormalizerImpl::~AddressNormalizerImpl() {}

void AddressNormalizerImpl::LoadRulesForRegion(const std::string& region_code) {
  address_validator_.LoadRules(region_code);
}

bool AddressNormalizerImpl::AreRulesLoadedForRegion(
    const std::string& region_code) {
  return address_validator_.AreRulesLoadedForRegion(region_code);
}

void AddressNormalizerImpl::NormalizeAddress(
    const AutofillProfile& profile,
    const std::string& region_code,
    int timeout_seconds,
    AddressNormalizer::NormalizationCallback callback) {
  DCHECK_GE(timeout_seconds, 0);

  std::unique_ptr<NormalizationRequest> request =
      std::make_unique<NormalizationRequest>(
          profile, region_code, timeout_seconds, std::move(callback),
          &address_validator_);

  // If the rules are already loaded for |region_code|, the |request| will
  // callback synchronously.
  if (AreRulesLoadedForRegion(region_code)) {
    request->OnRulesLoaded(true);
    return;
  }

  // Setup the variables so the profile gets normalized when the rules have
  // finished loading.
  auto it = pending_normalization_.find(region_code);
  if (it == pending_normalization_.end()) {
    // If no entry exists yet, create the entry and assign it to |it|.
    it = pending_normalization_
             .insert(std::make_pair(
                 region_code,
                 std::vector<std::unique_ptr<NormalizationRequest>>()))
             .first;
  }

  it->second.push_back(std::move(request));

  // Start loading the rules for that region. If the rules were already in the
  // process of being loaded, this call will do nothing.
  LoadRulesForRegion(region_code);
}

void AddressNormalizerImpl::OnAddressValidationRulesLoaded(
    const std::string& region_code,
    bool success) {
  // Check if an address normalization is pending.
  auto it = pending_normalization_.find(region_code);
  if (it != pending_normalization_.end()) {
    for (size_t i = 0; i < it->second.size(); ++i) {
      it->second[i]->OnRulesLoaded(success);
    }
    pending_normalization_.erase(it);
  }
}

}  // namespace autofill
