// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;

namespace {

// A structure which contains all the configuration information for sync.
struct SyncConfigInfo {
  SyncConfigInfo();
  ~SyncConfigInfo();

  bool encrypt_all;
  bool sync_everything;
  syncer::ModelTypeSet data_types;
  std::string passphrase;
  bool passphrase_is_gaia;
};

SyncConfigInfo::SyncConfigInfo()
    : encrypt_all(false),
      sync_everything(false),
      passphrase_is_gaia(false) {
}

SyncConfigInfo::~SyncConfigInfo() {}

// Note: The order of these types must match the ordering of
// the respective types in ModelType
const char* kDataTypeNames[] = {
  "bookmarks",
  "preferences",
  "passwords",
  "autofill",
  "themes",
  "typedUrls",
  "extensions",
  "sessions",
  "apps"
};

COMPILE_ASSERT(25 == syncer::MODEL_TYPE_COUNT,
               update_kDataTypeNames_to_match_UserSelectableTypes);

typedef std::map<syncer::ModelType, const char*> ModelTypeNameMap;

ModelTypeNameMap GetSelectableTypeNameMap() {
  ModelTypeNameMap type_names;
  syncer::ModelTypeSet type_set = syncer::UserSelectableTypes();
  syncer::ModelTypeSet::Iterator it = type_set.First();
  DCHECK_EQ(arraysize(kDataTypeNames), type_set.Size());
  for (size_t i = 0; i < arraysize(kDataTypeNames) && it.Good();
       ++i, it.Inc()) {
    type_names[it.Get()] = kDataTypeNames[i];
  }
  return type_names;
}

static const char kDefaultSigninDomain[] = "gmail.com";

bool GetAuthData(const std::string& json,
                 std::string* username,
                 std::string* password,
                 std::string* captcha,
                 std::string* otp,
                 std::string* access_code) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json));
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
  if (!result->GetString("user", username) ||
      !result->GetString("pass", password) ||
      !result->GetString("captcha", captcha) ||
      !result->GetString("otp", otp) ||
      !result->GetString("accessCode", access_code)) {
      return false;
  }
  return true;
}

bool GetConfiguration(const std::string& json, SyncConfigInfo* config) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json));
  DictionaryValue* result;
  if (!parsed_value.get() || !parsed_value->GetAsDictionary(&result)) {
    DLOG(ERROR) << "GetConfiguration() not passed a Dictionary";
    return false;
  }

  if (!result->GetBoolean("syncAllDataTypes", &config->sync_everything)) {
    DLOG(ERROR) << "GetConfiguration() not passed a syncAllDataTypes value";
    return false;
  }

  ModelTypeNameMap type_names = GetSelectableTypeNameMap();

  for (ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    std::string key_name = it->second + std::string("Synced");
    bool sync_value;
    if (!result->GetBoolean(key_name, &sync_value)) {
      DLOG(ERROR) << "GetConfiguration() not passed a value for " << key_name;
      return false;
    }
    if (sync_value)
      config->data_types.Put(it->first);
  }

  // Encryption settings.
  if (!result->GetBoolean("encryptAllData", &config->encrypt_all)) {
    DLOG(ERROR) << "GetConfiguration() not passed a value for encryptAllData";
    return false;
  }

  // Passphrase settings.
  bool have_passphrase;
  if (!result->GetBoolean("usePassphrase", &have_passphrase)) {
    DLOG(ERROR) << "GetConfiguration() not passed a usePassphrase value";
    return false;
  }

  if (have_passphrase) {
    if (!result->GetBoolean("isGooglePassphrase",
                            &config->passphrase_is_gaia)) {
      DLOG(ERROR) << "GetConfiguration() not passed isGooglePassphrase value";
      return false;
    }
    if (!result->GetString("passphrase", &config->passphrase)) {
      DLOG(ERROR) << "GetConfiguration() not passed a passphrase value";
      return false;
    }
  }
  return true;
}

string16 NormalizeUserName(const string16& user) {
  if (user.find_first_of(ASCIIToUTF16("@")) != string16::npos)
    return user;
  return user + ASCIIToUTF16("@") + ASCIIToUTF16(kDefaultSigninDomain);
}

bool AreUserNamesEqual(const string16& user1, const string16& user2) {
  return NormalizeUserName(user1) == NormalizeUserName(user2);
}

bool IsKeystoreEncryptionEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSyncKeystoreEncryption);
}

void BringTabToFront(WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(web_contents);
      if (index != TabStripModel::kNoTab)
        tab_strip_model->ActivateTabAt(index, false);
    }
  }
}

}  // namespace

SyncSetupHandler::SyncSetupHandler(ProfileManager* profile_manager)
    : configuring_sync_(false),
      profile_manager_(profile_manager),
      last_signin_error_(GoogleServiceAuthError::NONE),
      retry_on_signin_failure_(true),
      active_gaia_signin_tab_(NULL) {
}

SyncSetupHandler::~SyncSetupHandler() {
  // Just exit if running unit tests (no actual WebUI is attached).
  if (!web_ui())
    return;

  // This case is hit when the user performs a back navigation.
  CloseSyncSetup();
}

void SyncSetupHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings, web_ui());
}

void SyncSetupHandler::GetStaticLocalizedValues(
    DictionaryValue* localized_strings,
    content::WebUI* web_ui) {
  DCHECK(localized_strings);

  localized_strings->SetString(
      "invalidPasswordHelpURL", chrome::kInvalidPasswordHelpURL);
  localized_strings->SetString(
      "cannotAccessAccountURL", chrome::kCanNotAccessAccountURL);
  string16 product_name(GetStringUTF16(IDS_PRODUCT_NAME));
  localized_strings->SetString(
      "introduction",
      GetStringFUTF16(IDS_SYNC_LOGIN_INTRODUCTION, product_name));
  localized_strings->SetString(
      "chooseDataTypesInstructions",
      GetStringFUTF16(IDS_SYNC_CHOOSE_DATATYPES_INSTRUCTIONS, product_name));
  localized_strings->SetString(
      "encryptionInstructions",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_INSTRUCTIONS, product_name));
  localized_strings->SetString(
      "encryptionHelpURL", chrome::kSyncEncryptionHelpURL);
  localized_strings->SetString(
      "passphraseEncryptionMessage",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_ENCRYPTION_MESSAGE, product_name));
  localized_strings->SetString(
      "encryptionSectionMessage",
      GetStringFUTF16(IDS_SYNC_ENCRYPTION_SECTION_MESSAGE, product_name));
  localized_strings->SetString(
      "passphraseRecover",
      GetStringFUTF16(IDS_SYNC_PASSPHRASE_RECOVER,
                      ASCIIToUTF16(google_util::StringAppendGoogleLocaleParam(
                          chrome::kSyncGoogleDashboardURL))));
  localized_strings->SetString("stopSyncingExplanation",
      l10n_util::GetStringFUTF16(
          IDS_SYNC_STOP_SYNCING_EXPLANATION_LABEL,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          ASCIIToUTF16(google_util::StringAppendGoogleLocaleParam(
              chrome::kSyncGoogleDashboardURL))));
  localized_strings->SetString("stopSyncingTitle",
      l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_DIALOG_TITLE));
  localized_strings->SetString("stopSyncingConfirm",
        l10n_util::GetStringUTF16(IDS_SYNC_STOP_SYNCING_CONFIRM_BUTTON_LABEL));

  bool is_start_page = false;
  if (web_ui) {
    SyncPromoUI::Source source = SyncPromoUI::GetSourceForSyncPromoURL(
        web_ui->GetWebContents()->GetURL());
    is_start_page = source == SyncPromoUI::SOURCE_START_PAGE;
  }
  int title_id = is_start_page ? IDS_SYNC_PROMO_TITLE_SHORT :
                                 IDS_SYNC_PROMO_TITLE_EXISTING_USER;
  string16 short_product_name(GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  localized_strings->SetString(
      "promoTitle", GetStringFUTF16(title_id, short_product_name));

  localized_strings->SetString(
      "syncEverythingHelpURL", chrome::kSyncEverythingLearnMoreURL);
  localized_strings->SetString(
      "syncErrorHelpURL", chrome::kSyncErrorsHelpURL);

  std::string create_account_url = google_util::StringAppendGoogleLocaleParam(
      chrome::kSyncCreateNewAccountURL);
  string16 create_account = GetStringUTF16(IDS_SYNC_CREATE_ACCOUNT);
  create_account= UTF8ToUTF16("<a id='create-account-link' target='_blank' "
      "class='account-link' href='" + create_account_url + "'>") +
      create_account + UTF8ToUTF16("</a>");
  localized_strings->SetString("createAccountLinkHTML",
      GetStringFUTF16(IDS_SYNC_CREATE_ACCOUNT_PREFIX, create_account));

  string16 sync_benefits_url(
      UTF8ToUTF16(google_util::StringAppendGoogleLocaleParam(
          chrome::kSyncLearnMoreURL)));
  localized_strings->SetString("promoLearnMoreURL", sync_benefits_url);

  static OptionsStringResource resources[] = {
    { "syncSetupConfigureTitle", IDS_SYNC_SETUP_CONFIGURE_TITLE },
    { "syncSetupSpinnerTitle", IDS_SYNC_SETUP_SPINNER_TITLE },
    { "syncSetupTimeoutTitle", IDS_SYNC_SETUP_TIME_OUT_TITLE },
    { "syncSetupTimeoutContent", IDS_SYNC_SETUP_TIME_OUT_CONTENT },
    { "cannotBeBlank", IDS_SYNC_CANNOT_BE_BLANK },
    { "emailLabel", IDS_SYNC_LOGIN_EMAIL_NEW_LINE },
    { "passwordLabel", IDS_SYNC_LOGIN_PASSWORD_NEW_LINE },
    { "invalidCredentials", IDS_SYNC_INVALID_USER_CREDENTIALS },
    { "differentEmail", IDS_SYNC_DIFFERENT_EMAIL },
    { "signin", IDS_SYNC_SIGNIN },
    { "couldNotConnect", IDS_SYNC_LOGIN_COULD_NOT_CONNECT },
    { "unrecoverableError", IDS_SYNC_UNRECOVERABLE_ERROR },
    { "errorLearnMore", IDS_LEARN_MORE },
    { "unrecoverableErrorHelpURL", IDS_SYNC_UNRECOVERABLE_ERROR_HELP_URL },
    { "cannotAccessAccount", IDS_SYNC_CANNOT_ACCESS_ACCOUNT },
    { "cancel", IDS_CANCEL },
    { "loginSuccess", IDS_SYNC_SUCCESS },
    { "settingUp", IDS_SYNC_LOGIN_SETTING_UP },
    { "errorSigningIn", IDS_SYNC_ERROR_SIGNING_IN },
    { "signinHeader", IDS_SYNC_PROMO_SIGNIN_HEADER},
    { "captchaInstructions", IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS },
    { "invalidAccessCode", IDS_SYNC_INVALID_ACCESS_CODE_LABEL },
    { "enterAccessCode", IDS_SYNC_ENTER_ACCESS_CODE_LABEL },
    { "getAccessCodeHelp", IDS_SYNC_ACCESS_CODE_HELP_LABEL },
    { "getAccessCodeURL", IDS_SYNC_GET_ACCESS_CODE_URL },
    { "invalidOtp", IDS_SYNC_INVALID_OTP_LABEL },
    { "enterOtp", IDS_SYNC_ENTER_OTP_LABEL },
    { "getOtpHelp", IDS_SYNC_OTP_HELP_LABEL },
    { "getOtpURL", IDS_SYNC_GET_OTP_URL },
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
    { "enterPassphraseTitle", IDS_SYNC_ENTER_PASSPHRASE_TITLE },
    { "enterPassphraseBody", IDS_SYNC_ENTER_PASSPHRASE_BODY },
    { "enterGooglePassphraseBody", IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY },
    { "passphraseLabel", IDS_SYNC_PASSPHRASE_LABEL },
    { "incorrectPassphrase", IDS_SYNC_INCORRECT_PASSPHRASE },
    { "passphraseWarning", IDS_SYNC_PASSPHRASE_WARNING },
    { "yes", IDS_SYNC_PASSPHRASE_CANCEL_YES },
    { "no", IDS_SYNC_PASSPHRASE_CANCEL_NO },
    { "sectionExplicitMessagePrefix", IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_PREFIX },
    { "sectionExplicitMessagePostfix",
        IDS_SYNC_PASSPHRASE_MSG_EXPLICIT_POSTFIX },
    { "encryptedDataTypesTitle", IDS_SYNC_ENCRYPTION_DATA_TYPES_TITLE },
    { "encryptSensitiveOption", IDS_SYNC_ENCRYPT_SENSITIVE_DATA },
    { "encryptAllOption", IDS_SYNC_ENCRYPT_ALL_DATA },
    { "aspWarningText", IDS_SYNC_ASP_PASSWORD_WARNING_TEXT },
    { "promoPageTitle", IDS_SYNC_PROMO_TAB_TITLE },
    { "promoSkipButton", IDS_SYNC_PROMO_SKIP_BUTTON },
    { "promoAdvanced", IDS_SYNC_PROMO_ADVANCED },
    { "promoLearnMore", IDS_LEARN_MORE },
    { "promoTitleShort", IDS_SYNC_PROMO_MESSAGE_TITLE_SHORT },
    { "encryptionSectionTitle", IDS_SYNC_ENCRYPTION_SECTION_TITLE },
    { "basicEncryptionOption", IDS_SYNC_BASIC_ENCRYPTION_DATA },
    { "fullEncryptionOption", IDS_SYNC_FULL_ENCRYPTION_DATA },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "syncSetupOverlay", IDS_SYNC_SETUP_TITLE);
}

void SyncSetupHandler::DisplayConfigureSync(bool show_advanced,
                                            bool passphrase_failed) {
  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->sync_initialized()) {
    // When user tries to setup sync while the sync backend is not initialized,
    // kick the sync backend and wait for it to be ready and show spinner until
    // the backend gets ready.
    retry_on_signin_failure_ = false;

    service->UnsuppressAndStart();
    DisplaySpinner();

    // To listen to the token available notifications, start SigninTracker.
    signin_tracker_.reset(
        new SigninTracker(GetProfile(),
                          this,
                          SigninTracker::SERVICES_INITIALIZING));
    return;
  }

  // Should only be called if user is signed in, so no longer need our
  // SigninTracker.
  signin_tracker_.reset();
  configuring_sync_ = true;
  DCHECK(service->sync_initialized()) <<
      "Cannot configure sync until the sync backend is initialized";

  // Setup args for the sync configure screen:
  //   showSyncEverythingPage: false to skip directly to the configure screen
  //   syncAllDataTypes: true if the user wants to sync everything
  //   <data_type>Registered: true if the associated data type is supported
  //   <data_type>Synced: true if the user wants to sync that specific data type
  //   encryptionEnabled: true if sync supports encryption
  //   encryptAllData: true if user wants to encrypt all data (not just
  //       passwords)
  //   usePassphrase: true if the data is encrypted with a secondary passphrase
  //   show_passphrase: true if a passphrase is needed to decrypt the sync data
  DictionaryValue args;

  // Tell the UI layer which data types are registered/enabled by the user.
  const syncer::ModelTypeSet registered_types =
      service->GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      service->GetPreferredDataTypes();
  ModelTypeNameMap type_names = GetSelectableTypeNameMap();
  for (ModelTypeNameMap::const_iterator it = type_names.begin();
       it != type_names.end(); ++it) {
    syncer::ModelType sync_type = it->first;
    const std::string key_name = it->second;
    args.SetBoolean(key_name + "Registered",
                    registered_types.Has(sync_type));
    args.SetBoolean(key_name + "Synced", preferred_types.Has(sync_type));
  }
  browser_sync::SyncPrefs sync_prefs(GetProfile()->GetPrefs());
  args.SetBoolean("passphraseFailed", passphrase_failed);
  args.SetBoolean("showSyncEverythingPage", !show_advanced);
  args.SetBoolean("syncAllDataTypes", sync_prefs.HasKeepEverythingSynced());
  args.SetBoolean("encryptAllData", service->EncryptEverythingEnabled());

  // We call IsPassphraseRequired() here, instead of calling
  // IsPassphraseRequiredForDecryption(), because we want to show the passphrase
  // UI even if no encrypted data types are enabled.
  args.SetBoolean("showPassphrase", service->IsPassphraseRequired());
  // Keystore encryption is behind a flag. Only show the new encryption settings
  // if keystore encryption is enabled.
  args.SetBoolean("keystoreEncryptionEnabled", IsKeystoreEncryptionEnabled());

  // Set the proper encryption settings messages if keystore encryption is
  // enabled.
  if (IsKeystoreEncryptionEnabled()) {
    // To distinguish between FROZEN_IMPLICIT_PASSPHRASE and CUSTOM_PASSPHRASE
    // we only set usePassphrase for CUSTOM_PASSPHRASE.
    args.SetBoolean("usePassphrase",
                    service->GetPassphraseType() == syncer::CUSTOM_PASSPHRASE);
    base::Time passphrase_time = service->GetExplicitPassphraseTime();
    syncer::PassphraseType passphrase_type = service->GetPassphraseType();
    if (!passphrase_time.is_null()) {
      string16 passphrase_time_str = base::TimeFormatShortDate(passphrase_time);
      args.SetString(
          "enterPassphraseBody",
          GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
                          passphrase_time_str));
      args.SetString(
          "enterGooglePassphraseBody",
          GetStringFUTF16(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
                          passphrase_time_str));
      switch (passphrase_type) {
        case syncer::FROZEN_IMPLICIT_PASSPHRASE:
          args.SetString(
              "fullEncryptionBody",
              GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_GOOGLE_WITH_DATE,
                              passphrase_time_str));
          break;
        case syncer::CUSTOM_PASSPHRASE:
          args.SetString(
              "fullEncryptionBody",
              GetStringFUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM_WITH_DATE,
                              passphrase_time_str));
          break;
        default:
          args.SetString(
              "fullEncryptionBody",
              GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
          break;
      }
    } else if (passphrase_type == syncer::CUSTOM_PASSPHRASE) {
      args.SetString(
          "fullEncryptionBody",
          GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM));
    } else {
      args.SetString(
          "fullEncryptionBody",
          GetStringUTF16(IDS_SYNC_FULL_ENCRYPTION_DATA));
    }
  } else {
    args.SetBoolean("usePassphrase", service->IsUsingSecondaryPassphrase());
  }

  StringValue page("configure");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);

  if (SyncPromoUI::UseWebBasedSigninFlow()) {
    // Make sure the tab used for the Gaia sign in does not cover the settings
    // tab.
    FocusUI();
  }
}

void SyncSetupHandler::ConfigureSyncDone() {
  StringValue page("done");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page);

  // Suppress the sync promo once the user signs into sync. This way the user
  // doesn't see the sync promo even if they sign out of sync later on.
  SyncPromoUI::SetUserSkippedSyncPromo(GetProfile());

  ProfileSyncService* service = GetSyncService();
  DCHECK(service);
  if (!service->HasSyncSetupCompleted()) {
    // This is the first time configuring sync, so log it.
    base::FilePath profile_file_path = GetProfile()->GetPath();
    ProfileMetrics::LogProfileSyncSignIn(profile_file_path);

    // We're done configuring, so notify ProfileSyncService that it is OK to
    // start syncing.
    service->SetSyncSetupCompleted();
  }
}

bool SyncSetupHandler::IsActiveLogin() const {
  // LoginUIService can be NULL if page is brought up in incognito mode
  // (i.e. if the user is running in guest mode in cros and brings up settings).
  LoginUIService* service = GetLoginUIService();
  return service && (service->current_login_ui() == this);
}

void SyncSetupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "SyncSetupDidClosePage",
      base::Bind(&SyncSetupHandler::OnDidClosePage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupSubmitAuth",
      base::Bind(&SyncSetupHandler::HandleSubmitAuth,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupConfigure",
      base::Bind(&SyncSetupHandler::HandleConfigure,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupAttachHandler",
      base::Bind(&SyncSetupHandler::HandleAttachHandler,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowErrorUI",
      base::Bind(&SyncSetupHandler::HandleShowErrorUI,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowSetupUI",
      base::Bind(&SyncSetupHandler::HandleShowSetupUI,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupShowSetupUIWithoutLogin",
      base::Bind(&SyncSetupHandler::HandleShowSetupUIWithoutLogin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncSetupDoSignOutOnAuthError",
      base::Bind(&SyncSetupHandler::HandleDoSignOutOnAuthError,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("CloseTimeout",
      base::Bind(&SyncSetupHandler::HandleCloseTimeout,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncSetupStopSyncing",
      base::Bind(&SyncSetupHandler::HandleStopSyncing,
                 base::Unretained(this)));
}

SigninManager* SyncSetupHandler::GetSignin() const {
  return SigninManagerFactory::GetForProfile(GetProfile());
}

void SyncSetupHandler::DisplayGaiaLogin(bool fatal_error) {
  if (SyncPromoUI::UseWebBasedSigninFlow()) {
    // Advanced options are no longer being configured if the login screen is
    // visible. If the user exits the signin wizard after this without
    // configuring sync, CloseSyncSetup() will ensure they are logged out.
    configuring_sync_ = false;

    DisplayGaiaLoginInNewTabOrWindow();
    signin_tracker_.reset(
        new SigninTracker(GetProfile(), this,
                          SigninTracker::WAITING_FOR_GAIA_VALIDATION));
  } else {
    retry_on_signin_failure_ = true;
    DisplayGaiaLoginWithErrorMessage(string16(), fatal_error);
  }
}

void SyncSetupHandler::DisplayGaiaLoginInNewTabOrWindow() {
  DCHECK(!active_gaia_signin_tab_);
  GURL url(SyncPromoUI::GetSyncPromoURL(GURL(),
      SyncPromoUI::SOURCE_SETTINGS, false));
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser) {
    // Settings is not displayed in a browser window. Open a new window.
    browser = new Browser(Browser::CreateParams(
        Browser::TYPE_TABBED, GetProfile(), chrome::GetActiveDesktop()));
  }

  // If the signin manager already has an authenticated username, this is a
  // re-auth scenario, and we need to ensure that the user signs in with the
  // same email address.
  std::string email = SigninManagerFactory::GetForProfile(
      browser->profile())->GetAuthenticatedUsername();
  if (!email.empty()) {
    std::string fragment("Email=");
    fragment += email;
    GURL::Replacements replacements;
    replacements.SetRefStr(fragment);
    url = url.ReplaceComponents(replacements);
  }

  active_gaia_signin_tab_ = browser->OpenURL(
      content::OpenURLParams(url, content::Referrer(), SINGLETON_TAB,
                             content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  content::WebContentsObserver::Observe(active_gaia_signin_tab_);
}

void SyncSetupHandler::DisplayGaiaLoginWithErrorMessage(
    const string16& error_message, bool fatal_error) {
  // Advanced options are no longer being configured if the login screen is
  // visible. If the user exits the signin wizard after this without
  // configuring sync, CloseSyncSetup() will ensure they are logged out.
  configuring_sync_ = false;

  string16 local_error_message(error_message);

  // Setup args for the GAIA login screen:
  //   error_message: custom error message to display.
  //   fatalError: fatal error message to display.
  //   error: GoogleServiceAuthError from previous login attempt (0 if none).
  //   user: The email the user most recently entered.
  //   editable_user: Whether the username field should be editable.
  //   captchaUrl: The captcha image to display to the user (empty if none).
  std::string user, captcha;
  int error;
  bool editable_user;
  if (!last_attempted_user_email_.empty()) {
    // This is a repeat of a login attempt.
    user = last_attempted_user_email_;
    error = last_signin_error_.state();
    captcha = last_signin_error_.captcha().image_url.spec();
    editable_user = true;

    if (local_error_message.empty())
      local_error_message = UTF8ToUTF16(last_signin_error_.error_message());
  } else {
    // Fresh login attempt - lock in the authenticated username if there is
    // one (don't let the user change it).
    user = GetSignin()->GetAuthenticatedUsername();
    error = 0;
    editable_user = user.empty();
  }
  DictionaryValue args;
  args.SetString("user", user);
  args.SetInteger("error", error);

  // If the error is two-factor, then ask for an OTP if the ClientOAuth flow
  // is enasbled.  Otherwise ask for an ASP.  If the error is catptcha required,
  // then we don't want to show username and password fields if ClientOAuth is
  // being used, since those fields are ignored by the endpoint on challenges.
  if (error == GoogleServiceAuthError::TWO_FACTOR)
    args.SetBoolean("askForOtp", false);
  else if (error == GoogleServiceAuthError::CAPTCHA_REQUIRED)
    args.SetBoolean("hideEmailAndPassword", false);

  // Tell the page the previous email address used for sync.  If the user
  // enters a different email address, he will be shown a warning.
  std::string last_email = GetProfile()->GetPrefs()->GetString(
      prefs::kGoogleServicesLastUsername);
  args.SetString("lastEmailAddress", last_email);

  args.SetBoolean("editableUser", editable_user);
  if (!local_error_message.empty())
    args.SetString("errorMessage", local_error_message);
  if (fatal_error)
    args.SetBoolean("fatalError", true);
  args.SetString("captchaUrl", captcha);
  StringValue page("login");
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

bool SyncSetupHandler::PrepareSyncSetup() {

  // If the wizard is already visible, just focus that one.
  if (FocusExistingWizardIfPresent()) {
    if (!IsActiveLogin())
      CloseOverlay();
    return false;
  }

  // Notify services that login UI is now active.
  GetLoginUIService()->SetLoginUI(this);

  ProfileSyncService* service = GetSyncService();
  if (service)
    service->SetSetupInProgress(true);

  return true;
}

void SyncSetupHandler::DisplaySpinner() {
  configuring_sync_ = true;
  StringValue page("spinner");
  DictionaryValue args;

  const int kTimeoutSec = 30;
  DCHECK(!backend_start_timer_.get());
  backend_start_timer_.reset(new base::OneShotTimer<SyncSetupHandler>());
  backend_start_timer_->Start(FROM_HERE,
                              base::TimeDelta::FromSeconds(kTimeoutSec),
                              this, &SyncSetupHandler::DisplayTimeout);

  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

// TODO(kochi): Handle error conditions other than timeout.
// http://crbug.com/128692
void SyncSetupHandler::DisplayTimeout() {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // Do not listen to signin events.
  signin_tracker_.reset();

  StringValue page("timeout");
  DictionaryValue args;
  web_ui()->CallJavascriptFunction(
      "SyncSetupOverlay.showSyncSetupPage", page, args);
}

void SyncSetupHandler::RecordSignin() {
  // By default, do nothing - subclasses can override.
}

void SyncSetupHandler::DisplayGaiaSuccessAndClose() {
  RecordSignin();
  web_ui()->CallJavascriptFunction("SyncSetupOverlay.showSuccessAndClose");
}

void SyncSetupHandler::DisplayGaiaSuccessAndSettingUp() {
  RecordSignin();
  if (SyncPromoUI::UseWebBasedSigninFlow())
    CloseGaiaSigninPage();

  web_ui()->CallJavascriptFunction("SyncSetupOverlay.showSuccessAndSettingUp");
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

  std::string username, password, captcha, otp, access_code;
  if (!GetAuthData(json, &username, &password, &captcha, &otp, &access_code)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  string16 error_message;
  if (!IsLoginAuthDataValid(username, &error_message)) {
    DisplayGaiaLoginWithErrorMessage(error_message, false);
    return;
  }

  // If one of password, captcha, otp and access_code is non-empty, then the
  // others must be empty.  At least one should be non-empty.
  DCHECK(password.empty() ||
         (captcha.empty() && otp.empty() && access_code.empty()));
  DCHECK(captcha.empty() ||
         (password.empty() && otp.empty() && access_code.empty()));
  DCHECK(otp.empty() ||
         (captcha.empty() && password.empty() && access_code.empty()));
  DCHECK(access_code.empty() ||
         (captcha.empty() && password.empty() && otp.empty()));
  DCHECK(!otp.empty() || !captcha.empty() || !password.empty() ||
         !access_code.empty());

  const std::string& solution = captcha.empty() ?
      (otp.empty() ? EmptyString() : otp) : captcha;
  TryLogin(username, password, solution, access_code);
}

void SyncSetupHandler::TryLogin(const std::string& username,
                                const std::string& password,
                                const std::string& solution,
                                const std::string& access_code) {
  DCHECK(IsActiveLogin());
  // Make sure we are listening for signin traffic.
  if (!signin_tracker_.get())
    signin_tracker_.reset(new SigninTracker(GetProfile(), this));

  last_attempted_user_email_ = username;

  // User is trying to log in again so reset the cached error.
  GoogleServiceAuthError current_error = last_signin_error_;
  last_signin_error_ = GoogleServiceAuthError::None();

  SigninManager* signin = GetSignin();

  // If we're just being called to provide an ASP, then pass it to the
  // SigninManager and wait for the next step.
  if (!access_code.empty()) {
    signin->ProvideSecondFactorAccessCode(access_code);
    return;
  }

  // The user has submitted credentials, which indicates they don't want to
  // suppress start up anymore. We do this before starting the signin process,
  // so the ProfileSyncService knows to listen to the cached password.
  ProfileSyncService* service = GetSyncService();
  if (service)
    service->UnsuppressAndStart();

  // Kick off a sign-in through the signin manager.
  signin->StartSignIn(username, password, current_error.captcha().token,
                      solution);
}

void SyncSetupHandler::GaiaCredentialsValid() {
  DCHECK(IsActiveLogin());

  // Gaia credentials are valid - update the UI.
  DisplayGaiaSuccessAndSettingUp();
}

void SyncSetupHandler::SigninFailed(const GoogleServiceAuthError& error) {
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  last_signin_error_ = error;

  // If using web-based sign in flow, don't show the gaia sign in page again
  // since there is no way to show the user an error message.
  if (SyncPromoUI::UseWebBasedSigninFlow()) {
    CloseSyncSetup();
  } else if (retry_on_signin_failure_) {
    // Got a failed signin - this is either just a typical auth error, or a
    // sync error (treat sync errors as "fatal errors" - i.e. non-auth errors).
    // On ChromeOS, this condition can happen when auth token is invalid and
    // cannot start sync backend.
    // If using web-based sign in flow, don't show the gaia sign in page again
    // since there is no way to show the user an error message.
    ProfileSyncService* service = GetSyncService();
    DisplayGaiaLogin(service && service->HasUnrecoverableError());
  } else {
    // TODO(peria): Show error dialog for prompting sign in and out on
    // Chrome OS. http://crbug.com/128692
    CloseOverlay();
  }
}

Profile* SyncSetupHandler::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}

ProfileSyncService* SyncSetupHandler::GetSyncService() const {
  Profile* profile = GetProfile();
  return profile->IsSyncAccessible() ?
      ProfileSyncServiceFactory::GetForProfile(GetProfile()) : NULL;
}

void SyncSetupHandler::SigninSuccess() {
  ProfileSyncService* service = GetSyncService();
  DCHECK(!service || service->sync_initialized());
  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();

  // If we have signed in while sync is already setup, it must be due to some
  // kind of re-authentication flow. In that case, just close the signin dialog
  // rather than forcing the user to go through sync configuration.
  if (!service || service->HasSyncSetupCompleted())
    DisplayGaiaSuccessAndClose();
  else
    DisplayConfigureSync(false, false);
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

  SyncConfigInfo configuration;
  if (!GetConfiguration(json, &configuration)) {
    // The page sent us something that we didn't understand.
    // This probably indicates a programming error.
    NOTREACHED();
    return;
  }

  // Start configuring the ProfileSyncService using the configuration passed
  // to us from the JS layer.
  ProfileSyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just close the sync
  // dialog.
  if (!service || !service->sync_initialized()) {
    CloseOverlay();
    return;
  }

  // Note: Data encryption will not occur until configuration is complete
  // (when the PSS receives its CONFIGURE_DONE notification from the sync
  // backend), so the user still has a chance to cancel out of the operation
  // if (for example) some kind of passphrase error is encountered.
  if (configuration.encrypt_all)
    service->EnableEncryptEverything();

  bool passphrase_failed = false;
  if (!configuration.passphrase.empty()) {
    // We call IsPassphraseRequired() here (instead of
    // IsPassphraseRequiredForDecryption()) because the user may try to enter
    // a passphrase even though no encrypted data types are enabled.
    if (service->IsPassphraseRequired()) {
      // If we have pending keys, try to decrypt them with the provided
      // passphrase. We track if this succeeds or fails because a failed
      // decryption should result in an error even if there aren't any encrypted
      // data types.
      passphrase_failed =
          !service->SetDecryptionPassphrase(configuration.passphrase);
    } else {
      // OK, the user sent us a passphrase, but we don't have pending keys. So
      // it either means that the pending keys were resolved somehow since the
      // time the UI was displayed (re-encryption, pending passphrase change,
      // etc) or the user wants to re-encrypt.
      if (!configuration.passphrase_is_gaia &&
          !service->IsUsingSecondaryPassphrase()) {
        // User passed us a secondary passphrase, and the data is encrypted
        // with a GAIA passphrase so they must want to encrypt.
        service->SetEncryptionPassphrase(configuration.passphrase,
                                         ProfileSyncService::EXPLICIT);
      }
    }
  }

  bool user_was_prompted_for_passphrase =
      service->IsPassphraseRequiredForDecryption();
  service->OnUserChoseDatatypes(configuration.sync_everything,
                                configuration.data_types);

  // Need to call IsPassphraseRequiredForDecryption() *after* calling
  // OnUserChoseDatatypes() because the user may have just disabled the
  // encrypted datatypes (in which case we just want to exit, not prompt the
  // user for a passphrase).
  if (passphrase_failed || service->IsPassphraseRequiredForDecryption()) {
    // We need a passphrase, or the user's attempt to set a passphrase failed -
    // prompt them again. This covers a few subtle cases:
    // 1) The user enters an incorrect passphrase *and* disabled the encrypted
    //    data types. In that case we want to notify the user that the
    //    passphrase was incorrect even though there are no longer any encrypted
    //    types enabled (IsPassphraseRequiredForDecryption() == false).
    // 2) The user doesn't enter any passphrase. In this case, we won't call
    //    SetDecryptionPassphrase() (passphrase_failed == false), but we still
    //    want to display an error message to let the user know that their
    //    blank passphrase entry is not acceptable.
    // 3) The user just enabled an encrypted data type - in this case we don't
    //    want to display an "invalid passphrase" error, since it's the first
    //    time the user is seeing the prompt.
    DisplayConfigureSync(
        true, passphrase_failed || user_was_prompted_for_passphrase);
  } else {
    // No passphrase is required from the user so mark the configuration as
    // complete and close the sync setup overlay.
    ConfigureSyncDone();
  }

  ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CUSTOMIZE);
  if (configuration.encrypt_all)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_ENCRYPT);
  if (configuration.passphrase_is_gaia && !configuration.passphrase.empty())
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_PASSPHRASE);
  if (!configuration.sync_everything)
    ProfileMetrics::LogProfileSyncInfo(ProfileMetrics::SYNC_CHOOSE);
}

void SyncSetupHandler::HandleAttachHandler(const ListValue* args) {
  bool force_login = false;
  std::string json;
  if (args->GetString(0, &json) && !json.empty()) {
    scoped_ptr<Value> parsed_value(base::JSONReader::Read(json));
    DictionaryValue* result = static_cast<DictionaryValue*>(parsed_value.get());
    result->GetBoolean("forceLogin", &force_login);
  }

  OpenSyncSetup(force_login);
}

void SyncSetupHandler::HandleShowErrorUI(const ListValue* args) {
  DCHECK(!configuring_sync_);

  ProfileSyncService* service = GetSyncService();
  DCHECK(service);

  // Bring up the existing wizard, or just display it on this page.
  if (!FocusExistingWizardIfPresent())
    OpenSyncSetup(false);
}

void SyncSetupHandler::HandleShowSetupUI(const ListValue* args) {
  OpenSyncSetup(false);
}

// TODO(atwilson): Remove chrome-os-only API in favor of routing everything
// through ShowSetupUI.
void SyncSetupHandler::HandleShowSetupUIWithoutLogin(const ListValue* args) {
  OpenConfigureSync();
}

void SyncSetupHandler::HandleDoSignOutOnAuthError(const ListValue* args) {
  DLOG(INFO) << "Signing out the user to fix a sync error.";
  chrome::AttemptUserExit();
}

void SyncSetupHandler::HandleStopSyncing(const ListValue* args) {
  if (GetSyncService())
    ProfileSyncService::SyncEvent(ProfileSyncService::STOP_FROM_OPTIONS);

  GetSignin()->SignOut();
}

void SyncSetupHandler::HandleCloseTimeout(const ListValue* args) {
  CloseSyncSetup();
}

void SyncSetupHandler::CloseSyncSetup() {
  // TODO(atwilson): Move UMA tracking of signin events out of sync module.
  ProfileSyncService* sync_service = GetSyncService();
  if (IsActiveLogin()) {
    if (!sync_service || !sync_service->HasSyncSetupCompleted()) {
      if (signin_tracker_.get()) {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_DURING_SIGNON);
      } else if (configuring_sync_) {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_DURING_CONFIGURE);
      } else {
        ProfileSyncService::SyncEvent(
            ProfileSyncService::CANCEL_FROM_SIGNON_WITHOUT_AUTH);
      }
    }

    // Let the various services know that we're no longer active.
    if (SyncPromoUI::UseWebBasedSigninFlow())
      CloseGaiaSigninPage();

    GetLoginUIService()->LoginUIClosed(this);
  }

  if (sync_service) {
    // Make sure user isn't left half-logged-in (signed in, but without sync
    // started up). If the user hasn't finished setting up sync, then sign out
    // and shut down sync.
    if (!sync_service->HasSyncSetupCompleted()) {
      DVLOG(1) << "Signin aborted by user action";
      if (signin_tracker_.get() || sync_service->FirstSetupInProgress()) {
        // User was still in the process of signing in, so sign him out again.
        // This makes sure that the user isn't left signed in but with sync
        // un-configured.
        //
        // This has the side-effect of signing out the user in the following
        // scenario:
        //   * User signs in while sync is disabled by policy.
        //   * Sync is re-enabled by policy.
        //   * User brings up sync setup dialog to do initial sync config.
        //   * User cancels out of the dialog.
        //
        // This case is indistinguishable from the "one click signin" case where
        // the user checks the "advanced setup" checkbox, then cancels out of
        // the setup box, which is a much more common scenario, so we do the
        // right thing for the one-click case.
        GetSignin()->SignOut();
      }
      sync_service->DisableForUser();
      browser_sync::SyncPrefs sync_prefs(GetProfile()->GetPrefs());
      sync_prefs.SetStartSuppressed(true);
    }
    sync_service->SetSetupInProgress(false);
  }

  // Reset the attempted email address and error, otherwise the sync setup
  // overlay in the settings page will stay in whatever error state it was last
  // when it is reopened.
  last_attempted_user_email_.clear();
  last_signin_error_ = GoogleServiceAuthError::None();

  configuring_sync_ = false;
  signin_tracker_.reset();

  // Stop a timer to handle timeout in waiting for checking network connection.
  backend_start_timer_.reset();
}

void SyncSetupHandler::OpenSyncSetup(bool force_login) {
  if (!PrepareSyncSetup())
    return;

  // There are several different UI flows that can bring the user here:
  // 1) Signin promo (passes force_login=true)
  // 2) Normal signin through options page (GetAuthenticatedUsername() is
  //    empty).
  // 3) Previously working credentials have expired.
  // 4) User is already signed in, but App Notifications needs to force another
  //    login so it can fetch an oauth token (passes force_login=true)
  // 5) User is signed in, but has stopped sync via the google dashboard, and
  //    signout is prohibited by policy so we need to force a re-auth.
  // 6) User clicks [Advanced Settings] button on options page while already
  //    logged in.
  // 7) One-click signin (credentials are already available, so should display
  //    sync configure UI, not login UI).
  // 8) ChromeOS re-enable after disabling sync.
  SigninManager* signin = GetSignin();
  if (force_login ||
      signin->GetAuthenticatedUsername().empty() ||
#if !defined(OS_CHROMEOS)
      (GetSyncService() && GetSyncService()->IsStartSuppressed()) ||
#endif
      signin->signin_global_error()->HasBadge()) {
    // User is not logged in, or login has been specially requested - need to
    // display login UI (cases 1-4).
    DisplayGaiaLogin(false);
  } else {
    if (!GetSyncService()) {
      // This can happen if the user directly navigates to /settings/syncSetup.
      DLOG(WARNING) << "Cannot display sync UI when sync is disabled";
      CloseOverlay();
      return;
    }

    // User is already logged in. They must have brought up the config wizard
    // via the "Advanced..." button or through One-Click signin (cases 5/6), or
    // they are re-enabling sync on Chrome OS.
    DisplayConfigureSync(true, false);
  }

  if (!SyncPromoUI::UseWebBasedSigninFlow())
    ShowSetupUI();
}

void SyncSetupHandler::OpenConfigureSync() {
  if (!PrepareSyncSetup())
    return;

  DisplayConfigureSync(true, false);
  ShowSetupUI();
}

void SyncSetupHandler::FocusUI() {
  DCHECK(IsActiveLogin());
  // Bring the GAIA tab to the foreground if there is one.
  if (SyncPromoUI::UseWebBasedSigninFlow() && signin_tracker_ &&
      active_gaia_signin_tab_) {
    BringTabToFront(active_gaia_signin_tab_);
  } else {
    WebContents* web_contents = web_ui()->GetWebContents();
    web_contents->GetDelegate()->ActivateContents(web_contents);
  }
}

void SyncSetupHandler::CloseUI() {
  DCHECK(IsActiveLogin());
  CloseOverlay();
}

void SyncSetupHandler::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  DCHECK(active_gaia_signin_tab_);

  // If the user lands on a page outside of Gaia, assume they have navigated
  // away and are no longer thinking about signing in with this tab.  Treat
  // this as if the user closed the tab. However, don't actually close the tab
  // since the user is doing something with it.  Disconnect and forget about it
  // before closing down the sync setup.
  // The one exception is the expected continue URL.  If the user lands there,
  // this means sign in was successful.  Ignore the source parameter in the
  // continue URL since this user may have changed the state of the
  // "Let me choose what to sync" checkbox.
  const GURL& url = active_gaia_signin_tab_->GetURL();
  const GURL continue_url =
      SyncPromoUI::GetNextPageURLForSyncPromoURL(
          SyncPromoUI::GetSyncPromoURL(GURL(),
                                       SyncPromoUI::SOURCE_SETTINGS,
                                       false));
  GURL::Replacements replacements;
  replacements.ClearQuery();

  if (!gaia::IsGaiaSignonRealm(url.GetOrigin()) &&
      url.ReplaceComponents(replacements) !=
          continue_url.ReplaceComponents(replacements)) {
    content::WebContentsObserver::Observe(NULL);
    active_gaia_signin_tab_ = NULL;
    CloseSyncSetup();
  }
}

void SyncSetupHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(active_gaia_signin_tab_);
  CloseSyncSetup();
}

// Private member functions.

bool SyncSetupHandler::FocusExistingWizardIfPresent() {
  LoginUIService* service = GetLoginUIService();
  if (!service->current_login_ui())
    return false;
  service->current_login_ui()->FocusUI();
  return true;
}

LoginUIService* SyncSetupHandler::GetLoginUIService() const {
  return LoginUIServiceFactory::GetForProfile(GetProfile());
}

void SyncSetupHandler::CloseOverlay() {
  // Stop a timer to handle timeout in waiting for sync setup.
  backend_start_timer_.reset();

  CloseSyncSetup();
  web_ui()->CallJavascriptFunction("OptionsPage.closeOverlay");
}

void SyncSetupHandler::CloseGaiaSigninPage() {
  if (active_gaia_signin_tab_) {
    content::WebContentsObserver::Observe(NULL);

    Browser* browser = chrome::FindBrowserWithWebContents(
        active_gaia_signin_tab_);
    if (browser) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();
      if (tab_strip_model) {
        int index = tab_strip_model->GetIndexOfWebContents(
            active_gaia_signin_tab_);
        if (index != TabStripModel::kNoTab) {
          tab_strip_model->ExecuteContextMenuCommand(
              index, TabStripModel::CommandCloseTab);
        }
      }
    }
  }

  active_gaia_signin_tab_ = NULL;
}

bool SyncSetupHandler::IsLoginAuthDataValid(const std::string& username,
                                            string16* error_message) {
  if (username.empty())
    return true;

  // Can be null during some unit tests.
  if (!web_ui())
    return true;

  if (!GetSignin()->IsAllowedUsername(username)) {
    *error_message = l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_NAME_PROHIBITED);
    return false;
  }

  // If running in a unit test, skip profile check.
  if (!profile_manager_)
    return true;

  // Check if the username is already in use by another profile.
  const ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t current_profile_index =
      cache.GetIndexOfProfileWithPath(GetProfile()->GetPath());
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
