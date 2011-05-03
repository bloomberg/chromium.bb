// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"

#include <set>

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

// MigrateHelper handles migration from WebDB to PasswordStore. It runs
// entirely on the UI thread and is owned by PasswordStoreDefault.
class PasswordStoreDefault::MigrateHelper : public WebDataServiceConsumer {
 public:
  MigrateHelper(Profile* profile,
                WebDataService* web_data_service,
                PasswordStore* password_store)
      : profile_(profile),
        web_data_service_(web_data_service),
        password_store_(password_store) {
  }
  ~MigrateHelper();

  void Init();

  // WebDataServiceConsumer:
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle handle,
      const WDTypedResult *result) OVERRIDE;

 private:
  typedef std::set<WebDataService::Handle> Handles;

  Profile* profile_;

  scoped_refptr<WebDataService> web_data_service_;

  // This creates a cycle between us and PasswordStore. The cycle is broken
  // from PasswordStoreDefault::Shutdown, which deletes us.
  scoped_refptr<PasswordStore> password_store_;

  // Set of handles from requesting data from the WebDB.
  Handles handles_;

  DISALLOW_COPY_AND_ASSIGN(MigrateHelper);
};

PasswordStoreDefault::MigrateHelper::~MigrateHelper() {
  for (Handles::const_iterator i = handles_.begin(); i != handles_.end(); ++i)
    web_data_service_->CancelRequest(*i);
  handles_.clear();
}

void PasswordStoreDefault::MigrateHelper::Init() {
  handles_.insert(web_data_service_->GetAutofillableLogins(this));
  handles_.insert(web_data_service_->GetBlacklistLogins(this));
}

void PasswordStoreDefault::MigrateHelper::OnWebDataServiceRequestDone(
    WebDataService::Handle handle,
    const WDTypedResult* result) {
  typedef std::vector<const PasswordForm*> PasswordForms;

  DCHECK(handles_.end() != handles_.find(handle));
  DCHECK(password_store_);

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
    password_store_->AddLogin(**it);
    web_data_service_->RemoveLogin(**it);
    delete *it;
  }
  if (handles_.empty()) {
    profile_->GetPrefs()->RegisterBooleanPref(prefs::kLoginDatabaseMigrated,
                                              true);
  }
}

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
  // MigrateHelper should always be NULL as Shutdown should be invoked before
  // the destructor.
  DCHECK(!migrate_helper_.get());
}

void PasswordStoreDefault::Shutdown() {
  migrate_helper_.reset();
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
  login_db_->GetLogins(form, &request->value);
  ForwardLoginsResult(request);
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

void PasswordStoreDefault::MigrateIfNecessary() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->FindPreference(prefs::kLoginDatabaseMigrated))
    return;
  DCHECK(!migrate_helper_.get());
  migrate_helper_.reset(new MigrateHelper(profile_, web_data_service_, this));
  migrate_helper_->Init();
}
