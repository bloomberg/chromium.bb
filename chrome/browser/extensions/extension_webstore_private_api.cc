// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::GpuDataManager;
using extensions::BundleInstaller;

namespace {

// Holds the Approvals between the time we prompt and start the installs.
struct PendingApprovals {
  typedef ScopedVector<WebstoreInstaller::Approval> ApprovalList;

  PendingApprovals();
  ~PendingApprovals();

  void PushApproval(scoped_ptr<WebstoreInstaller::Approval> approval);
  scoped_ptr<WebstoreInstaller::Approval> PopApproval(
      Profile* profile, const std::string& id);

  ApprovalList approvals;
};

PendingApprovals::PendingApprovals() {}
PendingApprovals::~PendingApprovals() {}

void PendingApprovals::PushApproval(
    scoped_ptr<WebstoreInstaller::Approval> approval) {
  approvals.push_back(approval.release());
}

scoped_ptr<WebstoreInstaller::Approval> PendingApprovals::PopApproval(
    Profile* profile, const std::string& id) {
  for (size_t i = 0; i < approvals.size(); ++i) {
    WebstoreInstaller::Approval* approval = approvals[i];
    if (approval->extension_id == id &&
        profile->IsSameProfile(approval->profile)) {
      approvals.weak_erase(approvals.begin() + i);
      return scoped_ptr<WebstoreInstaller::Approval>(approval);
    }
  }
  return scoped_ptr<WebstoreInstaller::Approval>(NULL);
}

static base::LazyInstance<PendingApprovals> g_pending_approvals =
    LAZY_INSTANCE_INITIALIZER;

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
const char kNoPreviousBeginInstallWithManifestError[] =
    "* does not match a previous call to beginInstallWithManifest3";
const char kUserCancelledError[] = "User cancelled install";

ProfileSyncService* test_sync_service = NULL;

// Returns either the test sync service, or the real one from |profile|.
ProfileSyncService* GetSyncService(Profile* profile) {
  // TODO(webstore): It seems |test_sync_service| is not used anywhere. It
  // should be removed.
  if (test_sync_service)
    return test_sync_service;
  else
    return ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
}

// Whitelists extension IDs for use by webstorePrivate.silentlyInstall.
bool trust_test_ids = false;

bool IsTrustedForSilentInstall(const std::string& id) {
  // Trust the extensions in api_test/webstore_private/bundle when the flag
  // is set.
  if (trust_test_ids &&
      (id == "begfmnajjkbjdgmffnjaojchoncnmngg" ||
       id == "bmfoocgfinpmkmlbjhcbofejhkhlbchk" ||
       id == "mpneghmdnmaolkljkipbhaienajcflfe"))
    return true;

  return
      id == "jgoepmocgafhnchmokaimcmlojpnlkhp" ||  // +1 Extension
      id == "cpembckmhnjipbgbnfiocbgnkpjdokdd" ||  // +1 Extension - dev
      id == "boemmnepglcoinjcdlfcpcbmhiecichi" ||  // Notifications
      id == "flibmgiapaohcbondaoopaalfejliklp" ||  // Notifications - dev
      id == "nckgahadagoaajjgafhacjanaoiihapd" ||  // Talk
      id == "eggnbpckecmjlblplehfpjjdhhidfdoj" ||  // Talk Beta
      id == "dlppkpafhbajpcmmoheippocdidnckmm" ||  // Remaining are placeholders
      id == "hmglfmpefabcafaimbpldpambdfomanl" ||
      id == "idfijlieiecpfcjckpkliefekpokhhnd" ||
      id == "jaokjbijaokooelpahnlmbciccldmfla" ||
      id == "kdjeommiakphmeionoojjljlecmpaldd" ||
      id == "lpdeojkfhenboeibhkjhiancceeboknd";
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
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile);
    if (cmdline->HasSwitch(switches::kAppsGalleryReturnTokens) &&
        token_service->HasTokenForService(GaiaConstants::kGaiaService)) {
      dictionary->SetString(kTokenKey,
                            token_service->GetTokenForService(
                                GaiaConstants::kGaiaService));
    }
  }
  return dictionary;
}

WebstoreInstaller::Delegate* test_webstore_installer_delegate = NULL;

}  // namespace

// static
void WebstorePrivateApi::SetTestingProfileSyncService(
    ProfileSyncService* service) {
  test_sync_service = service;
}

// static
void WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(
    WebstoreInstaller::Delegate* delegate) {
  test_webstore_installer_delegate = delegate;
}

// static
void WebstorePrivateApi::SetTrustTestIDsForTesting(bool allow) {
  trust_test_ids = allow;
}

// static
scoped_ptr<WebstoreInstaller::Approval>
    WebstorePrivateApi::PopApprovalForTesting(
        Profile* profile, const std::string& extension_id) {
  return g_pending_approvals.Get().PopApproval(profile, extension_id);
}

InstallBundleFunction::InstallBundleFunction() {}
InstallBundleFunction::~InstallBundleFunction() {}

bool InstallBundleFunction::RunImpl() {
  ListValue* extensions = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &extensions));

  BundleInstaller::ItemList items;
  if (!ReadBundleInfo(extensions, &items))
    return false;

  bundle_ = new BundleInstaller(profile(), items);

  AddRef();  // Balanced in OnBundleInstallCompleted / OnBundleInstallCanceled.

  bundle_->PromptForApproval(this);
  return true;
}

bool InstallBundleFunction::ReadBundleInfo(ListValue* extensions,
                                           BundleInstaller::ItemList* items) {
  for (size_t i = 0; i < extensions->GetSize(); ++i) {
    DictionaryValue* details = NULL;
    EXTENSION_FUNCTION_VALIDATE(extensions->GetDictionary(i, &details));

    BundleInstaller::Item item;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(
        kIdKey, &item.id));
    EXTENSION_FUNCTION_VALIDATE(details->GetString(
        kManifestKey, &item.manifest));
    EXTENSION_FUNCTION_VALIDATE(details->GetString(
        kLocalizedNameKey, &item.localized_name));

    items->push_back(item);
  }

  return true;
}

void InstallBundleFunction::OnBundleInstallApproved() {
  bundle_->CompleteInstall(
      &(dispatcher()->delegate()->GetAssociatedWebContents()->GetController()),
      GetCurrentBrowser(),
      this);
}

void InstallBundleFunction::OnBundleInstallCanceled(bool user_initiated) {
  if (user_initiated)
    error_ = "user_canceled";
  else
    error_ = "unknown_error";

  SendResponse(false);

  Release();  // Balanced in RunImpl().
}

void InstallBundleFunction::OnBundleInstallCompleted() {
  SendResponse(true);

  Release();  // Balanced in RunImpl().
}

BeginInstallWithManifestFunction::BeginInstallWithManifestFunction()
    : use_app_installed_bubble_(false) {}

BeginInstallWithManifestFunction::~BeginInstallWithManifestFunction() {}

bool BeginInstallWithManifestFunction::RunImpl() {
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
      this, id_, manifest_, icon_data_, icon_url, context_getter);

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
    case INVALID_ICON_URL:
      result_.reset(Value::CreateStringValue("invalid_icon_url"));
      break;
    default:
      CHECK(false);
  }
}

void BeginInstallWithManifestFunction::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    DictionaryValue* parsed_manifest) {
  CHECK_EQ(id_, id);
  CHECK(parsed_manifest);
  icon_ = icon;
  parsed_manifest_.reset(parsed_manifest);

  std::string error;
  dummy_extension_ = ExtensionInstallUI::GetLocalizedExtensionForDisplay(
      parsed_manifest_.get(), id, localized_name_, "", &error);

  if (!dummy_extension_) {
    OnWebstoreParseFailure(id_, WebstoreInstallHelper::Delegate::MANIFEST_ERROR,
                           kInvalidManifestError);
    return;
  }

  install_ui_.reset(new ExtensionInstallUI(profile()));
  install_ui_->ConfirmWebstoreInstall(this, dummy_extension_, &icon_);
  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void BeginInstallWithManifestFunction::OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result_code,
    const std::string& error_message) {
  CHECK_EQ(id_, id);

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
  // This gets cleared in CrxInstaller::ConfirmInstall(). TODO(asargent) - in
  // the future we may also want to add time-based expiration, where a whitelist
  // entry is only valid for some number of minutes.
  scoped_ptr<WebstoreInstaller::Approval> approval(
      new WebstoreInstaller::Approval);
  approval->extension_id = id_;
  approval->profile = profile();
  approval->parsed_manifest.reset(parsed_manifest_.release());
  approval->use_app_installed_bubble = use_app_installed_bubble_;
  g_pending_approvals.Get().PushApproval(approval.Pass());

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
  // is never called for web store install cancellations.
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
  std::string id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id));
  if (!Extension::IdIsValid(id)) {
    error_ = kInvalidIdError;
    return false;
  }

  scoped_ptr<WebstoreInstaller::Approval> approval(
      g_pending_approvals.Get().PopApproval(profile(), id));
  if (!approval.get()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoPreviousBeginInstallWithManifestError, id);
    return false;
  }

  // The extension will install through the normal extension install flow, but
  // the whitelist entry will bypass the normal permissions install dialog.
  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile(), test_webstore_installer_delegate,
      &(dispatcher()->delegate()->GetAssociatedWebContents()->GetController()),
      id, approval.Pass(), WebstoreInstaller::FLAG_NONE);
  installer->Start();

  return true;
}

SilentlyInstallFunction::SilentlyInstallFunction() {}
SilentlyInstallFunction::~SilentlyInstallFunction() {}

bool SilentlyInstallFunction::RunImpl() {
  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  CHECK(details);

  EXTENSION_FUNCTION_VALIDATE(details->GetString(kIdKey, &id_));
  if (!IsTrustedForSilentInstall(id_)) {
    error_ = kInvalidIdError;
    return false;
  }

  EXTENSION_FUNCTION_VALIDATE(details->GetString(kManifestKey, &manifest_));

  // Matched in OnWebstoreParseFailure, OnExtensionInstall{Success,Failure}.
  AddRef();

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this, id_, manifest_, std::string(), GURL(), NULL);
  helper->Start();

  return true;
}

void SilentlyInstallFunction::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* parsed_manifest) {
  CHECK_EQ(id_, id);

  // This lets CrxInstaller bypass the permission confirmation UI for the
  // extension. The whitelist entry gets cleared in
  // CrxInstaller::ConfirmInstall.
  scoped_ptr<WebstoreInstaller::Approval> approval(
      new WebstoreInstaller::Approval);
  approval->extension_id = id_;
  approval->parsed_manifest.reset(parsed_manifest);
  approval->profile = profile();
  approval->use_app_installed_bubble = false;
  approval->skip_post_install_ui = true;

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile(), this,
      &(dispatcher()->delegate()->GetAssociatedWebContents()->GetController()),
      id_, approval.Pass(), WebstoreInstaller::FLAG_NONE);
  installer->Start();
}

void SilentlyInstallFunction::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  CHECK_EQ(id_, id);

  error_ = error_message;
  SendResponse(false);

  Release();  // Matches the AddRef() in RunImpl().
}

void SilentlyInstallFunction::OnExtensionInstallSuccess(const std::string& id) {
  CHECK_EQ(id_, id);

  SendResponse(true);

  Release();  // Matches the AddRef() in RunImpl().
}

void SilentlyInstallFunction::OnExtensionInstallFailure(
    const std::string& id, const std::string& error) {
  CHECK_EQ(id_, id);

  error_ = error;
  SendResponse(false);

  Release();  // Matches the AddRef() in RunImpl().
}

bool GetBrowserLoginFunction::RunImpl() {
  result_.reset(CreateLoginResult(profile_->GetOriginalProfile()));
  return true;
}

bool GetStoreLoginFunction::RunImpl() {
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
  std::string login;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &login));
  ExtensionService* service = profile_->GetExtensionService();
  ExtensionPrefs* prefs = service->extension_prefs();
  prefs->SetWebStoreLogin(login);
  return true;
}

GetWebGLStatusFunction::GetWebGLStatusFunction() {}
GetWebGLStatusFunction::~GetWebGLStatusFunction() {}

// static
bool GetWebGLStatusFunction::IsWebGLAllowed(GpuDataManager* manager) {
  bool webgl_allowed = true;
  if (!manager->GpuAccessAllowed()) {
    webgl_allowed = false;
  } else {
    uint32 blacklist_type = manager->GetGpuFeatureType();
    if (blacklist_type & content::GPU_FEATURE_TYPE_WEBGL)
      webgl_allowed = false;
  }
  return webgl_allowed;
}

void GetWebGLStatusFunction::OnGpuInfoUpdate() {
  GpuDataManager* manager = GpuDataManager::GetInstance();
  manager->RemoveObserver(this);
  bool webgl_allowed = IsWebGLAllowed(manager);
  CreateResult(webgl_allowed);
  SendResponse(true);

  // Matches the AddRef in RunImpl().
  Release();
}

void GetWebGLStatusFunction::CreateResult(bool webgl_allowed) {
  result_.reset(Value::CreateStringValue(
      webgl_allowed ? "webgl_allowed" : "webgl_blocked"));
}

bool GetWebGLStatusFunction::RunImpl() {
  bool finalized = true;
#if defined(OS_LINUX)
  // On Windows and Mac, so far we can always make the final WebGL blacklisting
  // decision based on partial GPU info; on Linux, we need to launch the GPU
  // process to collect full GPU info and make the final decision.
  finalized = false;
#endif

  GpuDataManager* manager = GpuDataManager::GetInstance();
  if (manager->IsCompleteGPUInfoAvailable())
    finalized = true;

  bool webgl_allowed = IsWebGLAllowed(manager);
  if (!webgl_allowed)
    finalized = true;

  if (finalized) {
    CreateResult(webgl_allowed);
    SendResponse(true);
  } else {
    // Matched with a Release in OnGpuInfoUpdate.
    AddRef();

    manager->AddObserver(this);
    manager->RequestCompleteGpuInfoIfNeeded();
  }
  return true;
}
