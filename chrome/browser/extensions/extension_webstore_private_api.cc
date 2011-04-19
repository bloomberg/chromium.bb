// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

#include <string>
#include <vector>

#include "base/memory/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kLoginKey[] = "login";
const char kTokenKey[] = "token";
const char kImageDecodeError[] = "Image decode failed";
const char kInvalidIdError[] = "Invalid id";
const char kInvalidManifestError[] = "Invalid manifest";
const char kNoPreviousBeginInstallError[] =
    "* does not match a previous call to beginInstall";
const char kUserCancelledError[] = "User cancelled install";
const char kUserGestureRequiredError[] =
    "This function must be called during a user gesture";

ProfileSyncService* test_sync_service = NULL;
BrowserSignin* test_signin = NULL;
bool ignore_user_gesture_for_tests = false;

// Returns either the test sync service, or the real one from |profile|.
ProfileSyncService* GetSyncService(Profile* profile) {
  if (test_sync_service)
    return test_sync_service;
  else
    return profile->GetProfileSyncService();
}

BrowserSignin* GetBrowserSignin(Profile* profile) {
  if (test_signin)
    return test_signin;
  else
    return profile->GetBrowserSignin();
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
  std::string username = GetBrowserSignin(profile)->GetSignedInUsername();
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

// If |profile| is not incognito, returns it. Otherwise returns the real
// (not incognito) default profile.
Profile* GetDefaultProfile(Profile* profile) {
  if (!profile->IsOffTheRecord())
    return profile;
  else
    return g_browser_process->profile_manager()->GetDefaultProfile();
}

}  // namespace

// static
void WebstorePrivateApi::SetTestingProfileSyncService(
    ProfileSyncService* service) {
  test_sync_service = service;
}

// static
void WebstorePrivateApi::SetTestingBrowserSignin(BrowserSignin* signin) {
  test_signin = signin;
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

// This is a class to help BeginInstallWithManifestFunction manage sending
// JSON manifests and base64-encoded icon data to the utility process for
// parsing.
class SafeBeginInstallHelper : public UtilityProcessHost::Client {
 public:
  SafeBeginInstallHelper(BeginInstallWithManifestFunction* client,
                         const std::string& icon_data,
                         const std::string& manifest)
      : client_(client),
        icon_data_(icon_data),
        manifest_(manifest),
        utility_host_(NULL),
        icon_decode_complete_(false),
        manifest_parse_complete_(false),
        parse_error_(BeginInstallWithManifestFunction::UNKNOWN_ERROR) {}

  void Start() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBeginInstallHelper::StartWorkOnIOThread));
  }

  void StartWorkOnIOThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    utility_host_ = new UtilityProcessHost(this, BrowserThread::IO);
    utility_host_->StartBatchMode();
    if (icon_data_.empty())
      icon_decode_complete_ = true;
    else
      utility_host_->StartImageDecodingBase64(icon_data_);
    utility_host_->StartJSONParsing(manifest_);
  }

  // Implementing pieces of the UtilityProcessHost::Client interface.
  virtual void OnDecodeImageSucceeded(const SkBitmap& decoded_image) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    icon_ = decoded_image;
    icon_decode_complete_ = true;
    ReportResultsIfComplete();
  }
  virtual void OnDecodeImageFailed() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    icon_decode_complete_ = true;
    error_ = std::string(kImageDecodeError);
    parse_error_ = BeginInstallWithManifestFunction::ICON_ERROR;
    ReportResultsIfComplete();
  }
  virtual void OnJSONParseSucceeded(const ListValue& wrapper) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    manifest_parse_complete_ = true;
    Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    if (value->IsType(Value::TYPE_DICTIONARY)) {
      parsed_manifest_.reset(
          static_cast<DictionaryValue*>(value)->DeepCopy());
    } else {
      parse_error_ = BeginInstallWithManifestFunction::MANIFEST_ERROR;
    }
    ReportResultsIfComplete();
  }

  virtual void OnJSONParseFailed(const std::string& error_message) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    manifest_parse_complete_ = true;
    error_ = error_message;
    parse_error_ = BeginInstallWithManifestFunction::MANIFEST_ERROR;
    ReportResultsIfComplete();
  }

  void ReportResultsIfComplete() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    if (!icon_decode_complete_ || !manifest_parse_complete_)
      return;

    // The utility_host_ will take care of deleting itself after this call.
    utility_host_->EndBatchMode();
    utility_host_ = NULL;

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &SafeBeginInstallHelper::ReportResultFromUIThread));
  }

  void ReportResultFromUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (error_.empty() && parsed_manifest_.get())
      client_->OnParseSuccess(icon_, parsed_manifest_.release());
    else
      client_->OnParseFailure(parse_error_, error_);
  }

 private:
  ~SafeBeginInstallHelper() {}

  // The client who we'll report results back to.
  BeginInstallWithManifestFunction* client_;

  // The data to parse.
  std::string icon_data_;
  std::string manifest_;

  UtilityProcessHost* utility_host_;

  // Flags for whether we're done doing icon decoding and manifest parsing.
  bool icon_decode_complete_;
  bool manifest_parse_complete_;

  // The results of succesful decoding/parsing.
  SkBitmap icon_;
  scoped_ptr<DictionaryValue> parsed_manifest_;

  // A details string for keeping track of any errors.
  std::string error_;

  // A code to distinguish between an error with the icon, and an error with the
  // manifest.
  BeginInstallWithManifestFunction::ResultCode parse_error_;
};

BeginInstallWithManifestFunction::BeginInstallWithManifestFunction() {}

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

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id_));
  if (!Extension::IdIsValid(id_)) {
    SetResult(INVALID_ID);
    error_ = kInvalidIdError;
    return false;
  }

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &icon_data_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &manifest_));

  scoped_refptr<SafeBeginInstallHelper> helper =
      new SafeBeginInstallHelper(this, icon_data_, manifest_);
  // The helper will call us back via OnParseSucces or OnParseFailure.
  helper->Start();

  // Matched with a Release in OnSuccess/OnFailure.
  AddRef();

  // The response is sent asynchronously in OnSuccess/OnFailure.
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
    default:
      CHECK(false);
  }
}


void BeginInstallWithManifestFunction::OnParseSuccess(
    const SkBitmap& icon, DictionaryValue* parsed_manifest) {
  CHECK(parsed_manifest);
  icon_ = icon;
  parsed_manifest_.reset(parsed_manifest);

  // Create a dummy extension and show the extension install confirmation
  // dialog.
  std::string init_errors;
  dummy_extension_ = Extension::Create(
      FilePath(),
      Extension::INTERNAL,
      *static_cast<DictionaryValue*>(parsed_manifest_.get()),
      Extension::NO_FLAGS,
      &init_errors);
  if (!dummy_extension_.get()) {
    OnParseFailure(MANIFEST_ERROR, std::string(kInvalidManifestError));
    return;
  }
  if (icon_.empty())
    icon_ = Extension::GetDefaultIcon(dummy_extension_->is_app());

  ShowExtensionInstallDialog(profile(),
                             this,
                             dummy_extension_.get(),
                             &icon_,
                             dummy_extension_->GetPermissionMessageStrings(),
                             ExtensionInstallUI::INSTALL_PROMPT);

  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void BeginInstallWithManifestFunction::OnParseFailure(
    ResultCode result_code, const std::string& error_message) {
  SetResult(result_code);
  error_ = error_message;
  SendResponse(false);

  // Matches the AddRef in RunImpl().
  Release();
}

void BeginInstallWithManifestFunction::InstallUIProceed() {
  CrxInstaller::SetWhitelistedManifest(id_, parsed_manifest_.release());
  SetResult(ERROR_NONE);
  SendResponse(true);

  // Matches the AddRef in RunImpl().
  Release();
}

void BeginInstallWithManifestFunction::InstallUIAbort() {
  error_ = std::string(kUserCancelledError);
  SetResult(USER_CANCELLED);
  SendResponse(false);

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
      !CrxInstaller::GetWhitelistedManifest(id)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoPreviousBeginInstallError, id);
    return false;
  }

  std::vector<std::string> params;
  params.push_back("id=" + id);
  params.push_back("lang=" + g_browser_process->GetApplicationLocale());
  params.push_back("uc");
  std::string url_string = Extension::GalleryUpdateUrl(true).spec();

  GURL url(url_string + "?response=redirect&x=" +
      EscapeQueryParamValue(JoinString(params, '&'), true));
  DCHECK(url.is_valid());

  // The download url for the given |id| is now contained in |url|. We
  // navigate the current (calling) tab to this url which will result in a
  // download starting. Once completed it will go through the normal extension
  // install flow. The above call to SetWhitelistedInstallId will bypass the
  // normal permissions install dialog.
  NavigationController& controller =
      dispatcher()->delegate()->associated_tab_contents()->controller();
  controller.LoadURL(url, source_url(), PageTransition::LINK);

  return true;
}

bool GetBrowserLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  result_.reset(CreateLoginResult(GetDefaultProfile(profile_)));
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

PromptBrowserLoginFunction::PromptBrowserLoginFunction()
    : waiting_for_token_(false) {}

PromptBrowserLoginFunction::~PromptBrowserLoginFunction() {
}

bool PromptBrowserLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;

  std::string preferred_email;
  if (args_->GetSize() > 0) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &preferred_email));
  }

  Profile* profile = GetDefaultProfile(profile_);

  // Login can currently only be invoked tab-modal.  Since this is
  // coming from the webstore, we should always have a tab, but check
  // just in case.
  TabContents* tab = dispatcher()->delegate()->associated_tab_contents();
  if (!tab)
    return false;

  // We return the result asynchronously, so we addref to keep ourself alive.
  // Matched with a Release in OnLoginSuccess() and OnLoginFailure().
  AddRef();

  // Start listening for notifications about the token.
  TokenService* token_service = profile->GetTokenService();
  registrar_.Add(this,
                 NotificationType::TOKEN_AVAILABLE,
                 Source<TokenService>(token_service));
  registrar_.Add(this,
                 NotificationType::TOKEN_REQUEST_FAILED,
                 Source<TokenService>(token_service));

  GetBrowserSignin(profile)->RequestSignin(tab,
                                           ASCIIToUTF16(preferred_email),
                                           GetLoginMessage(),
                                           this);

  // The response will be sent asynchronously in OnLoginSuccess/OnLoginFailure.
  return true;
}

string16 PromptBrowserLoginFunction::GetLoginMessage() {
  using l10n_util::GetStringUTF16;
  using l10n_util::GetStringFUTF16;

  // TODO(johnnyg): This would be cleaner as an HTML template.
  // http://crbug.com/60216
  string16 message;
  message = ASCIIToUTF16("<p>")
      + GetStringUTF16(IDS_WEB_STORE_LOGIN_INTRODUCTION_1)
      + ASCIIToUTF16("</p>");
  message = message + ASCIIToUTF16("<p>")
      + GetStringFUTF16(IDS_WEB_STORE_LOGIN_INTRODUCTION_2,
                        GetStringUTF16(IDS_PRODUCT_NAME))
      + ASCIIToUTF16("</p>");
  return message;
}

void PromptBrowserLoginFunction::OnLoginSuccess() {
  // Ensure that apps are synced.
  // - If the user has already setup sync, we add Apps to the current types.
  // - If not, we create a new set which is just Apps.
  ProfileSyncService* service = GetSyncService(GetDefaultProfile(profile_));
  syncable::ModelTypeSet types;
  if (service->HasSyncSetupCompleted())
    service->GetPreferredDataTypes(&types);
  types.insert(syncable::APPS);
  service->ChangePreferredDataTypes(types);
  service->SetSyncSetupCompleted();

  // We'll finish up in Observe() when the token is ready.
  waiting_for_token_ = true;
}

void PromptBrowserLoginFunction::OnLoginFailure(
    const GoogleServiceAuthError& error) {
  SendResponse(false);
  // Matches the AddRef in RunImpl().
  Release();
}

void PromptBrowserLoginFunction::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  // Make sure this notification is for the service we are interested in.
  std::string service;
  if (type == NotificationType::TOKEN_AVAILABLE) {
    TokenService::TokenAvailableDetails* available =
        Details<TokenService::TokenAvailableDetails>(details).ptr();
    service = available->service();
  } else if (type == NotificationType::TOKEN_REQUEST_FAILED) {
    TokenService::TokenRequestFailedDetails* failed =
        Details<TokenService::TokenRequestFailedDetails>(details).ptr();
    service = failed->service();
  } else {
    NOTREACHED();
  }

  if (service != GaiaConstants::kGaiaService) {
    return;
  }

  DCHECK(waiting_for_token_);

  result_.reset(CreateLoginResult(GetDefaultProfile(profile_)));
  SendResponse(true);

  // Matches the AddRef in RunImpl().
  Release();
}
