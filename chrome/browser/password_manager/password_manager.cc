// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager.h"

#include "base/metrics/histogram.h"
#include "base/threading/platform_thread.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/generated_resources.h"

using content::UserMetricsAction;
using content::WebContents;
using content::PasswordForm;
using content::PasswordFormMap;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PasswordManager);

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

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
void PasswordManager::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kPasswordManagerEnabled,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPasswordManagerAllowShowPasswords,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
}

// static
void PasswordManager::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    PasswordManagerDelegate* delegate) {
  if (FromWebContents(contents)) {
    DCHECK_EQ(delegate, FromWebContents(contents)->delegate_);
    return;
  }

  contents->SetUserData(UserDataKey(),
                        new PasswordManager(contents, delegate));
}

PasswordManager::PasswordManager(WebContents* web_contents,
                                 PasswordManagerDelegate* delegate)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      observer_(NULL) {
  DCHECK(delegate_);
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
                                 delegate_->GetProfile()->GetPrefs());

  ReportMetrics(*password_manager_enabled_);
}

PasswordManager::~PasswordManager() {
}

void PasswordManager::SetFormHasGeneratedPassword(const PasswordForm& form) {
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
      (*iter)->SetHasGeneratedPassword();
      return;
    }
  }
  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  bool ssl_valid = (form.origin.SchemeIsSecure() &&
                    !delegate_->DidLastPageLoadEncounterSSLErrors());
  PasswordFormManager* manager =
      new PasswordFormManager(delegate_->GetProfile(),
                              this,
                              web_contents(),
                              form,
                              ssl_valid);
  pending_login_managers_.push_back(manager);
  manager->SetHasGeneratedPassword();
  // TODO(gcasto): Add UMA stats to track this.
}

bool PasswordManager::IsSavingEnabled() const {
  return *password_manager_enabled_ &&
         !delegate_->GetProfile()->IsOffTheRecord();
}

void PasswordManager::ProvisionallySavePassword(const PasswordForm& form) {
  if (!IsSavingEnabled())
    return;

  // No password to save? Then don't.
  if (form.password_value.empty())
    return;

  scoped_ptr<PasswordFormManager> manager;
  ScopedVector<PasswordFormManager>::iterator matched_manager_it =
      pending_login_managers_.end();
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    // If we find a manager that exactly matches the submitted form including
    // the action URL, exit the loop.
    if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
      matched_manager_it = iter;
      break;
    // If the current manager matches the submitted form excluding the action
    // URL, remember it as a candidate and continue searching for an exact
    // match.
    } else if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_NOT_REQUIRED)) {
      matched_manager_it = iter;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (matched_manager_it != pending_login_managers_.end()) {
    // Transfer ownership of the manager from |pending_login_managers_| to
    // |manager|.
    manager.reset(*matched_manager_it);
    pending_login_managers_.weak_erase(matched_manager_it);
  } else {
    return;
  }

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

  // Always save generated passwords, as the user expresses explicit intent for
  // Chrome to manage such passwords. For other passwords, respect the
  // autocomplete attribute.
  if (!manager->HasGeneratedPassword() && !form.password_autocomplete_set)
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
  pending_login_managers_.clear();
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
  // Ask the SSLManager for current security.
  bool had_ssl_error = delegate_->DidLastPageLoadEncounterSSLErrors();

  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (EndsWith(iter->signon_realm, kSpdyProxyRealm, true))
      continue;

    bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
    PasswordFormManager* manager =
        new PasswordFormManager(delegate_->GetProfile(),
                                this,
                                web_contents(),
                                *iter,
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
    if (provisional_save_manager_->DoesManage(
        *iter, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
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

  // Looks like a successful login attempt. Either show an infobar or
  // automatically save the login data. We prompt when the user hasn't already
  // given consent, either through previously accepting the infobar or by having
  // the browser generate the password.
  provisional_save_manager_->SubmitPassed();
  if (provisional_save_manager_->HasGeneratedPassword())
    UMA_HISTOGRAM_COUNTS("PasswordGeneration.Submitted", 1);
  if (provisional_save_manager_->IsNewLogin() &&
      !provisional_save_manager_->HasGeneratedPassword()) {
    delegate_->AddSavePasswordInfoBarIfPermitted(
        provisional_save_manager_.release());
  } else {
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
      PasswordFormFillData fill_data;
      InitPasswordFormFillData(form_for_autofill,
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
