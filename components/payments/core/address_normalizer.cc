// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/address_normalizer.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace payments {
namespace {

using ::autofill::AutofillProfile;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

class AddressNormalizationRequest : public AddressNormalizer::Request {
 public:
  // The |delegate| and |address_validator| need to outlive this Request.
  AddressNormalizationRequest(const AutofillProfile& profile,
                              const std::string& region_code,
                              int timeout_seconds,
                              AddressNormalizer::Delegate* delegate,
                              autofill::AddressValidator* address_validator)
      : profile_(profile),
        region_code_(region_code),
        delegate_(delegate),
        address_validator_(address_validator),
        has_responded_(false),
        on_timeout_(
            base::Bind(&::payments::AddressNormalizationRequest::OnRulesLoaded,
                       base::Unretained(this),
                       false)) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_timeout_.callback(),
        base::TimeDelta::FromSeconds(timeout_seconds));
  }

  ~AddressNormalizationRequest() override {}

  void OnRulesLoaded(bool success) override {
    on_timeout_.Cancel();

    // Check if the timeout happened before the rules were loaded.
    if (has_responded_)
      return;
    has_responded_ = true;

    if (!success) {
      delegate_->OnCouldNotNormalize(profile_);
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
      profile_.SetRawInfo(autofill::ADDRESS_HOME_STATE,
                          base::UTF8ToUTF16(address_data.administrative_area));
      profile_.SetRawInfo(autofill::ADDRESS_HOME_CITY,
                          base::UTF8ToUTF16(address_data.locality));
      profile_.SetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
                          base::UTF8ToUTF16(address_data.dependent_locality));
    }

    delegate_->OnAddressNormalized(profile_);
  }

 private:
  AutofillProfile profile_;
  std::string region_code_;
  AddressNormalizer::Delegate* delegate_;
  autofill::AddressValidator* address_validator_;

  bool has_responded_;
  base::CancelableCallback<void()> on_timeout_;

  DISALLOW_COPY_AND_ASSIGN(AddressNormalizationRequest);
};

}  // namespace

AddressNormalizer::AddressNormalizer(std::unique_ptr<Source> source,
                                     std::unique_ptr<Storage> storage)
    : address_validator_(std::move(source), std::move(storage), this) {}

AddressNormalizer::~AddressNormalizer() {}

void AddressNormalizer::LoadRulesForRegion(const std::string& region_code) {
  address_validator_.LoadRules(region_code);
}

bool AddressNormalizer::AreRulesLoadedForRegion(
    const std::string& region_code) {
  return address_validator_.AreRulesLoadedForRegion(region_code);
}

void AddressNormalizer::StartAddressNormalization(
    const AutofillProfile& profile,
    const std::string& region_code,
    int timeout_seconds,
    AddressNormalizer::Delegate* requester) {
  DCHECK(timeout_seconds >= 0);

  std::unique_ptr<AddressNormalizationRequest> request(
      base::MakeUnique<AddressNormalizationRequest>(profile, region_code,
                                                    timeout_seconds, requester,
                                                    &address_validator_));

  // Check if the rules are already loaded.
  if (AreRulesLoadedForRegion(region_code)) {
    request->OnRulesLoaded(true);
  } else {
    // Setup the variables so the profile gets normalized when the rules have
    // finished loading.
    auto it = pending_normalization_.find(region_code);
    if (it == pending_normalization_.end()) {
      // If no entry exists yet, create the entry and assign it to |it|.
      it = pending_normalization_
               .insert(std::make_pair(region_code,
                                      std::vector<std::unique_ptr<Request>>()))
               .first;
    }

    it->second.push_back(std::move(request));

    // Start loading the rules for that region. If the rules were already in the
    // process of being loaded, this call will do nothing.
    LoadRulesForRegion(region_code);
  }
}

void AddressNormalizer::OnAddressValidationRulesLoaded(
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

}  // namespace payments