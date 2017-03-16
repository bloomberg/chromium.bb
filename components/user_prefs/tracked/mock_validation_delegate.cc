// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/mock_validation_delegate.h"

MockValidationDelegateRecord::MockValidationDelegateRecord() = default;

MockValidationDelegateRecord::~MockValidationDelegateRecord() = default;

size_t MockValidationDelegateRecord::CountValidationsOfState(
    PrefHashStoreTransaction::ValueState value_state) const {
  size_t count = 0;
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].value_state == value_state)
      ++count;
  }
  return count;
}

size_t MockValidationDelegateRecord::CountExternalValidationsOfState(
    PrefHashStoreTransaction::ValueState value_state) const {
  size_t count = 0;
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].external_validation_value_state == value_state)
      ++count;
  }
  return count;
}

const MockValidationDelegateRecord::ValidationEvent*
MockValidationDelegateRecord::GetEventForPath(
    const std::string& pref_path) const {
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].pref_path == pref_path)
      return &validations_[i];
  }
  return NULL;
}

void MockValidationDelegateRecord::RecordValidation(
    const std::string& pref_path,
    PrefHashStoreTransaction::ValueState value_state,
    PrefHashStoreTransaction::ValueState external_validation_value_state,
    bool is_personal,
    PrefHashFilter::PrefTrackingStrategy strategy) {
  validations_.push_back(ValidationEvent(pref_path, value_state,
                                         external_validation_value_state,
                                         is_personal, strategy));
}

MockValidationDelegate::MockValidationDelegate(
    scoped_refptr<MockValidationDelegateRecord> record)
    : record_(std::move(record)) {}

MockValidationDelegate::~MockValidationDelegate() = default;

void MockValidationDelegate::OnAtomicPreferenceValidation(
    const std::string& pref_path,
    std::unique_ptr<base::Value> value,
    PrefHashStoreTransaction::ValueState value_state,
    PrefHashStoreTransaction::ValueState external_validation_value_state,
    bool is_personal) {
  record_->RecordValidation(pref_path, value_state,
                            external_validation_value_state, is_personal,
                            PrefHashFilter::PrefTrackingStrategy::ATOMIC);
}

void MockValidationDelegate::OnSplitPreferenceValidation(
    const std::string& pref_path,
    const std::vector<std::string>& invalid_keys,
    const std::vector<std::string>& external_validation_invalid_keys,
    PrefHashStoreTransaction::ValueState value_state,
    PrefHashStoreTransaction::ValueState external_validation_value_state,
    bool is_personal) {
  record_->RecordValidation(pref_path, value_state,
                            external_validation_value_state, is_personal,
                            PrefHashFilter::PrefTrackingStrategy::SPLIT);
}
