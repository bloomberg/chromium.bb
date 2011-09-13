// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/sync/util/oauth.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
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
  if (!result->GetBoolean("syncAllDataTypes", &config->sync_everything))
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

  bool sync_search_engines;
  if (!result->GetBoolean("syncSearchEngines", &sync_search_engines))
    return false;
  if (sync_search_engines)
    config->data_types.insert(syncable::SEARCH_ENGINES);

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
  if (!result->GetBoolean("encryptAllData", &config->encrypt_all))
    return false;

  // Passphrase settings.
  bool have_passphrase;
  if (!result->GetBoolean("usePassphrase", &have_passphrase))
    return false;

  if (have_passphrase) {
    bool is_gaia;
    if (!result->GetBoolean("isGooglePassphrase", &is_gaia))
      return false;
    std::string passphrase;
    if (!result->GetString("passphrase", &passphrase))
      return false;
    // The user provided a passphrase - pass it off to SyncSetupFlow as either
    // the secondary or GAIA passphrase as appropriate.
    if (is_gaia) {
      config->set_gaia_passphrase = true;
      config->gaia_passphrase = passphrase;
    } else {
      config->set_secondary_passphrase = true;
      config->secondary_passphrase = passphrase;
    }
  }
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
  // This case is hit when the user performs a back navigation.
  if (flow_)
    flow_->OnDialogClosed("");
}

void SyncSetupHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings);
}

void SyncSetupHandler::GetStaticLocalizedValues(
    DictionaryValue* localized_strings) {
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
  localized_strings->SetString(
      "passphraseRecover",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_RECOVER,
                      ASCIIToUTF16(google_util::StringAppendGoogleLocaleParam(
                          chrome::kSyncGoogleDashboardURL))));

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
    { "syncAllDataTypes", IDS_SYNC_EVERYTHING },
    { "chooseDataTypes", IDS_SYNC_CHOOSE_DATATYPES },
    { "bookmarks", IDS_SYNC_DATATYPE_BOOKMARKS },
    { "preferences", IDS_SYNC_DATATYPE_PREFERENCES },
    { "autofill", IDS_SYNC_DATATYPE_AUTOFILL },
    { "themes", IDS_SYNC_DATATYPE_THEMES },
    { "passwords", IDS_SYNC_DATATYPE_PASSWORDS },
    { "extensions", IDS_SYNC_DATATYPE_EXTENSIONS },
    { "typedURLs", IDS_SYNC_DATATYPE_TYPED_URLS },
    { "apps", IDS_SYNC_DATATYPE_APPS },
    { "searchEngines", IDS_SYNC_DATATYPE_SEARCH_ENGINES },
    { "openTabs", IDS_SYNC_DATATYPE_TABS },
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
    { "customizeLinkLabel", IDS_SYNC_CUSTOMIZE_LINK_LABEL },
    { "confirmSyncPreferences", IDS_SYNC_CONFIRM_SYNC_PREFERENCES },
    { "syncEverything", IDS_SYNC_SYNC_EVERYTHING },
    { "useDefaultSettings", IDS_SYNC_USE_DEFAULT_SETTINGS },
    { "passphraseSectionTitle", IDS_SYNC_PASSPHRASE_SECTION_TITLE },
    { "privacyDashboardLink", IDS_SYNC_PRIVACY_DASHBOARD_LINK_LABEL },
    { "enterPassphraseTitle", IDS_SYNC_ENTER_PASSPHRASE_TITLE },
    { "enterPassphraseBody", IDS_SYNC_ENTER_PASSPHRASE_BODY },
    { "enterOtherPassphraseBody", IDS_SYNC_ENTER_OTHER_PASSPHRASE_BODY },
    { "enterGooglePassphraseBody", IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY },
    { "passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL },
    { "incorrectPassphrase", IDS_SYNC_INCORRECT_PASSPHRASE },
    { "passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING },
    { "cancelWarningHeader", IDS_SYNC_PASSPHRASE_CANCEL_WARNING_HEADER },
    { "cancelWarning", IDS_SYNC_PASSPHRASE_CANCEL_WARNING },
    { "yes", IDS_SYNC_PASSPHRASE_CANCEL_YES },
    { "no", IDS_SYNC_PASSPHRASE_CANCEL_NO },
    { "sectionExplicitMessagePrefix", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_PREFIX },
    { "sectionExplicitMessagePostfix",
        IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_POSTFIX },
    { "encryptedDataTypesTitle", IDS_SYNC_ENCRYPTION_DATA_TYPES_TITLE },
    { "encryptSensitiveOption", IDS_SYNC_ENCRYPT_SENSITIVE_DATA },
    { "encryptAllOption", IDS_SYNC_ENCRYPT_ALL_DATA },
    { "encryptAllOption", IDS_SYNC_ENCRYPT_ALL_DATA },
    { "aspWarningText", IDS_SYNC_ASP_PASSWORD_WARNING_TEXT },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void SyncSetupHandler::Initialize() {
}

void SyncSetupHandler::OnGetOAuthTokenSuccess(const std::string& oauth_token) {
  flow_->OnUserSubmittedOAuth(oauth_token);
}

void SyncSetupHandler::OnGetOAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  CloseSyncSetup();
}


void SyncSetupHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("SyncSetupDidClosePage",
      NewCallback(this, &SyncSetupHandler::OnDidClosePage));
  web_ui_->RegisterMessageCallback("SyncSetupSubmitAuth",
      NewCallback(this, &SyncSetupHandler::HandleSubmitAuth));
  web_ui_->RegisterMessageCallback("SyncSetupConfigure",
      NewCallback(this, &SyncSetupHandler::HandleConfigure));
  web_ui_->RegisterMessageCallback("SyncSetupPassphrase",
      NewCallback(this, &SyncSetupHandler::HandlePassphraseEntry));
  web_ui_->RegisterMessageCallback("SyncSetupPassphraseCancel",
      NewCallback(this, &SyncSetupHandler::HandlePassphraseCancel));
  web_ui_->RegisterMessageCallback("SyncSetupAttachHandler",
      NewCallback(this, &SyncSetupHandler::HandleAttachHandler));
  web_ui_->RegisterMessageCallback("SyncSetupShowErrorUI",
      NewCallback(this, &SyncSetupHandler::HandleShowErrorUI));
  web_ui_->RegisterMessageCallback("SyncSetupShowSetupUI",
      NewCallback(this, &SyncSetupHandler::HandleShowSetupUI));
}

// Ideal(?) solution here would be to mimic the ClientLogin overlay.  Since
// this UI must render an external URL, that overlay cannot be used directly.
// The current implementation is functional, but fails asthetically.
// TODO(rickcam): Bug 90711: Update UI for OAuth sign-in flow
void SyncSetupHandler::ShowOAuthLogin() {
  DCHECK(browser_sync::IsUsingOAuth());

  Profile* profile = Profile::FromWebUI(web_ui_);
  oauth_login_.reset(new GaiaOAuthFetcher(this,
                                          profile->GetRequestContext(),
                                          profile,
                                          GaiaConstants::kSyncServiceOAuth));
  oauth_login_->SetAutoFetchLimit(GaiaOAuthFetcher::OAUTH1_REQUEST_TOKEN);
  oauth_login_->StartGetOAuthToken();
}

void SyncSetupHandler::ShowGaiaLogin(const DictionaryValue& args) {
  DCHECK(!browser_sync::IsUsingOAuth());
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

void SyncSetupHandler::Focus() {
  static_cast<RenderViewHostDelegate*>(web_ui_->tab_contents())->Activate();
}

void SyncSetupHandler::OnDidClosePage(const ListValue* args) {
  CloseSyncSetup();
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

void SyncSetupHandler::HandleAttachHandler(const ListValue* args) {
  OpenSyncSetup();
}

void SyncSetupHandler::HandleShowErrorUI(const ListValue* args) {
  DCHECK(!flow_);

  Profile* profile = Profile::FromWebUI(web_ui_);
  ProfileSyncService* service = profile->GetProfileSyncService();
  DCHECK(service);

  service->get_wizard().Step(SyncSetupWizard::NONFATAL_ERROR);

  // Show the Sync Setup page.
  if (service->get_wizard().IsVisible()) {
    service->get_wizard().Focus();
  } else {
    StringValue page("syncSetup");
    web_ui_->CallJavascriptFunction("OptionsPage.navigateToPage", page);
  }
}

void SyncSetupHandler::HandleShowSetupUI(const ListValue* args) {
  DCHECK(!flow_);
  ShowSetupUI();
}

void SyncSetupHandler::CloseSyncSetup() {
  if (flow_) {
    flow_->OnDialogClosed(std::string());
    flow_ = NULL;
  }
}

void SyncSetupHandler::OpenSyncSetup() {
  DCHECK(web_ui_);
  DCHECK(!flow_);

  Profile* profile = Profile::FromWebUI(web_ui_);
  ProfileSyncService* service = profile->GetProfileSyncService();
  if (!service) {
    // If there's no sync service, the user tried to manually invoke a syncSetup
    // URL, but sync features are disabled.  We need to close the overlay for
    // this (rare) case.
    web_ui_->CallJavascriptFunction("OptionsPage.closeOverlay");
    return;
  }

  // If the wizard is not visible, step into the appropriate UI state.
  if (!service->get_wizard().IsVisible())
    ShowSetupUI();

  // The SyncSetupFlow will set itself as the |flow_|.
  if (!service->get_wizard().AttachSyncSetupHandler(this)) {
    // If attach fails, a wizard is already activated and attached to a flow
    // handler.
    web_ui_->CallJavascriptFunction("OptionsPage.closeOverlay");
    service->get_wizard().Focus();
  }
}
