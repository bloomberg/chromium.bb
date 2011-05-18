// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/sync_setup_handler.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;

namespace {

// TODO(jhawkins): Move these to url_constants.h.
const char* kInvalidPasswordHelpUrl =
    "http://www.google.com/support/accounts/bin/answer.py?ctx=ch&answer=27444";
const char* kCanNotAccessAccountUrl =
    "http://www.google.com/support/accounts/bin/answer.py?answer=48598";
#if defined(OS_CHROMEOS)
const char* kEncryptionHelpUrl =
    "http://www.google.com/support/chromeos/bin/answer.py?answer=1181035";
#else
const char* kEncryptionHelpUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=1181035";
#endif
const char* kCreateNewAccountUrl =
    "https://www.google.com/accounts/NewAccount?service=chromiumsync";

bool GetAuthData(const std::string& json,
                 std::string* username,
                 std::string* password,
                 std::string* captcha,
                 std::string* access_code) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString("user", username) ||
      !result->GetString("pass", password) ||
      !result->GetString("captcha", captcha) ||
      !result->GetString("access_code", access_code)) {
      return false;
  }
  return true;
}

bool GetConfiguration(const std::string& json, SyncConfiguration* config) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetBoolean("keepEverythingSynced", &config->sync_everything))
    return false;

  // These values need to be kept in sync with where they are written in
  // choose_datatypes.html.
  bool sync_bookmarks;
  if (!result->GetBoolean("syncBookmarks", &sync_bookmarks))
    return false;
  if (sync_bookmarks)
    config->data_types.insert(syncable::BOOKMARKS);

  bool sync_preferences;
  if (!result->GetBoolean("syncPreferences", &sync_preferences))
    return false;
  if (sync_preferences)
    config->data_types.insert(syncable::PREFERENCES);

  bool sync_themes;
  if (!result->GetBoolean("syncThemes", &sync_themes))
    return false;
  if (sync_themes)
    config->data_types.insert(syncable::THEMES);

  bool sync_passwords;
  if (!result->GetBoolean("syncPasswords", &sync_passwords))
    return false;
  if (sync_passwords)
    config->data_types.insert(syncable::PASSWORDS);

  bool sync_autofill;
  if (!result->GetBoolean("syncAutofill", &sync_autofill))
    return false;
  if (sync_autofill)
    config->data_types.insert(syncable::AUTOFILL);

  bool sync_extensions;
  if (!result->GetBoolean("syncExtensions", &sync_extensions))
    return false;
  if (sync_extensions)
    config->data_types.insert(syncable::EXTENSIONS);

  bool sync_typed_urls;
  if (!result->GetBoolean("syncTypedUrls", &sync_typed_urls))
    return false;
  if (sync_typed_urls)
    config->data_types.insert(syncable::TYPED_URLS);

  bool sync_sessions;
  if (!result->GetBoolean("syncSessions", &sync_sessions))
    return false;
  if (sync_sessions)
    config->data_types.insert(syncable::SESSIONS);

  bool sync_apps;
  if (!result->GetBoolean("syncApps", &sync_apps))
    return false;
  if (sync_apps)
    config->data_types.insert(syncable::APPS);

  // Encryption settings.
  if (!result->GetBoolean("usePassphrase", &config->use_secondary_passphrase))
    return false;
  if (config->use_secondary_passphrase &&
      !result->GetString("passphrase", &config->secondary_passphrase))
    return false;

  return true;
}

bool GetPassphrase(const std::string& json, std::string* passphrase) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  return result->GetString("passphrase", passphrase);
}

}  // namespace

SyncSetupHandler::SyncSetupHandler() : flow_(NULL) {
}

SyncSetupHandler::~SyncSetupHandler() {
}

void SyncSetupHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(
      "invalidPasswordHelpURL",
      google_util::StringAppendGoogleLocaleParam(kInvalidPasswordHelpUrl));
  localized_strings->SetString(
      "cannotAccessAccountURL",
      google_util::StringAppendGoogleLocaleParam(kCanNotAccessAccountUrl));
  localized_strings->SetString(
      "createNewAccountURL",
      google_util::StringAppendGoogleLocaleParam(kCreateNewAccountUrl));
  localized_strings->SetString(
      "introduction",
      GetStringFUTF16(IDS_SYNC_LOGIN_INTRODUCTION,
                      GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString(
      "chooseDataTypesInstructions",
      GetStringFUTF16(IDS_SYNC_CHOOSE_DATATYPES_INSTRUCTIONS,
                      GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString(
      "encryptionInstructions",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_INSTRUCTIONS,
                      GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString(
      "encryptionHelpURL",
      google_util::StringAppendGoogleLocaleParam(kEncryptionHelpUrl));
  localized_strings->SetString(
      "passphraseEncryptionMessage",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_ENCRYPTION_MESSAGE,
                      GetStringUTF16(IDS_PRODUCT_NAME)));

  static OptionsStringResource resources[] = {
    { "syncSetupOverlayTitle", IDS_SYNC_SETUP_TITLE },
    { "syncSetupConfigureTitle", IDS_SYNC_SETUP_CONFIGURE_TITLE },
    { "signinPrefix", IDS_SYNC_LOGIN_SIGNIN_PREFIX },
    { "signinSuffix", IDS_SYNC_LOGIN_SIGNIN_SUFFIX },
    { "cannotBeBlank", IDS_SYNC_CANNOT_BE_BLANK },
    { "emailLabel", IDS_SYNC_LOGIN_EMAIL },
    { "passwordLabel", IDS_SYNC_LOGIN_PASSWORD },
    { "invalidCredentials", IDS_SYNC_INVALID_USER_CREDENTIALS },
    { "signin", IDS_SYNC_SIGNIN },
    { "couldNotConnect", IDS_SYNC_LOGIN_COULD_NOT_CONNECT },
    { "cannotAccessAccount", IDS_SYNC_CANNOT_ACCESS_ACCOUNT },
    { "createAccount", IDS_SYNC_CREATE_ACCOUNT },
    { "cancel", IDS_CANCEL },
    { "settingUp", IDS_SYNC_LOGIN_SETTING_UP },
    { "errorSigningIn", IDS_SYNC_ERROR_SIGNING_IN },
    { "captchaInstructions", IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS },
    { "invalidAccessCode", IDS_SYNC_INVALID_ACCESS_CODE_LABEL },
    { "enterAccessCode", IDS_SYNC_ENTER_ACCESS_CODE_LABEL },
    { "getAccessCodeHelp", IDS_SYNC_ACCESS_CODE_HELP_LABEL },
    { "getAccessCodeURL", IDS_SYNC_GET_ACCESS_CODE_URL },
    { "keepEverythingSynced", IDS_SYNC_EVERYTHING },
    { "chooseDataTypes", IDS_SYNC_CHOOSE_DATATYPES },
    { "bookmarks", IDS_SYNC_DATATYPE_BOOKMARKS },
    { "preferences", IDS_SYNC_DATATYPE_PREFERENCES },
    { "autofill", IDS_SYNC_DATATYPE_AUTOFILL },
    { "themes", IDS_SYNC_DATATYPE_THEMES },
    { "passwords", IDS_SYNC_DATATYPE_PASSWORDS },
    { "extensions", IDS_SYNC_DATATYPE_EXTENSIONS },
    { "typedURLs", IDS_SYNC_DATATYPE_TYPED_URLS },
    { "apps", IDS_SYNC_DATATYPE_APPS },
    { "foreignSessions", IDS_SYNC_DATATYPE_SESSIONS },
    { "syncZeroDataTypesError", IDS_SYNC_ZERO_DATA_TYPES_ERROR },
    { "abortedError", IDS_SYNC_SETUP_ABORTED_BY_PENDING_CLEAR },
    { "encryptAllLabel", IDS_SYNC_ENCRYPT_ALL_LABEL },
    { "googleOption", IDS_SYNC_PASSPHRASE_OPT_GOOGLE },
    { "explicitOption", IDS_SYNC_PASSPHRASE_OPT_EXPLICIT },
    { "sectionGoogleMessage", IDS_SYNC_PASSPHRASE_MSG_GOOGLE },
    { "sectionExplicitMessage", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT },
    { "passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL },
    { "confirmLabel", IDS_SYNC_CONFIRM_PASSPHRASE_LABEL },
    { "emptyErrorMessage", IDS_SYNC_EMPTY_PASSPHRASE_ERROR },
    { "mismatchErrorMessage", IDS_SYNC_PASSPHRASE_MISMATCH_ERROR },
    { "passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING },
    { "clearDataLink", IDS_SYNC_CLEAR_DATA_LINK },
    { "customizeLinkLabel", IDS_SYNC_CUSTOMIZE_LINK_LABEL },
    { "confirmSyncPreferences", IDS_SYNC_CONFIRM_SYNC_PREFERENCES },
    { "syncEverything", IDS_SYNC_SYNC_EVERYTHING },
    { "useDefaultSettings", IDS_SYNC_USE_DEFAULT_SETTINGS },
    { "passphraseSectionTitle", IDS_SYNC_PASSPHRASE_SECTION_TITLE },
    { "privacyDashboardLink", IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL },
    { "enterPassphraseTitle", IDS_SYNC_ENTER_PASSPHRASE_TITLE },
    { "enterPassphraseBody", IDS_SYNC_ENTER_PASSPHRASE_BODY },
    { "enterOtherPassphraseBody", IDS_SYNC_ENTER_OTHER_PASSPHRASE_BODY },
    { "passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL },
    { "incorrectPassphrase", IDS_SYNC_INCORRECT_PASSPHRASE },
    { "passphraseRecover", IDS_SYNC_PASSPHRASE_RECOVER },
    { "passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING },
    { "cancelWarningHeader", IDS_SYNC_PASSPHRASE_CANCEL_WARNING_HEADER },
    { "cancelWarning", IDS_SYNC_PASSPHRASE_CANCEL_WARNING },
    { "yes", IDS_SYNC_PASSPHRASE_CANCEL_YES },
    { "no", IDS_SYNC_PASSPHRASE_CANCEL_NO },
    { "sectionExplicitMessagePrefix", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_PREFIX },
    { "sectionExplicitMessagePostfix",
        IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_POSTFIX },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void SyncSetupHandler::Initialize() {
}

void SyncSetupHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("didShowPage",
      NewCallback(this, &SyncSetupHandler::OnDidShowPage));
  web_ui_->RegisterMessageCallback("didClosePage",
      NewCallback(this, &SyncSetupHandler::OnDidClosePage));
  web_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &SyncSetupHandler::HandleSubmitAuth));
  web_ui_->RegisterMessageCallback("Configure",
      NewCallback(this, &SyncSetupHandler::HandleConfigure));
  web_ui_->RegisterMessageCallback("Passphrase",
      NewCallback(this, &SyncSetupHandler::HandlePassphraseEntry));
  web_ui_->RegisterMessageCallback("PassphraseCancel",
      NewCallback(this, &SyncSetupHandler::HandlePassphraseCancel));
}

void SyncSetupHandler::ShowGaiaLogin(const DictionaryValue& args) {
  StringValue page("login");
  web_ui_->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::ShowGaiaSuccessAndClose() {
  web_ui_->CallJavascriptFunction("SyncSetupOverlay.showSuccessAndClose");
}

void SyncSetupHandler::ShowGaiaSuccessAndSettingUp() {
  web_ui_->CallJavascriptFunction("SyncSetupOverlay.showSuccessAndSettingUp");
}

void SyncSetupHandler::ShowConfigure(const DictionaryValue& args) {
  StringValue page("configure");
  web_ui_->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::ShowPassphraseEntry(const DictionaryValue& args) {
  StringValue page("passphrase");
  web_ui_->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::ShowSettingUp() {
  StringValue page("settingUp");
  web_ui_->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page);
}

void SyncSetupHandler::ShowSetupDone(const std::wstring& user) {
  StringValue page("done");
  web_ui_->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page);
}

void SyncSetupHandler::SetFlow(SyncSetupFlow* flow) {
  flow_ = flow;
}

void SyncSetupHandler::OnDidShowPage(const ListValue* args) {
  DCHECK(web_ui_);

  ProfileSyncService* sync_service =
      web_ui_->GetProfile()->GetProfileSyncService();
  if (!sync_service)
    return;

  flow_ = sync_service->get_wizard().AttachSyncSetupHandler(this);
}

void SyncSetupHandler::OnDidClosePage(const ListValue* args) {
  if (flow_) {
    flow_->OnDialogClosed(std::string());
    flow_ = NULL;
  }
}

void SyncSetupHandler::HandleSubmitAuth(const ListValue* args) {
  std::string json;
  if (!args->GetString(0, &json)) {
    NOTREACHED() << "Could not read JSON argument";
    return;
  }

  if (json.empty())
    return;

  std::string username, password, captcha, access_code;
  if (!GetAuthData(json, &username, &password, &captcha, &access_code)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  if (flow_)
    flow_->OnUserSubmittedAuth(username, password, captcha, access_code);
}

void SyncSetupHandler::HandleConfigure(const ListValue* args) {
  std::string json;
  if (!args->GetString(0, &json)) {
    NOTREACHED() << "Could not read JSON argument";
    return;
  }
  if (json.empty()) {
    NOTREACHED();
    return;
  }

  SyncConfiguration configuration;
  if (!GetConfiguration(json, &configuration)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  DCHECK(flow_);
  flow_->OnUserConfigured(configuration);
}

void SyncSetupHandler::HandlePassphraseEntry(const ListValue* args) {
  std::string json;
  if (!args->GetString(0, &json)) {
    NOTREACHED() << "Could not read JSON argument";
    return;
  }

  if (json.empty())
    return;

  std::string passphrase;
  if (!GetPassphrase(json, &passphrase)) {
    // Couldn't understand what the page sent.  Indicates a programming error.
    NOTREACHED();
    return;
  }

  DCHECK(flow_);
  flow_->OnPassphraseEntry(passphrase);
}

void SyncSetupHandler::HandlePassphraseCancel(const ListValue* args) {
  DCHECK(flow_);
  flow_->OnPassphraseCancel();
}
