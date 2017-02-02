// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_migrator.h"

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace password_manager {

HttpPasswordMigrator::HttpPasswordMigrator(const GURL& https_origin,
                                           MigrationMode mode,
                                           PasswordStore* password_store,
                                           Consumer* consumer)
    : mode_(mode), consumer_(consumer), password_store_(password_store) {
  DCHECK(password_store_);
  DCHECK(https_origin.is_valid());
  DCHECK(https_origin.SchemeIs(url::kHttpsScheme)) << https_origin;

  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  PasswordStore::FormDigest form(autofill::PasswordForm::SCHEME_HTML,
                                 http_origin.GetOrigin().spec(), http_origin);
  password_store_->GetLogins(form, this);
}

HttpPasswordMigrator::~HttpPasswordMigrator() = default;

void HttpPasswordMigrator::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // Android and PSL matches are ignored.
  results.erase(
      std::remove_if(results.begin(), results.end(),
                     [](const std::unique_ptr<autofill::PasswordForm>& form) {
                       return form->is_affiliation_based_match ||
                              form->is_public_suffix_match;
                     }),
      results.end());

  // Add the new credentials to the password store. The HTTP forms are
  // removed iff |mode_| == MigrationMode::MOVE.
  for (const auto& form : results) {
    autofill::PasswordForm new_form = *form;

    GURL::Replacements rep;
    rep.SetSchemeStr(url::kHttpsScheme);
    new_form.origin = form->origin.ReplaceComponents(rep);
    new_form.signon_realm = new_form.origin.spec();
    // If |action| is not HTTPS then it's most likely obsolete. Otherwise, it
    // may still be valid.
    if (!form->action.SchemeIs(url::kHttpsScheme))
      new_form.action = new_form.origin;
    new_form.form_data = autofill::FormData();
    new_form.generation_upload_status = autofill::PasswordForm::NO_SIGNAL_SENT;
    new_form.skip_zero_click = false;
    password_store_->AddLogin(new_form);

    if (mode_ == MigrationMode::MOVE)
      password_store_->RemoveLogin(*form);
    *form = std::move(new_form);
  }

  metrics_util::LogCountHttpMigratedPasswords(results.size());

  if (consumer_)
    consumer_->ProcessMigratedForms(std::move(results));
}

}  // namespace password_manager
