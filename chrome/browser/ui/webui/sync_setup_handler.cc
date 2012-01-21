// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/sync/util/oauth.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;

namespace {

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
    config->data_types.Put(syncable::BOOKMARKS);

  bool sync_preferences;
  if (!result->GetBoolean("syncPreferences", &sync_preferences))
    return false;
  if (sync_preferences)
    config->data_types.Put(syncable::PREFERENCES);

  bool sync_themes;
  if (!result->GetBoolean("syncThemes", &sync_themes))
    return false;
  if (sync_themes)
    config->data_types.Put(syncable::THEMES);

  bool sync_passwords;
  if (!result->GetBoolean("syncPasswords", &sync_passwords))
    return false;
  if (sync_passwords)
    config->data_types.Put(syncable::PASSWORDS);

  bool sync_autofill;
  if (!result->GetBoolean("syncAutofill", &sync_autofill))
    return false;
  if (sync_autofill)
    config->data_types.Put(syncable::AUTOFILL);

  bool sync_extensions;
  if (!result->GetBoolean("syncExtensions", &sync_extensions))
    return false;
  if (sync_extensions) {
    config->data_types.Put(syncable::EXTENSIONS);
    config->data_types.Put(syncable::EXTENSION_SETTINGS);
  }

  bool sync_typed_urls;
  if (!result->GetBoolean("syncTypedUrls", &sync_typed_urls))
    return false;
  if (sync_typed_urls)
    config->data_types.Put(syncable::TYPED_URLS);

  bool sync_sessions;
  if (!result->GetBoolean("syncSessions", &sync_sessions))
    return false;
  if (sync_sessions)
    config->data_types.Put(syncable::SESSIONS);

  bool sync_apps;
  if (!result->GetBoolean("syncApps", &sync_apps))
    return false;
  if (sync_apps) {
    config->data_types.Put(syncable::APPS);
    config->data_types.Put(syncable::APP_SETTINGS);
  }

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

string16 NormalizeUserName(const string16& user) {
  if (user.find_first_of(ASCIIToUTF16("@")) != string16::npos)
    return user;
  return user + ASCIIToUTF16("@") + ASCIIToUTF16(DEFAULT_SIGNIN_DOMAIN);
}

bool AreUserNamesEqual(const string16& user1, const string16& user2) {
  return NormalizeUserName(user1) == NormalizeUserName(user2);
}

}  // namespace

SyncSetupHandler::SyncSetupHandler(ProfileManager* profile_manager)
    : flow_(NULL),
      profile_manager_(profile_manager) {
}

SyncSetupHandler::~SyncSetupHandler() {
  // This case is hit when the user performs a back navigation.
  if (flow_)
    flow_->OnDialogClosed("");
}

void SyncSetupHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings, web_ui());
}

void SyncSetupHandler::GetStaticLocalizedValues(
    DictionaryValue* localized_strings,
    content::WebUI* web_ui) {
  DCHECK(localized_strings);

  localized_strings->SetString(
      "invalidPasswordHelpURL",
      google_util::StringAppendGoogleLocaleParam(
          chrome::kInvalidPasswordHelpURL));
  localized_strings->SetString(
      "cannotAccessAccountURL",
      google_util::StringAppendGoogleLocaleParam(
          chrome::kCanNotAccessAccountURL));
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
      google_util::StringAppendGoogleLocaleParam(
          chrome::kSyncEncryptionHelpURL));
  localized_strings->SetString(
      "passphraseEncryptionMessage",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_ENCRYPTION_MESSAGE,
                      GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString(
      "passphraseRecover",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_RECOVER,
                      ASCIIToUTF16(google_util::StringAppendGoogleLocaleParam(
                          chrome::kSyncGoogleDashboardURL))));
  localized_strings->SetString(
      "promoTitle",
      GetStringFUTF16(IDS_SYNC_PROMO_TITLE,
                      GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString(
      "promoMessageTitle",
      GetStringFUTF16(IDS_SYNC_PROMO_MESSAGE_TITLE,
                      GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  localized_strings->SetString(
      "syncEverythingHelpURL",
      google_util::StringAppendGoogleLocaleParam(
          chrome::kSyncEverythingLearnMoreURL));

  // The experimental body string only appears if we are on the launch page
  // version of the Sync Promo.
  int message_body_resource_id = IDS_SYNC_PROMO_MESSAGE_BODY_A;
  if (web_ui && SyncPromoUI::GetIsLaunchPageForSyncPromoURL(
      web_ui->GetWebContents()->GetURL())) {
    message_body_resource_id = sync_promo_trial::GetMessageBodyResID();
  }
  localized_strings->SetString(
      "promoMessageBody",
      GetStringUTF16(message_body_resource_id));

  std::string create_account_url = google_util::StringAppendGoogleLocaleParam(
      chrome::kSyncCreateNewAccountURL);
  string16 create_account = GetStringUTF16(IDS_SYNC_CREATE_ACCOUNT);
  create_account= UTF8ToUTF16("<a id='create-account-link' target='_blank' "
      "class='account-link' href='" + create_account_url + "'>") +
      create_account + UTF8ToUTF16("</a>");
  localized_strings->SetString("createAccountLinkHTML",
      GetStringFUTF16(IDS_SYNC_CREATE_ACCOUNT_PREFIX, create_account));

  static OptionsStringResource resources[] = {
    { "syncSetupOverlayTitle", IDS_SYNC_SETUP_TITLE },
    { "syncSetupConfigureTitle", IDS_SYNC_SETUP_CONFIGURE_TITLE },
    { "cannotBeBlank", IDS_SYNC_CANNOT_BE_BLANK },
    { "emailLabel", IDS_SYNC_LOGIN_EMAIL_NEW_LINE },
    { "passwordLabel", IDS_SYNC_LOGIN_PASSWORD_NEW_LINE },
    { "invalidCredentials", IDS_SYNC_INVALID_USER_CREDENTIALS },
    { "signin", IDS_SYNC_SIGNIN },
    { "couldNotConnect", IDS_SYNC_LOGIN_COULD_NOT_CONNECT },
    { "unrecoverableError", IDS_SYNC_UNRECOVERABLE_ERROR },
    { "errorLearnMore", IDS_LEARN_MORE },
    { "unrecoverableErrorHelpURL", IDS_SYNC_UNRECOVERABLE_ERROR_HELP_URL },
    { "cannotAccessAccount", IDS_SYNC_CANNOT_ACCESS_ACCOUNT },
    { "cancel", IDS_CANCEL },
    { "settingUp", IDS_SYNC_LOGIN_SETTING_UP },
    { "errorSigningIn", IDS_SYNC_ERROR_SIGNING_IN },
    { "signinHeader", IDS_SYNC_PROMO_SIGNIN_HEADER},
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
    { "openTabs", IDS_SYNC_DATATYPE_TABS },
    { "syncZeroDataTypesError", IDS_SYNC_ZERO_DATA_TYPES_ERROR },
    { "serviceUnavailableError", IDS_SYNC_SETUP_ABORTED_BY_PENDING_CLEAR },
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
    { "promoPageTitle", IDS_SYNC_PROMO_TAB_TITLE},
    { "promoSkipButton", IDS_SYNC_PROMO_SKIP_BUTTON},
    { "promoAdvanced", IDS_SYNC_PROMO_ADVANCED},
    { "promoLearnMoreShow", IDS_SYNC_PROMO_LEARN_MORE_SHOW},
    { "promoLearnMoreHide", IDS_SYNC_PROMO_LEARN_MORE_HIDE},
    { "promoInformation", IDS_SYNC_PROMO_INFORMATION},
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
  web_ui()->RegisterMessageCallback("SyncSetupDidClosePage",
      base::Bind(&SyncSetupHandler::OnDidClosePage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupSubmitAuth",
      base::Bind(&SyncSetupHandler::HandleSubmitAuth,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupConfigure",
      base::Bind(&SyncSetupHandler::HandleConfigure,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupPassphrase",
      base::Bind(&SyncSetupHandler::HandlePassphraseEntry,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupPassphraseCancel",
      base::Bind(&SyncSetupHandler::HandlePassphraseCancel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupAttachHandler",
      base::Bind(&SyncSetupHandler::HandleAttachHandler,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupShowErrorUI",
      base::Bind(&SyncSetupHandler::HandleShowErrorUI,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupShowSetupUI",
      base::Bind(&SyncSetupHandler::HandleShowSetupUI,
                 base::Unretained(this)));
}

// Ideal(?) solution here would be to mimic the ClientLogin overlay.  Since
// this UI must render an external URL, that overlay cannot be used directly.
// The current implementation is functional, but fails asthetically.
// TODO(rickcam): Bug 90711: Update UI for OAuth sign-in flow
void SyncSetupHandler::ShowOAuthLogin() {
  DCHECK(browser_sync::IsUsingOAuth());

  Profile* profile = Profile::FromWebUI(web_ui());
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
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::ShowGaiaSuccessAndClose() {
  web_ui()->CallJavascriptFunction("SyncSetupOverlay.showSuccessAndClose");
}

void SyncSetupHandler::ShowGaiaSuccessAndSettingUp() {
  web_ui()->CallJavascriptFunction("SyncSetupOverlay.showSuccessAndSettingUp");
}

void SyncSetupHandler::ShowConfigure(const DictionaryValue& args) {
  StringValue page("configure");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::ShowPassphraseEntry(const DictionaryValue& args) {
  StringValue page("passphrase");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::ShowSettingUp() {
  StringValue page("settingUp");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page);
}

void SyncSetupHandler::ShowSetupDone(const string16& user) {
  StringValue page("done");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page);

  // Suppress the sync promo once the user signs into sync. This way the user
  // doesn't see the sync promo even if they sign out of sync later on.
  SyncPromoUI::SetUserSkippedSyncPromo(Profile::FromWebUI(web_ui()));

  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncService* service = profile->GetProfileSyncService();
  if (!service->HasSyncSetupCompleted()) {
    FilePath profile_file_path = profile->GetPath();
    ProfileMetrics::LogProfileSyncSignIn(profile_file_path);
  }
}

void SyncSetupHandler::SetFlow(SyncSetupFlow* flow) {
  flow_ = flow;
}

void SyncSetupHandler::Focus() {
  web_ui()->GetWebContents()->GetRenderViewHost()->delegate()->Activate();
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

  string16 error_message;
  if (!IsLoginAuthDataValid(username, &error_message)) {
    ShowLoginErrorMessage(error_message);
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

  ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CUSTOMIZE);
  if (configuration.encrypt_all) {
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_ENCRYPT);
  }
  if (configuration.set_secondary_passphrase) {
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_PASSPHRASE);
  }
  if (!configuration.sync_everything) {
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CHOOSE);
  }
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

  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncService* service = profile->GetProfileSyncService();
  DCHECK(service);

  service->ShowErrorUI();
}

void SyncSetupHandler::HandleShowSetupUI(const ListValue* args) {
  DCHECK(!flow_);
  if (FocusExistingWizard()) {
    CloseOverlay();
    return;
  }

  StepWizardForShowSetupUI();
  ShowSetupUI();
}

void SyncSetupHandler::CloseSyncSetup() {
  if (flow_) {
    flow_->OnDialogClosed(std::string());
    flow_ = NULL;
  }
}

void SyncSetupHandler::OpenSyncSetup() {
  DCHECK(!flow_);

  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncService* service = profile->GetProfileSyncService();
  if (!service) {
    // If there's no sync service, the user tried to manually invoke a syncSetup
    // URL, but sync features are disabled.  We need to close the overlay for
    // this (rare) case.
    CloseOverlay();
    return;
  }

  // If the wizard is already visible, it must be attached to another flow
  // handler.
  if (FocusExistingWizard()) {
    CloseOverlay();
    return;
  }

  // The wizard must be stepped before attaching. Allow subclasses to step the
  // wizard to appropriate state.
  StepWizardForShowSetupUI();

  // Attach this as the sync setup handler, before calling ShowSetupUI().
  if (!service->get_wizard().AttachSyncSetupHandler(this)) {
    LOG(ERROR) << "SyncSetupHandler attach failed!";
    CloseOverlay();
    return;
  }

  ShowSetupUI();
}

// Private member functions.

bool SyncSetupHandler::FocusExistingWizard() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncService* service = profile->GetProfileSyncService();
  if (!service)
    return false;

  // If the wizard is already visible, focus it.
  if (service->get_wizard().IsVisible()) {
    service->get_wizard().Focus();
    return true;
  }
  return false;
}

void SyncSetupHandler::CloseOverlay() {
  web_ui()->CallJavascriptFunction("OptionsPage.closeOverlay");
}

bool SyncSetupHandler::IsLoginAuthDataValid(const std::string& username,
                                            string16* error_message) {
  // Happens during unit tests.
  if (!web_ui() || !profile_manager_)
    return true;

  if (username.empty())
    return true;

  // Check if the username is already in use by another profile.
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t current_profile_index = cache.GetIndexOfProfileWithPath(
      Profile::FromWebUI(web_ui())->GetPath());
  string16 username_utf16 = UTF8ToUTF16(username);

  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
    if (i != current_profile_index && AreUserNamesEqual(
        cache.GetUserNameOfProfileAtIndex(i), username_utf16)) {
      *error_message = l10n_util::GetStringUTF16(
          IDS_SYNC_USER_NAME_IN_USE_ERROR);
      return false;
    }
  }

  return true;
}

void SyncSetupHandler::ShowLoginErrorMessage(const string16& error_message) {
  DCHECK(flow_);
  DictionaryValue args;
  flow_->GetArgsForGaiaLogin(&args);
  args.SetString("error_message", error_message);
  ShowGaiaLogin(args);
}
