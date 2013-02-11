// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"

#include <set>

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;
using content::PasswordForm;

PasswordStoreDefault::PasswordStoreDefault(LoginDatabase* login_db,
                                           Profile* profile)
    : login_db_(login_db), profile_(profile) {
  DCHECK(login_db);
  DCHECK(profile);
}

PasswordStoreDefault::~PasswordStoreDefault() {
}

void PasswordStoreDefault::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = NULL;
}

void PasswordStoreDefault::ReportMetricsImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  login_db_->ReportMetrics();
}

void PasswordStoreDefault::AddLoginImpl(const PasswordForm& form) {
  if (login_db_->AddLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGINS_CHANGED,
        content::Source<PasswordStore>(this),
        content::Details<PasswordStoreChangeList>(&changes));
  }
}

void PasswordStoreDefault::UpdateLoginImpl(const PasswordForm& form) {
  if (login_db_->UpdateLogin(form, NULL)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGINS_CHANGED,
        content::Source<PasswordStore>(this),
        content::Details<PasswordStoreChangeList>(&changes));
  }
}

void PasswordStoreDefault::RemoveLoginImpl(const PasswordForm& form) {
  if (login_db_->RemoveLogin(form)) {
    PasswordStoreChangeList changes;
    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGINS_CHANGED,
        content::Source<PasswordStore>(this),
        content::Details<PasswordStoreChangeList>(&changes));
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
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_LOGINS_CHANGED,
          content::Source<PasswordStore>(this),
          content::Details<PasswordStoreChangeList>(&changes));
    }
  }
  STLDeleteElements(&forms);
}

void PasswordStoreDefault::GetLoginsImpl(
    const content::PasswordForm& form,
    const ConsumerCallbackRunner& callback_runner) {
  std::vector<PasswordForm*> matched_forms;
  login_db_->GetLogins(form, &matched_forms);
  callback_runner.Run(matched_forms);
}

void PasswordStoreDefault::GetAutofillableLoginsImpl(
    GetLoginsRequest* request) {
  FillAutofillableLogins(&request->value);
  ForwardLoginsResult(request);
}

void PasswordStoreDefault::GetBlacklistLoginsImpl(
    GetLoginsRequest* request) {
  FillBlacklistLogins(&request->value);
  ForwardLoginsResult(request);
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
