// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enterprise_enrollment_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

// EnterpriseEnrollmentScreenHandler, public -----------------------------------

EnterpriseEnrollmentScreenHandler::EnterpriseEnrollmentScreenHandler()
    : controller_(NULL), editable_user_(true), show_on_init_(false) {
}

EnterpriseEnrollmentScreenHandler::~EnterpriseEnrollmentScreenHandler() {}

void EnterpriseEnrollmentScreenHandler::SetupGaiaStrings() {
  if (!controller_) {
    NOTREACHED();
    return;
  }
  std::string user;
  bool has_init_user = controller_->GetInitialUser(&user);
  if (!has_init_user)
    user = "";
  // Set the arguments for showing the gaia login page.
  base::DictionaryValue args;
  args.SetString("user", user);
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", !has_init_user);
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  RenderViewHost* render_view_host =
      web_ui_->tab_contents()->render_view_host();
  render_view_host->SetWebUIProperty("dialogArguments", json_args);
}

// EnterpriseEnrollmentScreenHandler, WebUIMessageHandler implementation -------

void EnterpriseEnrollmentScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "SubmitAuth",
      base::Bind(&EnterpriseEnrollmentScreenHandler::HandleSubmitAuth,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
      "DialogClose",
      base::Bind(&EnterpriseEnrollmentScreenHandler::HandleCancelAuth,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
      "confirmationClose",
      base::Bind(&EnterpriseEnrollmentScreenHandler::HandleConfirmationClose,
                 base::Unretained(this)));
}

// EnterpriseEnrollmentScreenHandler
//      EnterpriseEnrollmentScreenActor implementation -------------------------

void EnterpriseEnrollmentScreenHandler::SetController(Controller* controller) {
  controller_ = controller;
}

void EnterpriseEnrollmentScreenHandler::PrepareToShow() {
}

void EnterpriseEnrollmentScreenHandler::Show() {
  SetupGaiaStrings();

  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen("enrollment", NULL);
}

void EnterpriseEnrollmentScreenHandler::Hide() {
}

void EnterpriseEnrollmentScreenHandler::SetEditableUser(bool editable) {
  editable_user_ = editable;
}

void EnterpriseEnrollmentScreenHandler::ShowConfirmationScreen() {
  web_ui_->CallJavascriptFunction(
      "oobe.EnrollmentScreen.showConfirmationScreen");
  NotifyObservers(true);
}

void EnterpriseEnrollmentScreenHandler::ShowAuthError(
    const GoogleServiceAuthError& error) {
  base::DictionaryValue args;
  args.SetInteger("error", error.state());
  args.SetBoolean("editable_user", editable_user_);
  args.SetString("captchaUrl", error.captcha().image_url.spec());
  UpdateGaiaLogin(args);
}

void EnterpriseEnrollmentScreenHandler::ShowAccountError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR);
  NotifyObservers(false);
}

void EnterpriseEnrollmentScreenHandler::ShowSerialNumberError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_SERIAL_NUMBER_ERROR);
  NotifyObservers(false);
}

void EnterpriseEnrollmentScreenHandler::ShowFatalAuthError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_AUTH_ERROR);
  NotifyObservers(false);
}

void EnterpriseEnrollmentScreenHandler::ShowFatalEnrollmentError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR);
  NotifyObservers(false);
}

void EnterpriseEnrollmentScreenHandler::ShowNetworkEnrollmentError() {
  ShowError(IDS_ENTERPRISE_ENROLLMENT_NETWORK_ENROLLMENT_ERROR);
  NotifyObservers(false);
}

// EnterpriseEnrollmentScreenHandler BaseScreenHandler implementation ---------

void EnterpriseEnrollmentScreenHandler::GetLocalizedStrings(
    base::DictionaryValue *localized_strings) {

  localized_strings->SetString(
      "loginHeader",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_LOGIN_HEADER));
  localized_strings->SetString(
      "loginExplain",
      l10n_util::GetStringFUTF16(IDS_ENTERPRISE_ENROLLMENT_LOGIN_EXPLAIN,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString(
      "cloudHeader",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CLOUD_HEADER));
  localized_strings->SetString(
      "cloudExplain",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CLOUD_EXPLAIN));
  localized_strings->SetString(
      "accesscontrolHeader",
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_ENROLLMENT_ACCESSCONTROL_HEADER));
  localized_strings->SetString(
      "accesscontrolExplain",
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_ENROLLMENT_ACCESSCONTROL_EXPLAIN));
  localized_strings->SetString(
      "confirmationHeader",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CONFIRMATION_HEADER));
  localized_strings->SetString(
      "confirmationMessage",
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_ENROLLMENT_CONFIRMATION_MESSAGE));
  localized_strings->SetString(
      "confirmationClose",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CONFIRMATION_CLOSE));

  localized_strings->SetString("invalidpasswordhelpurl", "");
  localized_strings->SetString("invalidaccessaccounturl", "");
  localized_strings->SetString("cannotaccessaccount", "");
  localized_strings->SetString("cannotaccessaccounturl", "");
  localized_strings->SetString("createaccount", "");
  localized_strings->SetString("createnewaccounturl", "");
  localized_strings->SetString("getaccesscodehelp", "");
  localized_strings->SetString("getaccesscodeurl", "");

  // None of the strings used here currently have sync-specific wording in
  // them.  We have a unit test to catch if that happens.
  localized_strings->SetString("introduction", "");
  localized_strings->SetString(
      "signinprefix", l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SIGNIN_PREFIX));
  localized_strings->SetString(
      "signinsuffix", l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SIGNIN_SUFFIX));
  localized_strings->SetString(
      "cannotbeblank", l10n_util::GetStringUTF16(IDS_SYNC_CANNOT_BE_BLANK));
  localized_strings->SetString("emaillabel",
                               l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_EMAIL));
  localized_strings->SetString(
      "passwordlabel", l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_PASSWORD));
  localized_strings->SetString(
      "invalidcredentials",
      l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
  localized_strings->SetString("signin",
                               l10n_util::GetStringUTF16(IDS_SYNC_SIGNIN));
  localized_strings->SetString(
      "couldnotconnect",
      l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_COULD_NOT_CONNECT));
  localized_strings->SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString(
      "settingup", l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_SETTING_UP));
  localized_strings->SetString("success",
                               l10n_util::GetStringUTF16(IDS_SYNC_SUCCESS));
  localized_strings->SetString(
      "errorsigningin", l10n_util::GetStringUTF16(IDS_SYNC_ERROR_SIGNING_IN));
  localized_strings->SetString(
      "captchainstructions",
      l10n_util::GetStringUTF16(IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS));
  localized_strings->SetString(
      "invalidaccesscode",
      l10n_util::GetStringUTF16(IDS_SYNC_INVALID_ACCESS_CODE_LABEL));
  localized_strings->SetString(
      "enteraccesscode",
      l10n_util::GetStringUTF16(IDS_SYNC_ENTER_ACCESS_CODE_LABEL));
}

void EnterpriseEnrollmentScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

// EnterpriseEnrollmentScreenHandler, private ----------------------------------

void EnterpriseEnrollmentScreenHandler::HandleSubmitAuth(
    const base::ListValue* value) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  // Value carries single list entry, which is a json-encoded string that
  // contains the auth parameters (see gaia_login.js).
  std::string json_params;
  if (!value->GetString(0, &json_params)) {
    NOTREACHED();
    return;
  }

  // Check the value type.
  scoped_ptr<base::Value> params(base::JSONReader::Read(json_params, false));
  if (!params.get() || !params->IsType(base::Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    return;
  }

  // Read the parameters.
  base::DictionaryValue* params_dict =
      static_cast<base::DictionaryValue*>(params.get());
  std::string user;
  std::string pass;
  std::string captcha;
  std::string access_code;
  if (!params_dict->GetString("user", &user) ||
      !params_dict->GetString("pass", &pass) ||
      !params_dict->GetString("captcha", &captcha) ||
      !params_dict->GetString("access_code", &access_code)) {
    NOTREACHED();
    return;
  }

  controller_->OnAuthSubmitted(user, pass, captcha, access_code);
}

void EnterpriseEnrollmentScreenHandler::HandleCancelAuth(
    const base::ListValue* value) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  controller_->OnAuthCancelled();
}

void EnterpriseEnrollmentScreenHandler::HandleConfirmationClose(
    const base::ListValue* value) {
  if (!controller_) {
    NOTREACHED();
    return;
  }

  controller_->OnConfirmationClosed();
}

void EnterpriseEnrollmentScreenHandler::ShowError(int message_id) {
  base::DictionaryValue args;
  args.SetInteger("error", GoogleServiceAuthError::NONE);
  args.SetBoolean("editable_user", editable_user_);
  args.SetString("error_message", l10n_util::GetStringUTF16(message_id));
  UpdateGaiaLogin(args);
}

void EnterpriseEnrollmentScreenHandler::UpdateGaiaLogin(
    const base::DictionaryValue& args) {
  std::string json;
  base::JSONWriter::Write(&args, false, &json);

  RenderViewHost* render_view_host =
      web_ui_->tab_contents()->render_view_host();
  render_view_host->ExecuteJavascriptInWebFrame(
      ASCIIToUTF16("//iframe[@id='gaia-local-login']"),
      UTF8ToUTF16("showGaiaLogin(" + json + ");"));
}

}  // namespace chromeos
