// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_CREDENTIALS_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_CREDENTIALS_CLEANER_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace password_manager {

class PasswordStore;

// This class is responsible for reporting metrics about HTTP to HTTPS
// migration.
class HttpCredentialCleaner : public PasswordStoreConsumer,
                              public CredentialsCleaner {
 public:
  // |network_context_getter| should return nullptr if it can't get the network
  // context because whatever owns it is dead.
  HttpCredentialCleaner(
      scoped_refptr<PasswordStore> store,
      base::RepeatingCallback<network::mojom::NetworkContext*()>
          network_context_getter);
  ~HttpCredentialCleaner() override;

  // CredentialsCleaner:
  void StartCleaning(Observer* observer) override;

 private:
  // This type define a subset of PasswordForm where first argument is the
  // signon-realm excluding the protocol, the second argument is
  // the PasswordForm::scheme (i.e. HTML, BASIC, etc.) and the third argument is
  // the username of the form.
  using FormKey =
      std::tuple<std::string, autofill::PasswordForm::Scheme, base::string16>;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  void OnHSTSQueryResult(FormKey key,
                         base::string16 password_value,
                         HSTSResult hsts_result);

  // After metrics are reported, this function will inform the |observer_| about
  // completion.
  void ReportMetrics();

  scoped_refptr<PasswordStore> store_;

  // Needed to create HSTS request.
  base::RepeatingCallback<network::mojom::NetworkContext*()>
      network_context_getter_;

  // Map from (signon-realm excluding the protocol, Password::Scheme, username)
  // tuples of HTTPS forms to a list of passwords for that pair.
  std::map<FormKey, base::flat_set<base::string16>> https_credentials_map_;

  // Used to signal completion of the clean-up. It is null until
  // StartCleaning is called.
  Observer* observer_ = nullptr;

  // The number of HTTP credentials processed after HSTS query results are
  // received.
  size_t processed_results_ = 0;

  // The next three counters are in pairs where [0] component means that HSTS is
  // not enabled and [1] component means that HSTS is enabled for that HTTP type
  // of credentials.

  // Number of HTTP credentials for which no HTTPS credential for the same
  // signon_realm excluding protocol, PasswordForm::scheme and username exists.
  size_t https_credential_not_found_[2] = {0, 0};

  // Number of HTTP credentials for which an equivalent (i.e. same signon_realm
  // excluding protocol, PasswordForm::scheme (i.e. HTML, BASIC, etc.), username
  // and password) HTTPS credential exists.
  size_t same_password_[2] = {0, 0};

  // Number of HTTP credentials for which a conflicting (i.e. same signon-realm
  // excluding the protocol, PasswordForm::Scheme and username but different
  // password) HTTPS credential exists.
  size_t different_password_[2] = {0, 0};

  // Number of HTTP credentials from the password store. Used to know when all
  // credentials were processed.
  size_t total_http_credentials_ = 0;

  DISALLOW_COPY_AND_ASSIGN(HttpCredentialCleaner);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_CREDENTIALS_CLEANER_H_