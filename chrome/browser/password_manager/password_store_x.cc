// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_x.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

using std::vector;
using webkit_glue::PasswordForm;

PasswordStoreX::PasswordStoreX(LoginDatabase* login_db,
                               Profile* profile,
                               WebDataService* web_data_service,
                               NativeBackend* backend)
    : PasswordStoreDefault(login_db, profile, web_data_service),
      backend_(backend), migration_checked_(!backend), allow_fallback_(false) {
}

PasswordStoreX::~PasswordStoreX() {
}

void PasswordStoreX::AddLoginImpl(const PasswordForm& form) {
  CheckMigration();
  if (use_native_backend() && backend_->AddLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::AddLoginImpl(form);
  }
}

void PasswordStoreX::UpdateLoginImpl(const PasswordForm& form) {
  CheckMigration();
  if (use_native_backend() && backend_->UpdateLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::UpdateLoginImpl(form);
  }
}

void PasswordStoreX::RemoveLoginImpl(const PasswordForm& form) {
  CheckMigration();
  if (use_native_backend() && backend_->RemoveLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::RemoveLoginImpl(form);
  }
}

void PasswordStoreX::RemoveLoginsCreatedBetweenImpl(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  CheckMigration();
  vector<PasswordForm*> forms;
  if (use_native_backend() &&
      backend_->GetLoginsCreatedBetween(delete_begin, delete_end, &forms) &&
      backend_->RemoveLoginsCreatedBetween(delete_begin, delete_end)) {
    PasswordStoreChangeList changes;
    for (vector<PasswordForm*>::const_iterator it = forms.begin();
         it != forms.end(); ++it) {
      changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                            **it));
    }
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(delete_begin,
                                                         delete_end);
  }
  STLDeleteElements(&forms);
}

void PasswordStoreX::GetLoginsImpl(GetLoginsRequest* request,
                                     const PasswordForm& form) {
  CheckMigration();
  vector<PasswordForm*> forms;
  if (use_native_backend() && backend_->GetLogins(form, &forms)) {
    NotifyConsumer(request, forms);
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::GetLoginsImpl(request, form);
  } else {
    // The consumer will be left hanging unless we reply.
    NotifyConsumer(request, forms);
  }
}

void PasswordStoreX::GetAutofillableLoginsImpl(GetLoginsRequest* request) {
  CheckMigration();
  vector<PasswordForm*> forms;
  if (use_native_backend() && backend_->GetAutofillableLogins(&forms)) {
    NotifyConsumer(request, forms);
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::GetAutofillableLoginsImpl(request);
  } else {
    // The consumer will be left hanging unless we reply.
    NotifyConsumer(request, forms);
  }
}

void PasswordStoreX::GetBlacklistLoginsImpl(GetLoginsRequest* request) {
  CheckMigration();
  vector<PasswordForm*> forms;
  if (use_native_backend() && backend_->GetBlacklistLogins(&forms)) {
    NotifyConsumer(request, forms);
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    PasswordStoreDefault::GetBlacklistLoginsImpl(request);
  } else {
    // The consumer will be left hanging unless we reply.
    NotifyConsumer(request, forms);
  }
}

bool PasswordStoreX::FillAutofillableLogins(vector<PasswordForm*>* forms) {
  CheckMigration();
  if (use_native_backend() && backend_->GetAutofillableLogins(forms)) {
    allow_fallback_ = false;
    return true;
  }
  if (allow_default_store())
    return PasswordStoreDefault::FillAutofillableLogins(forms);
  return false;
}

bool PasswordStoreX::FillBlacklistLogins(vector<PasswordForm*>* forms) {
  CheckMigration();
  if (use_native_backend() && backend_->GetBlacklistLogins(forms)) {
    allow_fallback_ = false;
    return true;
  }
  if (allow_default_store())
    return PasswordStoreDefault::FillBlacklistLogins(forms);
  return false;
}

void PasswordStoreX::CheckMigration() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (migration_checked_ || !backend_.get())
    return;
  migration_checked_ = true;
  ssize_t migrated = MigrateLogins();
  if (migrated > 0) {
    VLOG(1) << "Migrated " << migrated << " passwords to native store.";
  } else if (migrated == 0) {
    // As long as we are able to migrate some passwords, we know the native
    // store is working. But if there is nothing to migrate, the "migration"
    // can succeed even when the native store would fail. In this case we
    // allow a later fallback to the default store. Once any later operation
    // succeeds on the native store, we will no longer allow it.
    allow_fallback_ = true;
  } else {
    LOG(WARNING) << "Native password store migration failed! " <<
                 "Falling back on default (unencrypted) store.";
    backend_.reset(NULL);
  }
}

bool PasswordStoreX::allow_default_store() {
  if (allow_fallback_) {
    LOG(WARNING) << "Native password store failed! " <<
                 "Falling back on default (unencrypted) store.";
    backend_.reset(NULL);
    // Don't warn again. We'll use the default store because backend_ is NULL.
    allow_fallback_ = false;
  }
  return !backend_.get();
}

ssize_t PasswordStoreX::MigrateLogins() {
  DCHECK(backend_.get());
  vector<PasswordForm*> forms;
  bool ok = PasswordStoreDefault::FillAutofillableLogins(&forms) &&
      PasswordStoreDefault::FillBlacklistLogins(&forms);
  if (ok) {
    // We add all the passwords (and blacklist entries) to the native backend
    // before attempting to remove any from the login database, to make sure we
    // don't somehow end up with some of the passwords in one store and some in
    // another. We'll always have at least one intact store this way.
    for (size_t i = 0; i < forms.size(); ++i) {
      if (!backend_->AddLogin(*forms[i])) {
        ok = false;
        break;
      }
    }
    if (ok) {
      for (size_t i = 0; i < forms.size(); ++i) {
        // If even one of these calls to RemoveLoginImpl() succeeds, then we
        // should prefer the native backend to the now-incomplete login
        // database. Thus we want to return a success status even in the case
        // where some fail. The only real problem with this is that we might
        // leave passwords in the login database and never come back to clean
        // them out if any of these calls do fail.
        PasswordStoreDefault::RemoveLoginImpl(*forms[i]);
      }
      // Finally, delete the database file itself. We remove the passwords from
      // it before deleting the file just in case there is some problem deleting
      // the file (e.g. directory is not writable, but file is), which would
      // otherwise cause passwords to re-migrate next (or maybe every) time.
      DeleteAndRecreateDatabaseFile();
    }
  }
  ssize_t result = ok ? forms.size() : -1;
  STLDeleteElements(&forms);
  return result;
}
