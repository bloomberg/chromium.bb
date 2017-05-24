// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/subkey_requester.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
namespace payments {

namespace {

using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

class SubKeyRequest : public SubKeyRequester::Request {
 public:
  // The |delegate| and |address_validator| need to outlive this Request.
  SubKeyRequest(const std::string& region_code,
                int timeout_seconds,
                autofill::AddressValidator* address_validator,
                SubKeyReceiverCallback on_subkeys_received)
      : region_code_(region_code),
        address_validator_(address_validator),
        on_subkeys_received_(std::move(on_subkeys_received)),
        has_responded_(false),
        on_timeout_(base::Bind(&::payments::SubKeyRequest::OnRulesLoaded,
                               base::Unretained(this))) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_timeout_.callback(),
        base::TimeDelta::FromSeconds(timeout_seconds));
  }

  ~SubKeyRequest() override {}

  void OnRulesLoaded() override {
    on_timeout_.Cancel();
    // Check if the timeout happened before the rules were loaded.
    if (has_responded_)
      return;
    has_responded_ = true;

    std::move(on_subkeys_received_)
        .Run(address_validator_->GetRegionSubKeys(region_code_));
  }

 private:
  std::string region_code_;
  // Not owned. Never null. Outlive this object.
  autofill::AddressValidator* address_validator_;

  SubKeyReceiverCallback on_subkeys_received_;

  bool has_responded_;
  base::CancelableCallback<void()> on_timeout_;

  DISALLOW_COPY_AND_ASSIGN(SubKeyRequest);
};

}  // namespace

SubKeyRequester::SubKeyRequester(std::unique_ptr<Source> source,
                                 std::unique_ptr<Storage> storage)
    : address_validator_(std::move(source), std::move(storage), this) {}

SubKeyRequester::~SubKeyRequester() {}

void SubKeyRequester::StartRegionSubKeysRequest(const std::string& region_code,
                                                int timeout_seconds,
                                                SubKeyReceiverCallback cb) {
  DCHECK(timeout_seconds >= 0);

  std::unique_ptr<SubKeyRequest> request(base::MakeUnique<SubKeyRequest>(
      region_code, timeout_seconds, &address_validator_, std::move(cb)));

  if (AreRulesLoadedForRegion(region_code)) {
    request->OnRulesLoaded();
  } else {
    // Setup the variables so that the subkeys request is sent, when the rules
    // are loaded.
    pending_subkey_region_code_ = region_code;
    pending_subkey_request_ = std::move(request);

    // Start loading the rules for that region. If the rules were already in the
    // process of being loaded, this call will do nothing.
    LoadRulesForRegion(region_code);
  }
}

bool SubKeyRequester::AreRulesLoadedForRegion(const std::string& region_code) {
  return address_validator_.AreRulesLoadedForRegion(region_code);
}

void SubKeyRequester::LoadRulesForRegion(const std::string& region_code) {
  address_validator_.LoadRules(region_code);
}

void SubKeyRequester::OnAddressValidationRulesLoaded(
    const std::string& region_code,
    bool success) {
  // The case for |success| == false is already handled. if |success| == false,
  // AddressValidator::GetRegionSubKeys will return an empty list of subkeys.
  // Therefore, here, we can ignore the value of |success|.

  // Check if there is any subkey request for that region code.
  if (!pending_subkey_region_code_.compare(region_code))
    pending_subkey_request_->OnRulesLoaded();
  pending_subkey_region_code_.clear();
  pending_subkey_request_.reset();
}

void SubKeyRequester::CancelPendingGetSubKeys() {
  pending_subkey_region_code_.clear();
  pending_subkey_request_.reset();
}

}  // namespace payments
