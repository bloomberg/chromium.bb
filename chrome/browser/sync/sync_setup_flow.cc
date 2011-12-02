// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_flow.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow_handler.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/oauth.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

namespace {

// Helper function to disable password sync.
void DisablePasswordSync(ProfileSyncService* service) {
  syncable::ModelTypeSet types;
  service->GetPreferredDataTypes(&types);
  types.erase(syncable::PASSWORDS);
  service->OnUserChoseDatatypes(false, types);
}

// Returns the next step for the non-fatal error case.
SyncSetupWizard::State GetStepForNonFatalError(ProfileSyncService* service) {
  // TODO(sync): Update this error handling to allow different platforms to
  // display the error appropriately (http://crbug.com/92722) instead of
  // navigating to a LOGIN state that is not supported on every platform.
  if (service->IsPassphraseRequired()) {
    if (service->IsUsingSecondaryPassphrase())
      return SyncSetupWizard::ENTER_PASSPHRASE;
    return SyncSetupWizard::GetLoginState();
  }

  const GoogleServiceAuthError& error = service->GetAuthError();
  if (error.state() == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      error.state() == GoogleServiceAuthError::CAPTCHA_REQUIRED ||
      error.state() == GoogleServiceAuthError::ACCOUNT_DELETED ||
      error.state() == GoogleServiceAuthError::ACCOUNT_DISABLED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE)
    return SyncSetupWizard::GetLoginState();

  NOTREACHED();
  return SyncSetupWizard::FATAL_ERROR;
}

}  // namespace

SyncConfiguration::SyncConfiguration()
    : encrypt_all(false),
      sync_everything(false),
      set_secondary_passphrase(false),
      set_gaia_passphrase(false) {
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
  if (start == SyncSetupWizard::NONFATAL_ERROR)
    start = GetStepForNonFatalError(service);
  if ((start == SyncSetupWizard::CONFIGURE ||
       start == SyncSetupWizard::SYNC_EVERYTHING ||
       start == SyncSetupWizard::ENTER_PASSPHRASE) &&
      !service->sync_initialized()) {
    // We are trying to open configuration window, but the backend isn't ready.
    // We just return NULL. This has the effect of the flow getting reset, and
    // the user's action has no effect.
    LOG(ERROR) << "Attempted to show sync configure before backend ready.";
    return NULL;
  }
  return new SyncSetupFlow(start, end, container, service);
}

// static
void SyncSetupFlow::GetArgsForGaiaLogin(const ProfileSyncService* service,
                                        DictionaryValue* args) {
  const GoogleServiceAuthError& error = service->GetAuthError();
  if (!service->last_attempted_user_email().empty()) {
    args->SetString("user", service->last_attempted_user_email());
    args->SetInteger("error", error.state());
    args->SetBoolean("editable_user", true);
  } else {
    string16 user;
    user = UTF8ToUTF16(service->profile()->GetPrefs()->GetString(
        prefs::kGoogleServicesUsername));
    args->SetString("user", user);
    args->SetInteger("error", 0);
    args->SetBoolean("editable_user", user.empty());
  }

  args->SetString("captchaUrl", error.captcha().image_url.spec());
}

void SyncSetupFlow::GetArgsForConfigure(ProfileSyncService* service,
                                        DictionaryValue* args) {
  // The SYNC_EVERYTHING case will set this to true.
  args->SetBoolean("showSyncEverythingPage", false);

  args->SetBoolean("syncAllDataTypes",
      service->profile()->GetPrefs()->GetBoolean(
          prefs::kSyncKeepEverythingSynced));

  // Bookmarks, Preferences, and Themes are launched for good, there's no
  // going back now.  Check if the other data types are registered though.
  syncable::ModelTypeSet registered_types;
  service->GetRegisteredDataTypes(&registered_types);
  syncable::ModelTypeSet preferred_types;
  service->GetPreferredDataTypes(&preferred_types);
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
                   preferred_types.count(syncable::BOOKMARKS) > 0);
  args->SetBoolean("syncPreferences",
                   preferred_types.count(syncable::PREFERENCES) > 0);
  args->SetBoolean("syncThemes",
                   preferred_types.count(syncable::THEMES) > 0);
  args->SetBoolean("syncPasswords",
                   preferred_types.count(syncable::PASSWORDS) > 0);
  args->SetBoolean("syncAutofill",
                   preferred_types.count(syncable::AUTOFILL) > 0);
  args->SetBoolean("syncExtensions",
                   preferred_types.count(syncable::EXTENSIONS) > 0);
  args->SetBoolean("syncSessions",
                   preferred_types.count(syncable::SESSIONS) > 0);
  args->SetBoolean("syncTypedUrls",
                   preferred_types.count(syncable::TYPED_URLS) > 0);
  args->SetBoolean("syncApps",
                   preferred_types.count(syncable::APPS) > 0);

  args->SetBoolean("encryptionEnabled",
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSyncEncryption));

  bool encrypt_all = service->EncryptEverythingEnabled();
  if (service->encryption_pending())
    encrypt_all = true;
  args->SetBoolean("encryptAllData", encrypt_all);

  // Load the parameters for the encryption tab.
  args->SetBoolean("usePassphrase", service->IsUsingSecondaryPassphrase());

  // Determine if we need a passphrase or not, and if so, prompt the user.
  if (service->IsPassphraseRequiredForDecryption() &&
      (service->IsUsingSecondaryPassphrase() || cached_passphrase_.empty())) {
    // We need a passphrase, and either it's an explicit passphrase, or we
    // don't have a cached gaia passphrase, so we have to prompt the user.
    args->SetBoolean("show_passphrase", true);
    // Tell the UI layer what kind of passphrase we need.
    args->SetBoolean("need_google_passphrase",
                     !service->IsUsingSecondaryPassphrase());
    args->SetBoolean("passphrase_creation_rejected",
                     user_tried_creating_explicit_passphrase_);
    args->SetBoolean("passphrase_setting_rejected",
                     user_tried_setting_passphrase_);
  }
}

bool SyncSetupFlow::AttachSyncSetupHandler(SyncSetupFlowHandler* handler) {
  if (flow_handler_)
    return false;

  flow_handler_ = handler;
  handler->SetFlow(this);
  ActivateState(current_state_);
  return true;
}

bool SyncSetupFlow::IsAttached() const {
  return flow_handler_ != NULL;
}

void SyncSetupFlow::Advance(SyncSetupWizard::State advance_state) {
  if (!ShouldAdvance(advance_state)) {
    LOG(WARNING) << "Invalid state change from "
                 << current_state_ << " to " << advance_state;
    return;
  }

  if (flow_handler_)
    ActivateState(advance_state);
}

void SyncSetupFlow::Focus() {
  // This gets called from SyncSetupWizard::Focus(), and might get called
  // before flow_handler_ is set in AttachSyncSetupHandler() (which gets
  // called asynchronously after the UI initializes).
  if (flow_handler_)
    flow_handler_->Focus();
}

// A callback to notify the delegate that the dialog closed.
// TODO(rickcam): Bug 90713: Handle OAUTH_LOGIN case here
void SyncSetupFlow::OnDialogClosed(const std::string& json_retval) {
  DCHECK(json_retval.empty());
  container_->set_flow(NULL);  // Sever ties from the wizard.
  // If we've reached the end, mark it.  This could be a discrete run, in which
  // case it's already set, but it simplifes the logic to do it this way.
  if (current_state_ == end_state_)
    service_->SetSyncSetupCompleted();

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
  // It's possible to receive an empty password (e.g. for ASP's), in which case
  // we don't want to overwrite any previously cached password.
  if (!password.empty())
    cached_passphrase_ = password;
  service_->OnUserSubmittedAuth(username, password, captcha, access_code);
}

void SyncSetupFlow::OnUserSubmittedOAuth(
    const std::string& oauth1_request_token) {
  service_->OnUserSubmittedOAuth(oauth1_request_token);
}

void SyncSetupFlow::OnUserConfigured(const SyncConfiguration& configuration) {
  // Go to the "loading..." screen.
  Advance(SyncSetupWizard::SETTING_UP);

  // Note: encryption will not occur until OnUserChoseDatatypes is called.
  if (configuration.encrypt_all)
    service_->EnableEncryptEverything();

  bool set_new_decryption_passphrase = false;
  if (configuration.set_gaia_passphrase) {
    // Caller passed a gaia passphrase. This is illegal if we are currently
    // using a secondary passphrase.
    DCHECK(!service_->IsUsingSecondaryPassphrase());
    service_->SetPassphrase(configuration.gaia_passphrase, false);
    // Since the user entered the passphrase manually, set this flag so we can
    // report an error if the passphrase setting failed.
    user_tried_setting_passphrase_ = true;
    set_new_decryption_passphrase = true;
  } else if (!service_->IsUsingSecondaryPassphrase() &&
             !cached_passphrase_.empty()) {
    // Service needs a GAIA passphrase and we have one cached, so try it.
    service_->SetPassphrase(cached_passphrase_, false);
    cached_passphrase_.clear();
    set_new_decryption_passphrase = true;
  } else {
    // We can get here if the user changes their GAIA passphrase but still has
    // data encrypted with the old passphrase. The UI prompts the user for their
    // passphrase, but they might just leave it blank/disable the encrypted
    // types.
    // No gaia passphrase cached or set, so make sure the ProfileSyncService
    // wasn't expecting one.
    DLOG_IF(WARNING, !service_->IsUsingSecondaryPassphrase() &&
            service_->IsPassphraseRequiredForDecryption()) <<
        "Google passphrase required but not provided by UI";
  }

  // Set the secondary passphrase, either as a decryption passphrase, or
  // as an attempt to encrypt the user's data using this new passphrase.
  if (configuration.set_secondary_passphrase) {
    service_->SetPassphrase(configuration.secondary_passphrase, true);
    if (service_->IsUsingSecondaryPassphrase()) {
      user_tried_setting_passphrase_ = true;
      set_new_decryption_passphrase = true;
    } else {
      user_tried_creating_explicit_passphrase_ = true;
    }
  }

  service_->OnUserChoseDatatypes(configuration.sync_everything,
                                 configuration.data_types);

  // See if we are done configuring (if we don't need a passphrase, and don't
  // need to hang around waiting for encryption to happen, just exit). This call
  // to IsPassphraseRequiredForDecryption() takes into account the data types
  // we just enabled/disabled.
  if (!service_->IsPassphraseRequiredForDecryption() &&
      !service_->encryption_pending()) {
    Advance(SyncSetupWizard::DONE);
  } else if (!set_new_decryption_passphrase) {
    // We need a passphrase, but the user did not provide one, so transition
    // directly to ENTER_PASSPHRASE (otherwise we'll have to wait until
    // the sync engine generates another OnPassphraseRequired() at the end of
    // the sync cycle which can take a long time).
    Advance(SyncSetupWizard::ENTER_PASSPHRASE);
  }
}

void SyncSetupFlow::OnPassphraseEntry(const std::string& passphrase) {
  Advance(SyncSetupWizard::SETTING_UP);
  service_->SetPassphrase(passphrase, true);
  user_tried_setting_passphrase_ = true;
}

void SyncSetupFlow::OnPassphraseCancel() {
  // If the user cancels when being asked for the passphrase,
  // just disable encrypted sync and continue setting up.
  if (current_state_ == SyncSetupWizard::ENTER_PASSPHRASE)
    DisablePasswordSync(service_);

  Advance(SyncSetupWizard::SETTING_UP);
}

// Use static Run method to get an instance.
SyncSetupFlow::SyncSetupFlow(SyncSetupWizard::State start_state,
                             SyncSetupWizard::State end_state,
                             SyncSetupFlowContainer* container,
                             ProfileSyncService* service)
    : container_(container),
      current_state_(start_state),
      end_state_(end_state),
      login_start_time_(base::TimeTicks::Now()),
      flow_handler_(NULL),
      service_(service),
      user_tried_creating_explicit_passphrase_(false),
      user_tried_setting_passphrase_(false) {
}

// Returns true if the flow should advance to |state| based on |current_state_|.
bool SyncSetupFlow::ShouldAdvance(SyncSetupWizard::State state) {
  switch (state) {
    case SyncSetupWizard::OAUTH_LOGIN:
      return current_state_ == SyncSetupWizard::FATAL_ERROR ||
             current_state_ == SyncSetupWizard::OAUTH_LOGIN ||
             current_state_ == SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::GAIA_LOGIN:
      return current_state_ == SyncSetupWizard::FATAL_ERROR ||
             current_state_ == SyncSetupWizard::GAIA_LOGIN ||
             current_state_ == SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::GAIA_SUCCESS:
      return current_state_ == SyncSetupWizard::GAIA_LOGIN ||
             current_state_ == SyncSetupWizard::OAUTH_LOGIN;
    case SyncSetupWizard::SYNC_EVERYTHING:  // Intentionally fall through.
    case SyncSetupWizard::CONFIGURE:
      return current_state_ == SyncSetupWizard::GAIA_SUCCESS;
    case SyncSetupWizard::ENTER_PASSPHRASE:
      return (service_->auto_start_enabled() &&
              current_state_ == SyncSetupWizard::GAIA_LOGIN) ||
             current_state_ == SyncSetupWizard::SYNC_EVERYTHING ||
             current_state_ == SyncSetupWizard::CONFIGURE ||
             current_state_ == SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR:
      return current_state_ != SyncSetupWizard::ABORT;
    case SyncSetupWizard::SETTING_UP:
      return current_state_ == SyncSetupWizard::SYNC_EVERYTHING ||
             current_state_ == SyncSetupWizard::CONFIGURE ||
             current_state_ == SyncSetupWizard::ENTER_PASSPHRASE;
    case SyncSetupWizard::NONFATAL_ERROR:  // Intentionally fall through.
    case SyncSetupWizard::FATAL_ERROR:
      return current_state_ != SyncSetupWizard::ABORT;
    case SyncSetupWizard::ABORT:
      return true;
    case SyncSetupWizard::DONE:
      return current_state_ == SyncSetupWizard::SETTING_UP ||
             current_state_ == SyncSetupWizard::ENTER_PASSPHRASE;
    default:
      NOTREACHED() << "Unhandled State: " << state;
      return false;
  }
}

void SyncSetupFlow::ActivateState(SyncSetupWizard::State state) {
  DCHECK(flow_handler_);

  if (state == SyncSetupWizard::NONFATAL_ERROR)
    state = GetStepForNonFatalError(service_);

  current_state_ = state;

  switch (state) {
    case SyncSetupWizard::OAUTH_LOGIN: {
      flow_handler_->ShowOAuthLogin();
      break;
    }
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
      flow_handler_->ShowGaiaSuccessAndSettingUp();
      break;
    case SyncSetupWizard::SYNC_EVERYTHING: {
      DictionaryValue args;
      SyncSetupFlow::GetArgsForConfigure(service_, &args);
      args.SetBoolean("showSyncEverythingPage", true);
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
      SyncSetupFlow::GetArgsForConfigure(service_, &args);
      GetArgsForConfigure(service_, &args);
      // TODO(atwilson): Remove ShowPassphraseEntry in favor of using
      // ShowConfigure() - http://crbug.com/90786.
      flow_handler_->ShowPassphraseEntry(args);
      break;
    }
    case SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR: {
      // TODO(sync): We should expose a real "display an error" API on
      // SyncSetupFlowHandler (crbug.com/92722) but for now just transition
      // to the login state with a special error code.
      DictionaryValue args;
      SyncSetupFlow::GetArgsForGaiaLogin(service_, &args);
      args.SetInteger("error", GoogleServiceAuthError::SERVICE_UNAVAILABLE);
      current_state_ = SyncSetupWizard::GAIA_LOGIN;
      flow_handler_->ShowGaiaLogin(args);
      break;
    }
    case SyncSetupWizard::SETTING_UP: {
      flow_handler_->ShowSettingUp();
      break;
    }
    case SyncSetupWizard::FATAL_ERROR: {
      // This shows the user the "Could not connect to server" error.
      // TODO(sync): Update this error handling to allow different platforms to
      // display the error appropriately (http://crbug.com/92722).
      DictionaryValue args;
      SyncSetupFlow::GetArgsForGaiaLogin(service_, &args);
      args.SetBoolean("fatalError", true);
      current_state_ = SyncSetupWizard::GAIA_LOGIN;
      flow_handler_->ShowGaiaLogin(args);
      break;
    }
    case SyncSetupWizard::DONE:
    case SyncSetupWizard::ABORT:
      flow_handler_->ShowSetupDone(UTF8ToUTF16(
          service_->profile()->GetPrefs()->GetString(
          prefs::kGoogleServicesUsername)));
      break;
    default:
      NOTREACHED() << "Invalid advance state: " << state;
  }
}
