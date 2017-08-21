// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "net/http/transport_security_state.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// This template allows creating methods with signature conforming to
// TestingFactoryFunction of the appropriate platform instance of
// KeyedServiceFactory. Context is the browser context prescribed by
// TestingFactoryFunction. Store is the PasswordStore version needed in the
// tests which use this method.
template <class Context, class Store>
scoped_refptr<RefcountedKeyedService> BuildPasswordStore(Context* context) {
  scoped_refptr<password_manager::PasswordStore> store(new Store);
  if (!store->Init(syncer::SyncableService::StartSyncFlare(), nullptr))
    return nullptr;
  return store;
}

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
  const double creation_time;
};

// Creates and returns a new PasswordForm built from |form_data|.
std::unique_ptr<autofill::PasswordForm> PasswordFormFromData(
    const PasswordFormData& form_data);

// Like PasswordFormFromData(), but also fills arbitrary values into fields not
// specified by |form_data|.  This may be useful e.g. for tests looking to
// verify the handling of these fields.  If |use_federated_login| is true, this
// function will set the form's |federation_origin|.
std::unique_ptr<autofill::PasswordForm> FillPasswordFormWithData(
    const PasswordFormData& form_data,
    bool use_federated_login = false);

// Convenience method that wraps the passed in forms in unique ptrs and returns
// the result.
std::vector<std::unique_ptr<autofill::PasswordForm>> WrapForms(
    std::vector<autofill::PasswordForm> forms);

// Checks whether the PasswordForms pointed to in |actual_values| are in some
// permutation pairwise equal to those in |expectations|. Returns true in case
// of a perfect match; otherwise returns false and writes details of mismatches
// in human readable format to |mismatch_output| unless it is null.
// Note: |expectations| should be a const ref, but needs to be a const pointer,
// because GMock tried to copy the reference by value.
bool ContainsEqualPasswordFormsUnordered(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& expectations,
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& actual_values,
    std::ostream* mismatch_output);

MATCHER_P(UnorderedPasswordFormElementsAre, expectations, "") {
  return ContainsEqualPasswordFormsUnordered(*expectations, arg,
                                             result_listener->stream());
}

class MockPasswordStoreObserver : public PasswordStore::Observer {
 public:
  MockPasswordStoreObserver();
  ~MockPasswordStoreObserver() override;

  MOCK_METHOD1(OnLoginsChanged, void(const PasswordStoreChangeList& changes));
};

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
class MockPasswordReuseDetectorConsumer : public PasswordReuseDetectorConsumer {
 public:
  MockPasswordReuseDetectorConsumer();
  ~MockPasswordReuseDetectorConsumer() override;

  MOCK_METHOD4(
      OnReuseFound,
      void(const base::string16&, bool, const std::vector<std::string>&, int));
};
#endif

// Auxiliary class to automatically set and reset the HSTS state for a given
// host.
class HSTSStateManager {
 public:
  HSTSStateManager(net::TransportSecurityState* state,
                   bool is_hsts,
                   const std::string& host);
  ~HSTSStateManager();

 private:
  net::TransportSecurityState* state_;
  const bool is_hsts_;
  const std::string host_;

  DISALLOW_COPY_AND_ASSIGN(HSTSStateManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
