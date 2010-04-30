// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"

#include "base/logging.h"
#include "base/task.h"

using webkit_glue::PasswordForm;

PasswordStoreDefault::PasswordStoreDefault(LoginDatabase* login_db,
                                           Profile* profile,
                                           WebDataService* web_data_service)
    : web_data_service_(web_data_service),
      login_db_(login_db), profile_(profile) {
  DCHECK(login_db);
  DCHECK(profile);
  DCHECK(web_data_service);
  MigrateIfNecessary();
}

PasswordStoreDefault::~PasswordStoreDefault() {
}

void PasswordStoreDefault::AddLoginImpl(const PasswordForm& form) {
  login_db_->AddLogin(form);
}

void PasswordStoreDefault::UpdateLoginImpl(const PasswordForm& form) {
  login_db_->UpdateLogin(form, NULL);
}

void PasswordStoreDefault::RemoveLoginImpl(const PasswordForm& form) {
  login_db_->RemoveLogin(form);
}

void PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(
    const base::Time& delete_begin, const base::Time& delete_end) {
  login_db_->RemoveLoginsCreatedBetween(delete_begin, delete_end);
}

void PasswordStoreDefault::GetLoginsImpl(
    GetLoginsRequest* request, const webkit_glue::PasswordForm& form) {
  std::vector<PasswordForm*> forms;
  login_db_->GetLogins(form, &forms);
  NotifyConsumer(request, forms);
}

void PasswordStoreDefault::GetAutofillableLoginsImpl(
    GetLoginsRequest* request) {
  std::vector<PasswordForm*> forms;
  login_db_->GetAutofillableLogins(&forms);
  NotifyConsumer(request, forms);
}

void PasswordStoreDefault::GetBlacklistLoginsImpl(
    GetLoginsRequest* request) {
  std::vector<PasswordForm*> forms;
  login_db_->GetBlacklistLogins(&forms);
  NotifyConsumer(request, forms);
}

void PasswordStoreDefault::MigrateIfNecessary() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->FindPreference(prefs::kLoginDatabaseMigrated))
    return;
  handles_.insert(web_data_service_->GetAutofillableLogins(this));
  handles_.insert(web_data_service_->GetBlacklistLogins(this));
}

typedef std::vector<const PasswordForm*> PasswordForms;

void PasswordStoreDefault::OnWebDataServiceRequestDone(
    WebDataService::Handle handle,
    const WDTypedResult *result) {
  DCHECK(handles_.end() != handles_.find(handle));
  DCHECK(result);

  handles_.erase(handle);
  if (!result)
    return;

  const PasswordForms& forms =
      static_cast<const WDResult<PasswordForms>*>(result)->GetValue();
  for (PasswordForms::const_iterator it = forms.begin();
       it != forms.end(); ++it) {
    AddLoginImpl(**it);
    web_data_service_->RemoveLogin(**it);
    delete *it;
  }
  if (handles_.empty()) {
    profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                              true);
  }
}
