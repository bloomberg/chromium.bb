// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kAppInstallBubbleKey[] = "appInstallBubble";
const char kIconDataKey[] = "iconData";
const char kIconUrlKey[] = "iconUrl";
const char kIdKey[] = "id";
const char kLocalizedNameKey[] = "localizedName";
const char kLoginKey[] = "login";
const char kManifestKey[] = "manifest";
const char kTokenKey[] = "token";

const char kCannotSpecifyIconDataAndUrlError[] =
    "You cannot specify both icon data and an icon url";
const char kInvalidIconUrlError[] = "Invalid icon url";
const char kInvalidIdError[] = "Invalid id";
const char kInvalidManifestError[] = "Invalid manifest";
const char kNoPreviousBeginInstallError[] =
    "* does not match a previous call to beginInstall";
const char kUserCancelledError[] = "User cancelled install";
const char kUserGestureRequiredError[] =
    "This function must be called during a user gesture";

ProfileSyncService* test_sync_service = NULL;
bool ignore_user_gesture_for_tests = false;

// Returns either the test sync service, or the real one from |profile|.
ProfileSyncService* GetSyncService(Profile* profile) {
  if (test_sync_service)
    return test_sync_service;
  else
    return profile->GetProfileSyncService();
}

bool IsWebStoreURL(Profile* profile, const GURL& url) {
  ExtensionService* service = profile->GetExtensionService();
  const Extension* store = service->GetWebStoreApp();
  if (!store) {
    NOTREACHED();
    return false;
  }
  return (service->GetExtensionByWebExtent(url) == store);
}

// Helper to create a dictionary with login and token properties set from
// the appropriate values in the passed-in |profile|.
DictionaryValue* CreateLoginResult(Profile* profile) {
  DictionaryValue* dictionary = new DictionaryValue();
  std::string username = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  dictionary->SetString(kLoginKey, username);
  if (!username.empty()) {
    CommandLine* cmdline = CommandLine::ForCurrentProcess();
    TokenService* token_service = profile->GetTokenService();
    if (cmdline->HasSwitch(switches::kAppsGalleryReturnTokens) &&
        token_service->HasTokenForService(GaiaConstants::kGaiaService)) {
      dictionary->SetString(kTokenKey,
                            token_service->GetTokenForService(
                                GaiaConstants::kGaiaService));
    }
  }
  return dictionary;
}

}  // namespace

// static
void WebstorePrivateApi::SetTestingProfileSyncService(
    ProfileSyncService* service) {
  test_sync_service = service;
}

// static
void BeginInstallFunction::SetIgnoreUserGestureForTests(bool ignore) {
  ignore_user_gesture_for_tests = ignore;
}

bool BeginInstallFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;

  std::string id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id));
  if (!Extension::IdIsValid(id)) {
    error_ = kInvalidIdError;
    return false;
  }

  if (!user_gesture() && !ignore_user_gesture_for_tests) {
    error_ = kUserGestureRequiredError;
    return false;
  }

  // This gets cleared in CrxInstaller::ConfirmInstall(). TODO(asargent) - in
  // the future we may also want to add time-based expiration, where a whitelist
  // entry is only valid for some number of minutes.
  CrxInstaller::SetWhitelistedInstallId(id);
  return true;
}

BeginInstallWithManifestFunction::BeginInstallWithManifestFunction()
  : use_app_installed_bubble_(false) {}

BeginInstallWithManifestFunction::~BeginInstallWithManifestFunction() {}

bool BeginInstallWithManifestFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url())) {
    SetResult(PERMISSION_DENIED);
    return false;
  }

  if (!user_gesture() && !ignore_user_gesture_for_tests) {
    SetResult(NO_GESTURE);
    error_ = kUserGestureRequiredError;
    return false;
  }

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  CHECK(details);

  EXTENSION_FUNCTION_VALIDATE(details->GetString(kIdKey, &id_));
  if (!Extension::IdIsValid(id_)) {
    SetResult(INVALID_ID);
    error_ = kInvalidIdError;
    return false;
  }

  EXTENSION_FUNCTION_VALIDATE(details->GetString(kManifestKey, &manifest_));

  if (details->HasKey(kIconDataKey) && details->HasKey(kIconUrlKey)) {
    SetResult(ICON_ERROR);
    error_ = kCannotSpecifyIconDataAndUrlError;
    return false;
  }

  if (details->HasKey(kIconDataKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kIconDataKey, &icon_data_));

  GURL icon_url;
  if (details->HasKey(kIconUrlKey)) {
    std::string tmp_url;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kIconUrlKey, &tmp_url));
    icon_url = source_url().Resolve(tmp_url);
    if (!icon_url.is_valid()) {
      SetResult(INVALID_ICON_URL);
      error_ = kInvalidIconUrlError;
      return false;
    }
  }

  if (details->HasKey(kLocalizedNameKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kLocalizedNameKey,
                                                   &localized_name_));

  if (details->HasKey(kAppInstallBubbleKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(
        kAppInstallBubbleKey, &use_app_installed_bubble_));

  net::URLRequestContextGetter* context_getter = NULL;
  if (!icon_url.is_empty())
    context_getter = profile()->GetRequestContext();

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this, manifest_, icon_data_, icon_url, context_getter);

  // The helper will call us back via OnWebstoreParseSuccess or
  // OnWebstoreParseFailure.
  helper->Start();

  // Matched with a Release in OnWebstoreParseSuccess/OnWebstoreParseFailure.
  AddRef();

  // The response is sent asynchronously in OnWebstoreParseSuccess/
  // OnWebstoreParseFailure.
  return true;
}


void BeginInstallWithManifestFunction::SetResult(ResultCode code) {
  switch (code) {
    case ERROR_NONE:
      result_.reset(Value::CreateStringValue(""));
      break;
    case UNKNOWN_ERROR:
      result_.reset(Value::CreateStringValue("unknown_error"));
      break;
    case USER_CANCELLED:
      result_.reset(Value::CreateStringValue("user_cancelled"));
      break;
    case MANIFEST_ERROR:
      result_.reset(Value::CreateStringValue("manifest_error"));
      break;
    case ICON_ERROR:
      result_.reset(Value::CreateStringValue("icon_error"));
      break;
    case INVALID_ID:
      result_.reset(Value::CreateStringValue("invalid_id"));
      break;
    case PERMISSION_DENIED:
      result_.reset(Value::CreateStringValue("permission_denied"));
      break;
    case NO_GESTURE:
      result_.reset(Value::CreateStringValue("no_gesture"));
      break;
    case INVALID_ICON_URL:
      result_.reset(Value::CreateStringValue("invalid_icon_url"));
      break;
    default:
      CHECK(false);
  }
}

// static
void BeginInstallWithManifestFunction::SetIgnoreUserGestureForTests(
    bool ignore) {
  ignore_user_gesture_for_tests = ignore;
}

void BeginInstallWithManifestFunction::OnWebstoreParseSuccess(
    const SkBitmap& icon, DictionaryValue* parsed_manifest) {
  CHECK(parsed_manifest);
  icon_ = icon;
  parsed_manifest_.reset(parsed_manifest);

  ShowExtensionInstallDialogForManifest(
      profile(),
      this,
      parsed_manifest,
      id_,
      localized_name_,
      &icon_,
      ExtensionInstallUI::INSTALL_PROMPT,
      &dummy_extension_);
  if (!dummy_extension_.get()) {
    OnWebstoreParseFailure(WebstoreInstallHelper::Delegate::MANIFEST_ERROR,
                           kInvalidManifestError);
    return;
  }

  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void BeginInstallWithManifestFunction::OnWebstoreParseFailure(
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result_code,
    const std::string& error_message) {
  // Map from WebstoreInstallHelper's result codes to ours.
  switch (result_code) {
    case WebstoreInstallHelper::Delegate::UNKNOWN_ERROR:
      SetResult(UNKNOWN_ERROR);
      break;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      SetResult(ICON_ERROR);
      break;
    case WebstoreInstallHelper::Delegate::MANIFEST_ERROR:
      SetResult(MANIFEST_ERROR);
      break;
    default:
      CHECK(false);
  }
  error_ = error_message;
  SendResponse(false);

  // Matches the AddRef in RunImpl().
  Release();
}

void BeginInstallWithManifestFunction::InstallUIProceed() {
  CrxInstaller::WhitelistEntry* entry = new CrxInstaller::WhitelistEntry;
  entry->parsed_manifest.reset(parsed_manifest_.release());
  entry->localized_name = localized_name_;
  entry->use_app_installed_bubble = use_app_installed_bubble_;
  CrxInstaller::SetWhitelistEntry(id_, entry);
  SetResult(ERROR_NONE);
  SendResponse(true);

  // The Permissions_Install histogram is recorded from the ExtensionService
  // for all extension installs, so we only need to record the web store
  // specific histogram here.
  ExtensionService::RecordPermissionMessagesHistogram(
      dummy_extension_, "Extensions.Permissions_WebStoreInstall");

  // Matches the AddRef in RunImpl().
  Release();
}

void BeginInstallWithManifestFunction::InstallUIAbort(bool user_initiated) {
  error_ = kUserCancelledError;
  SetResult(USER_CANCELLED);
  SendResponse(false);

  // The web store install histograms are a subset of the install histograms.
  // We need to record both histograms here since CrxInstaller::InstallUIAbort
  // is never called for web store install cancellations
  std::string histogram_name = user_initiated ?
      "Extensions.Permissions_WebStoreInstallCancel" :
      "Extensions.Permissions_WebStoreInstallAbort";
  ExtensionService::RecordPermissionMessagesHistogram(
      dummy_extension_, histogram_name.c_str());

  histogram_name = user_initiated ?
      "Extensions.Permissions_InstallCancel" :
      "Extensions.Permissions_InstallAbort";
  ExtensionService::RecordPermissionMessagesHistogram(
      dummy_extension_, histogram_name.c_str());

  // Matches the AddRef in RunImpl().
  Release();
}

bool CompleteInstallFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;

  std::string id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id));
  if (!Extension::IdIsValid(id)) {
    error_ = kInvalidIdError;
    return false;
  }

  if (!CrxInstaller::IsIdWhitelisted(id) &&
      !CrxInstaller::GetWhitelistEntry(id)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoPreviousBeginInstallError, id);
    return false;
  }

  GURL install_url(extension_urls::GetWebstoreInstallUrl(
      id, g_browser_process->GetApplicationLocale()));

  // The download url for the given |id| is now contained in |url|. We
  // navigate the current (calling) tab to this url which will result in a
  // download starting. Once completed it will go through the normal extension
  // install flow. The above call to SetWhitelistedInstallId will bypass the
  // normal permissions install dialog.
  NavigationController& controller =
      dispatcher()->delegate()->GetAssociatedTabContents()->controller();
  controller.LoadURL(install_url, source_url(), PageTransition::LINK);

  return true;
}

bool GetBrowserLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  result_.reset(CreateLoginResult(profile_->GetOriginalProfile()));
  return true;
}

bool GetStoreLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  ExtensionService* service = profile_->GetExtensionService();
  ExtensionPrefs* prefs = service->extension_prefs();
  std::string login;
  if (prefs->GetWebStoreLogin(&login)) {
    result_.reset(Value::CreateStringValue(login));
  } else {
    result_.reset(Value::CreateStringValue(std::string()));
  }
  return true;
}

bool SetStoreLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  std::string login;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &login));
  ExtensionService* service = profile_->GetExtensionService();
  ExtensionPrefs* prefs = service->extension_prefs();
  prefs->SetWebStoreLogin(login);
  return true;
}
