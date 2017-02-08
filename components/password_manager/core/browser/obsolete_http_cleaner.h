// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OBSOLETE_HTTP_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OBSOLETE_HTTP_CLEANER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class PasswordManagerClient;

// This class removes obsolete HTTP data from a password store. HTTP data is
// obsolete, if the corresponding host migrated to HTTPS and has HSTS enabled.
class ObsoleteHttpCleaner : public PasswordStoreConsumer {
 public:
  explicit ObsoleteHttpCleaner(const PasswordManagerClient* client);
  ~ObsoleteHttpCleaner() override;

  // PasswordStoreConsumer:
  // This will be called for both autofillable logins as well as blacklisted
  // logins. Blacklisted logins are removed iff the scheme is HTTP and HSTS is
  // enabled for the host.
  // Autofillable logins are removed iff the scheme is HTTP and there exists
  // another HTTPS login with active HSTS that has the same host as well as the
  // same username and password.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // This will remove all stats for HTTP sites for which HSTS is active.
  void OnGetSiteStatistics(std::vector<InteractionsStats> stats) override;

 private:
  const PasswordManagerClient* const client_;

  DISALLOW_COPY_AND_ASSIGN(ObsoleteHttpCleaner);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OBSOLETE_HTTP_CLEANER_H_
