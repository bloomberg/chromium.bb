// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliated_match_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_service.h"

namespace password_manager {

namespace {

// Returns whether or not |form| represents a credential for an Android
// application, and if so, returns the |facet_uri| of that application.
bool IsAndroidApplicationCredential(const autofill::PasswordForm& form,
                                    FacetURI* facet_uri) {
  DCHECK(facet_uri);
  if (form.scheme != autofill::PasswordForm::SCHEME_HTML)
    return false;

  *facet_uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  return facet_uri->IsValidAndroidFacetURI();
}

}  // namespace

// static
const int64 AffiliatedMatchHelper::kInitializationDelayOnStartupInSeconds;

AffiliatedMatchHelper::AffiliatedMatchHelper(
    PasswordStore* password_store,
    scoped_ptr<AffiliationService> affiliation_service)
    : password_store_(password_store),
      task_runner_for_waiting_(base::ThreadTaskRunnerHandle::Get()),
      affiliation_service_(affiliation_service.Pass()),
      weak_ptr_factory_(this) {
  DCHECK(affiliation_service_);
  DCHECK(password_store_);
}

AffiliatedMatchHelper::~AffiliatedMatchHelper() {
  password_store_->RemoveObserver(this);
}

void AffiliatedMatchHelper::Initialize() {
  task_runner_for_waiting_->PostDelayedTask(
      FROM_HERE, base::Bind(&AffiliatedMatchHelper::DoDeferredInitialization,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitializationDelayOnStartupInSeconds));
}

void AffiliatedMatchHelper::GetAffiliatedAndroidRealms(
    const autofill::PasswordForm& observed_form,
    const AffiliatedRealmsCallback& result_callback) {
  FacetURI facet_uri(
      FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
  if (observed_form.scheme == autofill::PasswordForm::SCHEME_HTML &&
      observed_form.ssl_valid && facet_uri.IsValidWebFacetURI()) {
    affiliation_service_->GetAffiliations(
        facet_uri, true /* cached_only */,
        base::Bind(&AffiliatedMatchHelper::OnGetAffiliationsResults,
                   weak_ptr_factory_.GetWeakPtr(), facet_uri, result_callback));
  } else {
    result_callback.Run(std::vector<std::string>());
  }
}

// static
ScopedVector<autofill::PasswordForm>
AffiliatedMatchHelper::TransformAffiliatedAndroidCredentials(
    const autofill::PasswordForm& observed_form,
    ScopedVector<autofill::PasswordForm> android_credentials) {
  for (autofill::PasswordForm* form : android_credentials) {
    DCHECK_EQ(form->scheme, autofill::PasswordForm::SCHEME_HTML);
    form->original_signon_realm = form->signon_realm;
    form->signon_realm = observed_form.signon_realm;
  }
  return android_credentials.Pass();
}

void AffiliatedMatchHelper::SetTaskRunnerUsedForWaitingForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_for_waiting_ = task_runner;
}

void AffiliatedMatchHelper::DoDeferredInitialization() {
  // Must start observing for changes at the same time as when the snapshot is
  // taken to avoid inconsistencies due to any changes taking place in-between.
  password_store_->AddObserver(this);
  password_store_->GetAutofillableLogins(this);
}

void AffiliatedMatchHelper::OnGetAffiliationsResults(
    const FacetURI& original_facet_uri,
    const AffiliatedRealmsCallback& result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const FacetURI& affiliated_facet : results) {
      if (affiliated_facet != original_facet_uri &&
          affiliated_facet.IsValidAndroidFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.canonical_spec() + "/");
    }
  }
  result_callback.Run(affiliated_realms);
}

void AffiliatedMatchHelper::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri;
    if (!IsAndroidApplicationCredential(change.form(), &facet_uri))
      continue;

    if (change.type() == PasswordStoreChange::ADD)
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
    else if (change.type() == PasswordStoreChange::REMOVE)
      affiliation_service_->CancelPrefetch(facet_uri, base::Time::Max());
  }
}

void AffiliatedMatchHelper::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  for (autofill::PasswordForm* form : results) {
    FacetURI facet_uri;
    if (IsAndroidApplicationCredential(*form, &facet_uri))
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
  }
}

}  // namespace password_manager
