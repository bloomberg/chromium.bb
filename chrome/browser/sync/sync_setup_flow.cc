// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_flow.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/gfx/font.h"

namespace {

// Helper function to disable password sync.
void DisablePasswordSync(ProfileSyncService* service) {
  syncable::ModelTypeSet types;
  service->GetPreferredDataTypes(&types);
  types.erase(syncable::PASSWORDS);
  service->OnUserChoseDatatypes(false, types);
}

}  // namespace

SyncConfiguration::SyncConfiguration()
    : sync_everything(false),
      use_secondary_passphrase(false) {
}

SyncConfiguration::~SyncConfiguration() {}

SyncSetupFlow::~SyncSetupFlow() {
  flow_handler_->SetFlow(NULL);
}

// static
SyncSetupFlow* SyncSetupFlow::Run(ProfileSyncService* service,
                                  SyncSetupFlowContainer* container,
                                  SyncSetupWizard::State start,
                                  SyncSetupWizard::State end) {
  DictionaryValue args;
  if (start == SyncSetupWizard::GAIA_LOGIN)
    SyncSetupFlow::GetArgsForGaiaLogin(service, &args);
  else if (start == SyncSetupWizard::CONFIGURE)
    SyncSetupFlow::GetArgsForConfigure(service, &args);
  else if (start == SyncSetupWizard::ENTER_PASSPHRASE)
    SyncSetupFlow::GetArgsForEnterPassphrase(false, false, &args);
  else if (start == SyncSetupWizard::PASSPHRASE_MIGRATION)
    args.SetString("iframeToShow", "firstpassphrase");

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  SyncSetupFlow* flow = new SyncSetupFlow(start, end, json_args,
      container, service);

  Browser* b = BrowserList::GetLastActive();
  b->ShowOptionsTab(chrome::kSyncSetupSubPage);
  return flow;
}

// static
void SyncSetupFlow::GetArgsForGaiaLogin(const ProfileSyncService* service,
                                        DictionaryValue* args) {
  args->SetString("iframeToShow", "login");
  const GoogleServiceAuthError& error = service->GetAuthError();
  if (!service->last_attempted_user_email().empty()) {
    args->SetString("user", service->last_attempted_user_email());
    args->SetInteger("error", error.state());
    args->SetBoolean("editable_user", true);
  } else {
    string16 user;
    if (!service->cros_user().empty())
      user = UTF8ToUTF16(service->cros_user());
    else
      user = service->GetAuthenticatedUsername();
    args->SetString("user", user);
    args->SetInteger("error", 0);
    args->SetBoolean("editable_user", user.empty());
  }

  args->SetString("captchaUrl", error.captcha().image_url.spec());
}

// static
void SyncSetupFlow::GetArgsForConfigure(ProfileSyncService* service,
                                        DictionaryValue* args) {
  args->SetString("iframeToShow", "configure");

  // The SYNC_EVERYTHING case will set this to true.
  args->SetBoolean("syncEverything", false);

  args->SetBoolean("keepEverythingSynced",
      service->profile()->GetPrefs()->GetBoolean(prefs::kKeepEverythingSynced));

  // Bookmarks, Preferences, and Themes are launched for good, there's no
  // going back now.  Check if the other data types are registered though.
  syncable::ModelTypeSet registered_types;
  service->GetRegisteredDataTypes(&registered_types);
  args->SetBoolean("passwordsRegistered",
      registered_types.count(syncable::PASSWORDS) > 0);
  args->SetBoolean("autofillRegistered",
      registered_types.count(syncable::AUTOFILL) > 0);
  args->SetBoolean("extensionsRegistered",
      registered_types.count(syncable::EXTENSIONS) > 0);
  args->SetBoolean("typedUrlsRegistered",
      registered_types.count(syncable::TYPED_URLS) > 0);
  args->SetBoolean("appsRegistered",
      registered_types.count(syncable::APPS) > 0);
  args->SetBoolean("sessionsRegistered",
      registered_types.count(syncable::SESSIONS) > 0);
  args->SetBoolean("syncBookmarks",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncBookmarks));
  args->SetBoolean("syncPreferences",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncPreferences));
  args->SetBoolean("syncThemes",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncThemes));
  args->SetBoolean("syncPasswords",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncPasswords));
  args->SetBoolean("syncAutofill",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncAutofill));
  args->SetBoolean("syncExtensions",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncExtensions));
  args->SetBoolean("syncSessions",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncSessions));
  args->SetBoolean("syncTypedUrls",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncTypedUrls));
  args->SetBoolean("syncApps",
      service->profile()->GetPrefs()->GetBoolean(prefs::kSyncApps));

  // Load the parameters for the encryption tab.
  args->SetBoolean("usePassphrase", service->IsUsingSecondaryPassphrase());
}

// static
void SyncSetupFlow::GetArgsForEnterPassphrase(
    bool tried_creating_explicit_passphrase,
    bool tried_setting_explicit_passphrase,
    DictionaryValue* args) {
  args->SetString("iframeToShow", "passphrase");
  args->SetBoolean("passphrase_creation_rejected",
                   tried_creating_explicit_passphrase);
  args->SetBoolean("passphrase_setting_rejected",
                   tried_setting_explicit_passphrase);
}

void SyncSetupFlow::AttachSyncSetupHandler(SyncSetupFlowHandler* handler) {
  flow_handler_ = handler;
  ActivateState(current_state_);
}

void SyncSetupFlow::Advance(SyncSetupWizard::State advance_state) {
  if (!ShouldAdvance(advance_state)) {
    LOG(WARNING) << "Invalid state change from "
                 << current_state_ << " to " << advance_state;
    return;
  }

  ActivateState(advance_state);
}

void SyncSetupFlow::Focus() {
  // TODO(jhawkins): Implement this.
}

// A callback to notify the delegate that the dialog closed.
void SyncSetupFlow::OnDialogClosed(const std::string& json_retval) {
  DCHECK(json_retval.empty());
  container_->set_flow(NULL);  // Sever ties from the wizard.
  if (current_state_ == SyncSetupWizard::DONE ||
      current_state_ == SyncSetupWizard::DONE_FIRST_TIME) {
    service_->SetSyncSetupCompleted();
  }

  // Record the state at which the user cancelled the signon dialog.
  switch (current_state_) {
    case SyncSetupWizard::GAIA_LOGIN:
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_FROM_SIGNON_WITHOUT_AUTH);
      break;
    case SyncSetupWizard::GAIA_SUCCESS:
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_DURING_SIGNON);
      break;
    case SyncSetupWizard::CONFIGURE:
    case SyncSetupWizard::ENTER_PASSPHRASE:
    case SyncSetupWizard::SETTING_UP:
      // TODO(atwilson): Treat a close during ENTER_PASSPHRASE like a
      // Cancel + Skip (i.e. call OnPassphraseCancel()). http://crbug.com/74645
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_DURING_CONFIGURE);
      break;
    case SyncSetupWizard::DONE_FIRST_TIME:
    case SyncSetupWizard::DONE:
      // TODO(sync): rename this histogram; it's tracking authorization AND
      // initial sync download time.
      UMA_HISTOGRAM_MEDIUM_TIMES("Sync.UserPerceivedAuthorizationTime",
                                 base::TimeTicks::Now() - login_start_time_);
      break;
    default:
      break;
  }

  service_->OnUserCancelledDialog();
  delete this;
}

void SyncSetupFlow::OnUserSubmittedAuth(const std::string& username,
                                        const std::string& password,
                                        const std::string& captcha,
                                        const std::string& access_code) {
  service_->OnUserSubmittedAuth(username, password, captcha, access_code);
}

void SyncSetupFlow::OnUserConfigured(const SyncConfiguration& configuration) {
  // Go to the "loading..." screen.
  Advance(SyncSetupWizard::SETTING_UP);

  // If we are activating the passphrase, we need to have one supplied.
  DCHECK(service_->IsUsingSecondaryPassphrase() ||
         !configuration.use_secondary_passphrase ||
         configuration.secondary_passphrase.length() > 0);

  if (configuration.use_secondary_passphrase &&
      !service_->IsUsingSecondaryPassphrase()) {
    service_->SetPassphrase(configuration.secondary_passphrase, true, true);
    tried_creating_explicit_passphrase_ = true;
  }

  service_->OnUserChoseDatatypes(configuration.sync_everything,
                                 configuration.data_types);
}

void SyncSetupFlow::OnPassphraseEntry(const std::string& passphrase) {
  Advance(SyncSetupWizard::SETTING_UP);
  service_->SetPassphrase(passphrase, true, false);
  tried_setting_explicit_passphrase_ = true;
}

void SyncSetupFlow::OnPassphraseCancel() {
  // If the user cancels when being asked for the passphrase,
  // just disable encrypted sync and continue setting up.
  if (current_state_ == SyncSetupWizard::ENTER_PASSPHRASE)
    DisablePasswordSync(service_);

  Advance(SyncSetupWizard::SETTING_UP);
}

// TODO(jhawkins): Remove this method.
void SyncSetupFlow::OnFirstPassphraseEntry(const std::string& option,
                                           const std::string& passphrase) {
  NOTREACHED();
}

// TODO(jhawkins): Use this method instead of a direct link in the html.
void SyncSetupFlow::OnGoToDashboard() {
  BrowserList::GetLastActive()->OpenPrivacyDashboardTabAndActivate();
}

// Use static Run method to get an instance.
SyncSetupFlow::SyncSetupFlow(SyncSetupWizard::State start_state,
                             SyncSetupWizard::State end_state,
                             const std::string& args,
                             SyncSetupFlowContainer* container,
                             ProfileSyncService* service)
    : container_(container),
      dialog_start_args_(args),
      current_state_(start_state),
      end_state_(end_state),
      login_start_time_(base::TimeTicks::Now()),
      flow_handler_(NULL),
      service_(service),
      tried_creating_explicit_passphrase_(false),
      tried_setting_explicit_passphrase_(false) {
}

// Returns true if the flow should advance to |state| based on |current_state_|.
bool SyncSetupFlow::ShouldAdvance(SyncSetupWizard::State state) {
  switch (state) {
    case SyncSetupWizard::GAIA_LOGIN:
      return current_state_ == SyncSetupWizard::FATAL_ERROR ||
             current_state_ == SyncSetupWizard::GAIA_LOGIN ||
             current_state_ == SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::GAIA_SUCCESS:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN;
    case SyncSetupWizard::SYNC_EVERYTHING:
    case SyncSetupWizard::CONFIGURE:
      return current_state_ == SyncSetupWizard::GAIA_SUCCESS;
    case SyncSetupWizard::ENTER_PASSPHRASE:
      return current_state_ == SyncSetupWizard::SYNC_EVERYTHING ||
             current_state_ == SyncSetupWizard::CONFIGURE ||
             current_state_ == SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::PASSPHRASE_MIGRATION:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN;
    case SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR:
      DCHECK(current_state_ != SyncSetupWizard::GAIA_LOGIN &&
             current_state_ != SyncSetupWizard::GAIA_SUCCESS);
      return true;
    case SyncSetupWizard::SETTING_UP:
      return current_state_ == SyncSetupWizard::SYNC_EVERYTHING ||
             current_state_ == SyncSetupWizard::CONFIGURE ||
             current_state_ == SyncSetupWizard::ENTER_PASSPHRASE ||
             current_state_ == SyncSetupWizard::PASSPHRASE_MIGRATION;
    case SyncSetupWizard::FATAL_ERROR:
      return true;  // You can always hit the panic button.
    case SyncSetupWizard::DONE_FIRST_TIME:
    case SyncSetupWizard::DONE:
      return current_state_ == SyncSetupWizard::SETTING_UP ||
             current_state_ == SyncSetupWizard::ENTER_PASSPHRASE;
    default:
      NOTREACHED() << "Unhandled State: " << state;
      return false;
  }
}

void SyncSetupFlow::ActivateState(SyncSetupWizard::State state) {
  switch (state) {
    case SyncSetupWizard::GAIA_LOGIN: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForGaiaLogin(service_, &args);
      flow_handler_->ShowGaiaLogin(args);
      break;
    }
    case SyncSetupWizard::GAIA_SUCCESS:
      if (end_state_ == SyncSetupWizard::GAIA_SUCCESS) {
        flow_handler_->ShowGaiaSuccessAndClose();
        break;
      }
      state = SyncSetupWizard::SYNC_EVERYTHING;
      //  Fall through.
    case SyncSetupWizard::SYNC_EVERYTHING: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForConfigure(service_, &args);
      args.SetBoolean("syncEverything", true);
      flow_handler_->ShowConfigure(args);
      break;
    }
    case SyncSetupWizard::CONFIGURE: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForConfigure(service_, &args);
      flow_handler_->ShowConfigure(args);
      break;
    }
    case SyncSetupWizard::ENTER_PASSPHRASE: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForEnterPassphrase(
          tried_creating_explicit_passphrase_,
          tried_setting_explicit_passphrase_,
          &args);
      flow_handler_->ShowPassphraseEntry(args);
      break;
    }
    case SyncSetupWizard::PASSPHRASE_MIGRATION: {
      DictionaryValue args;
      args.SetString("iframeToShow", "firstpassphrase");
      flow_handler_->ShowFirstPassphrase(args);
      break;
    }
    case SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForConfigure(service_, &args);
      args.SetBoolean("was_aborted", true);
      flow_handler_->ShowConfigure(args);
      break;
    }
    case SyncSetupWizard::SETTING_UP: {
      flow_handler_->ShowSettingUp();
      break;
    }
    case SyncSetupWizard::FATAL_ERROR: {
      // This shows the user the "Could not connect to server" error.
      // TODO(sync): Update this error messaging.
      DictionaryValue args;
      SyncSetupFlow::GetArgsForGaiaLogin(service_, &args);
      args.SetInteger("error", GoogleServiceAuthError::CONNECTION_FAILED);
      flow_handler_->ShowGaiaLogin(args);
      break;
    }
    case SyncSetupWizard::DONE_FIRST_TIME:
      flow_handler_->ShowFirstTimeDone(
          UTF16ToWide(service_->GetAuthenticatedUsername()));
      break;
    case SyncSetupWizard::DONE:
      flow_handler_->ShowSetupDone(
          UTF16ToWide(service_->GetAuthenticatedUsername()));
      break;
    default:
      NOTREACHED() << "Invalid advance state: " << state;
  }
  current_state_ = state;
}
