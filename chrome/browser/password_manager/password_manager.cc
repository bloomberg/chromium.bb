// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager.h"

#include <vector>

#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/generated_resources.h"

using webkit_glue::PasswordForm;
using webkit_glue::PasswordFormMap;

// static
void PasswordManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPasswordManagerEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPasswordManagerAllowShowPasswords,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

// This routine is called when PasswordManagers are constructed.
//
// Currently we report metrics only once at startup. We require
// that this is only ever called from a single thread in order to
// avoid needing to lock (a static boolean flag is then sufficient to
// guarantee running only once).
static void ReportMetrics(bool password_manager_enabled) {
  static base::PlatformThreadId initial_thread_id =
      base::PlatformThread::CurrentId();
  DCHECK(initial_thread_id == base::PlatformThread::CurrentId());

  static bool ran_once = false;
  if (ran_once)
    return;
  ran_once = true;

  if (password_manager_enabled)
    UserMetrics::RecordAction(UserMetricsAction("PasswordManager_Enabled"));
  else
    UserMetrics::RecordAction(UserMetricsAction("PasswordManager_Disabled"));
}

PasswordManager::PasswordManager(TabContents* tab_contents,
                                 PasswordManagerDelegate* delegate)
    : TabContentsObserver(tab_contents),
      login_managers_deleter_(&pending_login_managers_),
      delegate_(delegate),
      observer_(NULL) {
  DCHECK(delegate_);
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
      delegate_->GetProfileForPasswordManager()->GetPrefs(), NULL);

  ReportMetrics(*password_manager_enabled_);
}

PasswordManager::~PasswordManager() {
}

void PasswordManager::ProvisionallySavePassword(PasswordForm form) {
  if (!delegate_->GetProfileForPasswordManager() ||
      delegate_->GetProfileForPasswordManager()->IsOffTheRecord() ||
      !*password_manager_enabled_)
    return;

  // No password to save? Then don't.
  if (form.password_value.empty())
    return;

  LoginManagers::iterator iter;
  PasswordFormManager* manager = NULL;
  for (iter = pending_login_managers_.begin();
       iter != pending_login_managers_.end(); iter++) {
    if ((*iter)->DoesManage(form)) {
      manager = *iter;
      break;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (!manager)
    return;

  // If we found a manager but it didn't finish matching yet, the user has
  // tried to submit credentials before we had time to even find matching
  // results for the given form and autofill. If this is the case, we just
  // give up.
  if (!manager->HasCompletedMatching())
    return;

  // Also get out of here if the user told us to 'never remember' passwords for
  // this form.
  if (manager->IsBlacklisted())
    return;

  form.ssl_valid = form.origin.SchemeIsSecure() &&
      !delegate_->DidLastPageLoadEncounterSSLErrors();
  form.preferred = true;
  manager->ProvisionallySave(form);
  provisional_save_manager_.reset(manager);
  pending_login_managers_.erase(iter);
  // We don't care about the rest of the forms on the page now that one
  // was selected.
  STLDeleteElements(&pending_login_managers_);
}

void PasswordManager::DidNavigate() {
  // As long as this navigation isn't due to a currently pending
  // password form submit, we're ready to reset and move on.
  if (!provisional_save_manager_.get() && !pending_login_managers_.empty())
    STLDeleteElements(&pending_login_managers_);
}

void PasswordManager::ClearProvisionalSave() {
  provisional_save_manager_.reset();
}

void PasswordManager::SetObserver(LoginModelObserver* observer) {
  observer_ = observer;
}

void PasswordManager::DidStopLoading() {
  if (!provisional_save_manager_.get())
    return;

  DCHECK(!delegate_->GetProfileForPasswordManager()->IsOffTheRecord());
  DCHECK(!provisional_save_manager_->IsBlacklisted());

  if (!delegate_->GetProfileForPasswordManager())
    return;
  // Form is not completely valid - we do not support it.
  if (!provisional_save_manager_->HasValidPasswordForm())
    return;

  provisional_save_manager_->SubmitPassed();
  if (provisional_save_manager_->IsNewLogin()) {
    delegate_->AddSavePasswordInfoBar(provisional_save_manager_.release());
  } else {
    // If the save is not a new username entry, then we just want to save this
    // data (since the user already has related data saved), so don't prompt.
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
  }
}

void PasswordManager::DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) {
  if (params.password_form.origin.is_valid())
    ProvisionallySavePassword(params.password_form);
}

bool PasswordManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordManager, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_PasswordFormsFound,
                        OnPasswordFormsFound)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_PasswordFormsVisible,
                        OnPasswordFormsVisible)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordManager::OnPasswordFormsFound(
    const std::vector<PasswordForm>& forms) {
  if (!delegate_->GetProfileForPasswordManager())
    return;
  if (!*password_manager_enabled_)
    return;

  // Ask the SSLManager for current security.
  bool had_ssl_error = delegate_->DidLastPageLoadEncounterSSLErrors();

  std::vector<PasswordForm>::const_iterator iter;
  for (iter = forms.begin(); iter != forms.end(); iter++) {
    bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
    PasswordFormManager* manager =
        new PasswordFormManager(delegate_->GetProfileForPasswordManager(),
                                this, *iter, ssl_valid);
    pending_login_managers_.push_back(manager);
    manager->FetchMatchingLoginsFromPasswordStore();
  }
}

void PasswordManager::OnPasswordFormsVisible(
    const std::vector<PasswordForm>& visible_forms) {
  if (!provisional_save_manager_.get())
    return;
  std::vector<PasswordForm>::const_iterator iter;
  for (iter = visible_forms.begin(); iter != visible_forms.end(); iter++) {
    if (provisional_save_manager_->DoesManage(*iter)) {
      // The form trying to be saved has immediately re-appeared. Assume login
      // failure and abort this save, by clearing provisional_save_manager_.
      // Don't delete the login managers since the user may try again
      // and we want to be able to save in that case.
      provisional_save_manager_->SubmitFailed();
      ClearProvisionalSave();
      break;
    }
  }
}

void PasswordManager::Autofill(
    const PasswordForm& form_for_autofill,
    const PasswordFormMap& best_matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username) const {
  DCHECK(preferred_match);
  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observer_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      webkit_glue::PasswordFormFillData fill_data;
      webkit_glue::PasswordFormDomManager::InitFillData(form_for_autofill,
                                                        best_matches,
                                                        preferred_match,
                                                        wait_for_username,
                                                        &fill_data);
      delegate_->FillPasswordForm(fill_data);
      return;
    }
    default:
      if (observer_) {
        observer_->OnAutofillDataAvailable(preferred_match->username_value,
                                           preferred_match->password_value);
      }
  }
}
