// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_test_utils.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

using autofill::PasswordForm;

namespace password_manager {

std::unique_ptr<PasswordForm> PasswordFormFromData(
    const PasswordFormData& form_data) {
  auto form = std::make_unique<PasswordForm>();
  form->scheme = form_data.scheme;
  form->preferred = form_data.preferred;
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->origin = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = base::WideToUTF16(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = base::WideToUTF16(form_data.username_element);
  if (form_data.password_element)
    form->password_element = base::WideToUTF16(form_data.password_element);
  if (form_data.username_value)
    form->username_value = base::WideToUTF16(form_data.username_value);
  if (form_data.password_value)
    form->password_value = base::WideToUTF16(form_data.password_value);
  return form;
}

std::unique_ptr<PasswordForm> FillPasswordFormWithData(
    const PasswordFormData& form_data,
    bool use_federated_login) {
  auto form = PasswordFormFromData(form_data);
  form->date_synced = form->date_created + base::TimeDelta::FromDays(1);
  if (form_data.username_value)
    form->display_name = form->username_value;
  else
    form->blacklisted_by_user = true;
  form->icon_url = GURL("https://accounts.google.com/Icon");
  if (use_federated_login) {
    form->password_value.clear();
    form->federation_origin =
        url::Origin::Create(GURL("https://accounts.google.com/login"));
  }
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> WrapForms(
    std::vector<PasswordForm> forms) {
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.reserve(forms.size());
  std::transform(forms.begin(), forms.end(), std::back_inserter(results),
                 [](PasswordForm& form) {
                   return std::make_unique<PasswordForm>(std::move(form));
                 });
  return results;
}

bool ContainsEqualPasswordFormsUnordered(
    const std::vector<std::unique_ptr<PasswordForm>>& expectations,
    const std::vector<std::unique_ptr<PasswordForm>>& actual_values,
    std::ostream* mismatch_output) {
  std::vector<PasswordForm*> remaining_expectations(expectations.size());
  std::transform(
      expectations.begin(), expectations.end(), remaining_expectations.begin(),
      [](const std::unique_ptr<PasswordForm>& form) { return form.get(); });

  bool had_mismatched_actual_form = false;
  for (const auto& actual : actual_values) {
    auto it_matching_expectation = std::find_if(
        remaining_expectations.begin(), remaining_expectations.end(),
        [&actual](const PasswordForm* expected) {
          return *expected == *actual;
        });
    if (it_matching_expectation != remaining_expectations.end()) {
      // Erase the matched expectation by moving the last element to its place.
      *it_matching_expectation = remaining_expectations.back();
      remaining_expectations.pop_back();
    } else {
      if (mismatch_output) {
        *mismatch_output << std::endl
                         << "Unmatched actual form:" << std::endl
                         << *actual;
      }
      had_mismatched_actual_form = true;
    }
  }

  if (mismatch_output) {
    for (const PasswordForm* remaining_expected_form : remaining_expectations) {
      *mismatch_output << std::endl
                       << "Unmatched expected form:" << std::endl
                       << *remaining_expected_form;
    }
  }

  return !had_mismatched_actual_form && remaining_expectations.empty();
}

MockPasswordStoreObserver::MockPasswordStoreObserver() {}

MockPasswordStoreObserver::~MockPasswordStoreObserver() {}

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
MockPasswordReuseDetectorConsumer::MockPasswordReuseDetectorConsumer() {}

MockPasswordReuseDetectorConsumer::~MockPasswordReuseDetectorConsumer() {}
#endif

HSTSStateManager::HSTSStateManager(net::TransportSecurityState* state,
                                   bool is_hsts,
                                   const std::string& host)
    : state_(state), is_hsts_(is_hsts), host_(host) {
  if (is_hsts_) {
    base::Time expiry = base::Time::Max();
    bool include_subdomains = false;
    state_->AddHSTS(host_, expiry, include_subdomains);
  }
}

HSTSStateManager::~HSTSStateManager() {
  if (is_hsts_)
    state_->DeleteDynamicDataForHost(host_);
}
}  // namespace password_manager
