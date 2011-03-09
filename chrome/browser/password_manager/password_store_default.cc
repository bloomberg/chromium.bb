// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"

#include <vector>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

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

void PasswordStoreDefault::ReportMetricsImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  login_db_->ReportMetrics();
}

void PasswordStoreDefault::AddLoginImpl(const PasswordForm& form) {
  if (login_db_->AddLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
  }
}

void PasswordStoreDefault::UpdateLoginImpl(const PasswordForm& form) {
  if (login_db_->UpdateLogin(form, NULL)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
  }
}

void PasswordStoreDefault::RemoveLoginImpl(const PasswordForm& form) {
  if (login_db_->RemoveLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    NotificationService::current()->Notify(
        NotificationType::LOGINS_CHANGED,
        Source<PasswordStore>(this),
        Details<PasswordStoreChangeList>(&changes));
  }
}

void PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(
    const base::Time& delete_begin, const base::Time& delete_end) {
  std::vector<PasswordForm*> forms;
  if (login_db_->GetLoginsCreatedBetween(delete_begin, delete_end, &forms)) {
    if (login_db_->RemoveLoginsCreatedBetween(delete_begin, delete_end)) {
      PasswordStoreChangeList changes;
      for (std::vector<PasswordForm*>::const_iterator it = forms.begin();
           it != forms.end(); ++it) {
        changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                              **it));
      }
      NotificationService::current()->Notify(
          NotificationType::LOGINS_CHANGED,
          Source<PasswordStore>(this),
          Details<PasswordStoreChangeList>(&changes));
    }
  }
  STLDeleteElements(&forms);
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
  FillAutofillableLogins(&forms);
  NotifyConsumer(request, forms);
}

void PasswordStoreDefault::GetBlacklistLoginsImpl(
    GetLoginsRequest* request) {
  std::vector<PasswordForm*> forms;
  FillBlacklistLogins(&forms);
  NotifyConsumer(request, forms);
}

bool PasswordStoreDefault::FillAutofillableLogins(
         std::vector<PasswordForm*>* forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return login_db_->GetAutofillableLogins(forms);
}

bool PasswordStoreDefault::FillBlacklistLogins(
         std::vector<PasswordForm*>* forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return login_db_->GetBlacklistLogins(forms);
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
    const WDTypedResult* result) {
  DCHECK(handles_.end() != handles_.find(handle));

  handles_.erase(handle);
  if (!result)
    return;

  if (PASSWORD_RESULT != result->GetType()) {
    NOTREACHED();
    return;
  }

  const PasswordForms& forms =
      static_cast<const WDResult<PasswordForms>*>(result)->GetValue();
  for (PasswordForms::const_iterator it = forms.begin();
       it != forms.end(); ++it) {
    AddLogin(**it);
    web_data_service_->RemoveLogin(**it);
    delete *it;
  }
  if (handles_.empty()) {
    profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                              true);
  }
}
