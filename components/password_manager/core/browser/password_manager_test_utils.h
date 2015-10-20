// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_

#include <iosfwd>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_service.h"
#include "testing/gmock/include/gmock/gmock.h"

// TODO(sync): The PasswordFormData code must eventually be refactored away --
// http://crbug.com/87185.

namespace password_manager {

// This templates allows creating methods with signature conforming to
// TestingFactoryFunction of the appropriate platform instance of
// KeyedServiceFactory. Context is the browser context prescribed by
// TestingFactoryFunction. Store is the PasswordStore version needed in the
// tests which use this method.
template <class Context, class Store>
scoped_ptr<KeyedService> BuildPasswordStoreService(Context* context) {
  scoped_refptr<password_manager::PasswordStore> store(new Store);
  if (!store->Init(syncer::SyncableService::StartSyncFlare()))
    return nullptr;
  return scoped_ptr<KeyedService>(new PasswordStoreService(store));
}

// These constants are used by CreatePasswordFormFromDataForTesting to supply
// values not covered by PasswordFormData.
extern const char kTestingIconUrlSpec[];
extern const char kTestingFederationUrlSpec[];
extern const int kTestingDaysAfterPasswordsAreSynced;

// Magic value for PasswordFormData::password_value to indicate a federated
// login.
extern const wchar_t kTestingFederatedLoginMarker[];

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

// Checks whether the PasswordForms pointed to in |actual_values| are in some
// permutation pairwise equal to those in |expectations|. Returns true in case
// of a perfect match; otherwise returns false and writes details of mismatches
// in human readable format to |mismatches_output| unless it is null.
bool ContainsEqualPasswordFormsUnordered(
    const std::vector<autofill::PasswordForm*>& expectations,
    const std::vector<autofill::PasswordForm*>& actual_values,
    std::ostream* mismatches_output);

MATCHER_P(UnorderedPasswordFormElementsAre, expectations, "") {
  return ContainsEqualPasswordFormsUnordered(expectations, arg,
                                             result_listener->stream());
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
