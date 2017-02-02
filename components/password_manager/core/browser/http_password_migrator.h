// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_MIGRATOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace autofill {
struct PasswordForm;
}

class GURL;

namespace password_manager {

class PasswordStore;

// The class is responsible for migrating the passwords saved on HTTP to HTTPS
// origin.
class HttpPasswordMigrator : public PasswordStoreConsumer {
 public:
  enum class MigrationMode {
    MOVE,  // HTTP credentials are deleted after migration to HTTPS.
    COPY,  // HTTP credentials are kept after migration to HTTPS.
  };

  // API to be implemented by an embedder of HttpPasswordMigrator.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Notify the embedder that |forms| were migrated to HTTPS. |forms| contain
    // the updated HTTPS scheme.
    virtual void ProcessMigratedForms(
        std::vector<std::unique_ptr<autofill::PasswordForm>> forms) = 0;
  };

  // |https_origin| should specify a valid HTTPS URL.
  HttpPasswordMigrator(const GURL& https_origin,
                       MigrationMode mode,
                       PasswordStore* password_store,
                       Consumer* consumer);
  ~HttpPasswordMigrator() override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  const MigrationMode mode_;
  Consumer* consumer_;
  PasswordStore* password_store_;

  DISALLOW_COPY_AND_ASSIGN(HttpPasswordMigrator);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_PASSWORD_MIGRATOR_H_
