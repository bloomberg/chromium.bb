// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager.h"

#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/generated_resources.h"

using content::UserMetricsAction;
using content::WebContents;
using webkit::forms::PasswordForm;
using webkit::forms::PasswordFormMap;

namespace {

// This routine is called when PasswordManagers are constructed.
//
// Currently we report metrics only once at startup. We require
// that this is only ever called from a single thread in order to
// avoid needing to lock (a static boolean flag is then sufficient to
// guarantee running only once).
void ReportMetrics(bool password_manager_enabled) {
  static base::PlatformThreadId initial_thread_id =
      base::PlatformThread::CurrentId();
  DCHECK(initial_thread_id == base::PlatformThread::CurrentId());

  static bool ran_once = false;
  if (ran_once)
    return;
  ran_once = true;

  // TODO(isherman): This does not actually measure a user action.  It should be
  // a boolean histogram.
  if (password_manager_enabled)
    content::RecordAction(UserMetricsAction("PasswordManager_Enabled"));
  else
    content::RecordAction(UserMetricsAction("PasswordManager_Disabled"));
}

}  // anonymous namespace

// static
void PasswordManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPasswordManagerEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPasswordManagerAllowShowPasswords,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

PasswordManager::PasswordManager(WebContents* web_contents,
                                 PasswordManagerDelegate* delegate)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      observer_(NULL) {
  DCHECK(delegate_);
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
                                 delegate_->GetProfile()->GetPrefs(), NULL);

  ReportMetrics(*password_manager_enabled_);
}

PasswordManager::~PasswordManager() {
}

bool PasswordManager::IsSavingEnabled() const {
  return IsFillingEnabled() && !delegate_->GetProfile()->IsOffTheRecord();
}

void PasswordManager::ProvisionallySavePassword(const PasswordForm& form) {
  if (!IsSavingEnabled())
    return;

  // No password to save? Then don't.
  if (form.password_value.empty())
    return;

  scoped_ptr<PasswordFormManager> manager;
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    if ((*iter)->DoesManage(form)) {
      // Transfer ownership of the manager from |pending_login_managers_| to
      // |manager|.
      manager.reset(*iter);
      pending_login_managers_.weak_erase(iter);
      break;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (!manager.get())
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

  // Bail if we're missing any of the necessary form components.
  if (!manager->HasValidPasswordForm())
    return;

  PasswordForm provisionally_saved_form(form);
  provisionally_saved_form.ssl_valid = form.origin.SchemeIsSecure() &&
      !delegate_->DidLastPageLoadEncounterSSLErrors();
  provisionally_saved_form.preferred = true;
  manager->ProvisionallySave(provisionally_saved_form);
  provisional_save_manager_.swap(manager);
}

void PasswordManager::SetObserver(LoginModelObserver* observer) {
  observer_ = observer;
}

void PasswordManager::DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) {
  if (!params.password_form.origin.is_valid()) {
    // This codepath essentially means that a frame not containing a password
    // form was navigated.  For example, this might be a subframe of the main
    // page, navigated either automatically or in response to a user action.
    return;
  }

  // There might be password data to provisionally save. Other than that, we're
  // ready to reset and move on.
  ProvisionallySavePassword(params.password_form);
  pending_login_managers_.reset();
}

bool PasswordManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordManager, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_PasswordFormsParsed,
                        OnPasswordFormsParsed)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_PasswordFormsRendered,
                        OnPasswordFormsRendered)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordManager::OnPasswordFormsParsed(
    const std::vector<PasswordForm>& forms) {
  if (!IsFillingEnabled())
    return;

  // Ask the SSLManager for current security.
  bool had_ssl_error = delegate_->DidLastPageLoadEncounterSSLErrors();

  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
    PasswordFormManager* manager =
        new PasswordFormManager(delegate_->GetProfile(), this, *iter,
                                ssl_valid);
    pending_login_managers_.push_back(manager);
    manager->FetchMatchingLoginsFromPasswordStore();
  }
}

void PasswordManager::OnPasswordFormsRendered(
    const std::vector<PasswordForm>& visible_forms) {
  if (!provisional_save_manager_.get())
    return;

  DCHECK(IsSavingEnabled());

  // First, check for a failed login attempt.
  for (std::vector<PasswordForm>::const_iterator iter = visible_forms.begin();
       iter != visible_forms.end(); ++iter) {
    if (provisional_save_manager_->DoesManage(*iter)) {
      // The form trying to be saved has immediately re-appeared. Assume login
      // failure and abort this save, by clearing provisional_save_manager_.
      provisional_save_manager_->SubmitFailed();
      provisional_save_manager_.reset();
      return;
    }
  }

  if (!provisional_save_manager_->HasValidPasswordForm()) {
    // Form is not completely valid - we do not support it.
    NOTREACHED();
    provisional_save_manager_.reset();
    return;
  }

  // Looks like a successful login attempt. Either show an infobar or update
  // the previously saved login data.
  provisional_save_manager_->SubmitPassed();
  if (provisional_save_manager_->IsNewLogin()) {
    delegate_->AddSavePasswordInfoBarIfPermitted(
        provisional_save_manager_.release());
  } else {
    // If the save is not a new username entry, then we just want to save this
    // data (since the user already has related data saved), so don't prompt.
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
  }
}

void PasswordManager::Autofill(
    const PasswordForm& form_for_autofill,
    const PasswordFormMap& best_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) const {
  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observer_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      webkit::forms::PasswordFormFillData fill_data;
      webkit::forms::PasswordFormDomManager::InitFillData(form_for_autofill,
                                                          best_matches,
                                                          &preferred_match,
                                                          wait_for_username,
                                                          &fill_data);
      delegate_->FillPasswordForm(fill_data);
      return;
    }
    default:
      if (observer_) {
        observer_->OnAutofillDataAvailable(preferred_match.username_value,
                                           preferred_match.password_value);
      }
  }
}

bool PasswordManager::IsFillingEnabled() const {
  return delegate_->GetProfile() && *password_manager_enabled_;
}
