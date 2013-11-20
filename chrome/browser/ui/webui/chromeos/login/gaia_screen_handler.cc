// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const char kJsScreenPath[] = "login.GaiaSigninScreen";

// Updates params dictionary passed to the auth extension with related
// preferences from CrosSettings.
void UpdateAuthParamsFromSettings(DictionaryValue* params,
                                  const CrosSettings* cros_settings) {
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  bool allow_guest = true;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  // Account creation depends on Guest sign-in (http://crosbug.com/24570).
  params->SetBoolean("createAccount", allow_new_user && allow_guest);
  params->SetBoolean("guestSignin", allow_guest);
}

void UpdateAuthParams(DictionaryValue* params, bool has_users) {
  UpdateAuthParamsFromSettings(params, CrosSettings::Get());

  // Allow locally managed user creation only if:
  // 1. Enterprise managed device > is allowed by policy.
  // 2. Consumer device > owner exists.
  // 3. New users are allowed by owner.

  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);

  bool managed_users_allowed =
      UserManager::Get()->AreLocallyManagedUsersAllowed();
  bool managed_users_can_create = true;
  int message_id = -1;
  if (!has_users) {
    managed_users_can_create = false;
    message_id = IDS_CREATE_LOCALLY_MANAGED_USER_NO_MANAGER_TEXT;
  }
  if (!allow_new_user) {
    managed_users_can_create = false;
    message_id = IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_RESTRICTED_TEXT;
  }

  params->SetBoolean("managedUsersEnabled", managed_users_allowed);
  params->SetBoolean("managedUsersCanCreate", managed_users_can_create);
  if (!managed_users_can_create) {
    params->SetString("managedUsersRestrictionReason",
                      l10n_util::GetStringUTF16(message_id));
  }
}

}  // namespace

GaiaContext::GaiaContext()
    : force_reload(false),
      is_local(false),
      password_changed(false),
      show_users(false),
      use_offline(false),
      has_users(false) {}

GaiaScreenHandler::GaiaScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : BaseScreenHandler(kJsScreenPath),
      frame_state_(FRAME_STATE_UNKNOWN),
      frame_error_(net::OK),
      network_state_informer_(network_state_informer),
      signin_screen_handler_(NULL) {
  DCHECK(network_state_informer_.get());
}

GaiaScreenHandler::~GaiaScreenHandler() {}

void GaiaScreenHandler::LoadGaia(const GaiaContext& context) {
  LOG(WARNING) << "LoadGaia() call.";

  DictionaryValue params;

  params.SetBoolean("forceReload", context.force_reload);
  params.SetBoolean("isLocal", context.is_local);
  params.SetBoolean("passwordChanged", context.password_changed);
  params.SetBoolean("isShowUsers", context.show_users);
  params.SetBoolean("useOffline", context.use_offline);
  params.SetString("email", context.email);

  UpdateAuthParams(&params, context.has_users);

  if (!context.use_offline) {
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    if (!app_locale.empty())
      params.SetString("hl", app_locale);
  } else {
    base::DictionaryValue* localized_strings = new base::DictionaryValue();
    localized_strings->SetString(
        "stringEmail", l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMAIL));
    localized_strings->SetString(
        "stringPassword",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_PASSWORD));
    localized_strings->SetString(
        "stringSignIn", l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_SIGNIN));
    localized_strings->SetString(
        "stringEmptyEmail",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMPTY_EMAIL));
    localized_strings->SetString(
        "stringEmptyPassword",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMPTY_PASSWORD));
    localized_strings->SetString(
        "stringError", l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_ERROR));
    params.Set("localizedStrings", localized_strings);
  }

  const GURL gaia_url =
      CommandLine::ForCurrentProcess()->HasSwitch(::switches::kGaiaUrl)
          ? GURL(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                ::switches::kGaiaUrl))
          : GaiaUrls::GetInstance()->gaia_url();
  params.SetString("gaiaUrl", gaia_url.spec());

  frame_state_ = FRAME_STATE_LOADING;
  CallJS("loadAuthExtension", params);
}

void GaiaScreenHandler::UpdateGaia(const GaiaContext& context) {
  DictionaryValue params;
  UpdateAuthParams(&params, context.has_users);
  CallJS("updateAuthExtension", params);
}

void GaiaScreenHandler::ReloadGaia() {
  if (frame_state_ == FRAME_STATE_LOADING)
    return;
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE) {
    LOG(WARNING) << "Skipping reloading of Gaia since "
                 << "network state="
                 << NetworkStateInformer::StatusString(state);
    return;
  }
  LOG(WARNING) << "Reloading Gaia.";
  frame_state_ = FRAME_STATE_LOADING;
  CallJS("doReload");
}

void GaiaScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("signinScreenTitle", IDS_SIGNIN_SCREEN_TITLE);
  builder->Add("signinScreenPasswordChanged",
               IDS_SIGNIN_SCREEN_PASSWORD_CHANGED);
  builder->Add("createAccount", IDS_CREATE_ACCOUNT_HTML);
  builder->Add("guestSignin", IDS_BROWSE_WITHOUT_SIGNING_IN_HTML);
  builder->Add("createLocallyManagedUser",
               IDS_CREATE_LOCALLY_MANAGED_USER_HTML);
  builder->Add("createManagedUserFeatureName",
               IDS_CREATE_LOCALLY_MANAGED_USER_FEATURE_NAME);

  // Strings used by no password warning dialog.
  builder->Add("noPasswordWarningTitle", IDS_LOGIN_NO_PASSWORD_WARNING_TITLE);
  builder->Add("noPasswordWarningBody", IDS_LOGIN_NO_PASSWORD_WARNING);
  builder->Add("noPasswordWarningOkButton",
               IDS_LOGIN_NO_PASSWORD_WARNING_DISMISS_BUTTON);
}

void GaiaScreenHandler::Initialize() {}

void GaiaScreenHandler::RegisterMessages() {
  AddCallback("frameLoadingCompleted",
              &GaiaScreenHandler::HandleFrameLoadingCompleted);
}

void GaiaScreenHandler::HandleFrameLoadingCompleted(int status) {
  const net::Error frame_error = static_cast<net::Error>(-status);
  if (frame_error == net::ERR_ABORTED) {
    LOG(WARNING) << "Ignoring Gaia frame error: " << frame_error;
    return;
  }
  frame_error_ = frame_error;
  if (frame_error == net::OK) {
    LOG(INFO) << "Gaia is loaded";
    frame_state_ = FRAME_STATE_LOADED;
  } else {
    LOG(WARNING) << "Gaia frame error: " << frame_error_;
    frame_state_ = FRAME_STATE_ERROR;
  }

  if (network_state_informer_->state() != NetworkStateInformer::ONLINE)
    return;
  if (frame_state_ == FRAME_STATE_LOADED)
    UpdateState(ErrorScreenActor::ERROR_REASON_UPDATE);
  else if (frame_state_ == FRAME_STATE_ERROR)
    UpdateState(ErrorScreenActor::ERROR_REASON_FRAME_ERROR);
}

void GaiaScreenHandler::UpdateState(ErrorScreenActor::ErrorReason reason) {
  if (signin_screen_handler_)
    signin_screen_handler_->UpdateState(reason);
}

void GaiaScreenHandler::SetSigninScreenHandler(SigninScreenHandler* handler) {
  signin_screen_handler_ = handler;
}

}  // namespace chromeos
