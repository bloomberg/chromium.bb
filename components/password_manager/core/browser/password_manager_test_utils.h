// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"

// TODO(sync): This file must eventually be refactored away -- crbug.com/87185.

namespace password_manager {

// These constants are used by CreatePasswordFormFromDataForTesting to supply
// values not covered by PasswordFormData.
extern const char kTestingAvatarUrlSpec[];
extern const char kTestingFederationUrlSpec[];
extern const int kTestingDaysAfterPasswordsAreSynced;

// Struct used for creation of PasswordForms from static arrays of data.
// Note: This is only meant to be used in unit test.
struct PasswordFormData {
  const autofill::PasswordForm::Scheme scheme;
  const char* signon_realm;
  const char* origin;
  const char* action;
  const wchar_t* submit_element;
  const wchar_t* username_element;
  const wchar_t* password_element;
  const wchar_t* username_value;  // Set to NULL for a blacklist entry.
  const wchar_t* password_value;
  const bool preferred;
  const bool ssl_valid;
  const double creation_time;
};

// Creates and returns a new PasswordForm built from form_data.
scoped_ptr<autofill::PasswordForm> CreatePasswordFormFromDataForTesting(
    const PasswordFormData& form_data);

// Checks whether two vectors of PasswordForms contain equivalent elements,
// regardless of order.
// TODO(vabr) The *Ptr suffix is obsolete, rename.
bool ContainsSamePasswordFormsPtr(
    const std::vector<autofill::PasswordForm*>& first,
    const std::vector<autofill::PasswordForm*>& second);

// This gmock matcher is used to check that the |arg| contains exactly the same
// PasswordForms as |forms|, regardless of order.
MATCHER_P(ContainsSamePasswordForms, forms, "") {
  return ContainsSamePasswordFormsPtr(forms, arg);
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
