// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/gpu/gpu_feature_checker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_tracker_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::GpuDataManager;

namespace extensions {

namespace BeginInstallWithManifest3 =
    api::webstore_private::BeginInstallWithManifest3;
namespace CompleteInstall = api::webstore_private::CompleteInstall;
namespace GetBrowserLogin = api::webstore_private::GetBrowserLogin;
namespace GetIsLauncherEnabled = api::webstore_private::GetIsLauncherEnabled;
namespace GetStoreLogin = api::webstore_private::GetStoreLogin;
namespace GetWebGLStatus = api::webstore_private::GetWebGLStatus;
namespace InstallBundle = api::webstore_private::InstallBundle;
namespace IsInIncognitoMode = api::webstore_private::IsInIncognitoMode;
namespace SetStoreLogin = api::webstore_private::SetStoreLogin;

namespace {

// Holds the Approvals between the time we prompt and start the installs.
class PendingApprovals {
 public:
  PendingApprovals();
  ~PendingApprovals();

  void PushApproval(scoped_ptr<WebstoreInstaller::Approval> approval);
  scoped_ptr<WebstoreInstaller::Approval> PopApproval(
      Profile* profile, const std::string& id);
 private:
  typedef ScopedVector<WebstoreInstaller::Approval> ApprovalList;

  ApprovalList approvals_;

  DISALLOW_COPY_AND_ASSIGN(PendingApprovals);
};

PendingApprovals::PendingApprovals() {}
PendingApprovals::~PendingApprovals() {}

void PendingApprovals::PushApproval(
    scoped_ptr<WebstoreInstaller::Approval> approval) {
  approvals_.push_back(approval.release());
}

scoped_ptr<WebstoreInstaller::Approval> PendingApprovals::PopApproval(
    Profile* profile, const std::string& id) {
  for (size_t i = 0; i < approvals_.size(); ++i) {
    WebstoreInstaller::Approval* approval = approvals_[i];
    if (approval->extension_id == id &&
        profile->IsSameProfile(approval->profile)) {
      approvals_.weak_erase(approvals_.begin() + i);
      return scoped_ptr<WebstoreInstaller::Approval>(approval);
    }
  }
  return scoped_ptr<WebstoreInstaller::Approval>();
}

// Uniquely holds the profile and extension id of an install between the time we
// prompt and complete the installs.
class PendingInstalls {
 public:
  PendingInstalls();
  ~PendingInstalls();

  bool InsertInstall(Profile* profile, const std::string& id);
  void EraseInstall(Profile* profile, const std::string& id);
 private:
  typedef std::pair<Profile*, std::string> ProfileAndExtensionId;
  typedef std::vector<ProfileAndExtensionId> InstallList;

  InstallList::iterator FindInstall(Profile* profile, const std::string& id);

  InstallList installs_;

  DISALLOW_COPY_AND_ASSIGN(PendingInstalls);
};

PendingInstalls::PendingInstalls() {}
PendingInstalls::~PendingInstalls() {}

// Returns true and inserts the profile/id pair if it is not present. Otherwise
// returns false.
bool PendingInstalls::InsertInstall(Profile* profile, const std::string& id) {
  if (FindInstall(profile, id) != installs_.end())
    return false;
  installs_.push_back(make_pair(profile, id));
  return true;
}

// Removes the given profile/id pair.
void PendingInstalls::EraseInstall(Profile* profile, const std::string& id) {
  InstallList::iterator it = FindInstall(profile, id);
  if (it != installs_.end())
    installs_.erase(it);
}

PendingInstalls::InstallList::iterator PendingInstalls::FindInstall(
    Profile* profile,
    const std::string& id) {
  for (size_t i = 0; i < installs_.size(); ++i) {
    ProfileAndExtensionId install = installs_[i];
    if (install.second == id && profile->IsSameProfile(install.first))
      return (installs_.begin() + i);
  }
  return installs_.end();
}

static base::LazyInstance<PendingApprovals> g_pending_approvals =
    LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<PendingInstalls> g_pending_installs =
    LAZY_INSTANCE_INITIALIZER;

// A preference set by the web store to indicate login information for
// purchased apps.
const char kWebstoreLogin[] = "extensions.webstore_login";
const char kAlreadyInstalledError[] = "This item is already installed";
const char kCannotSpecifyIconDataAndUrlError[] =
    "You cannot specify both icon data and an icon url";
const char kInvalidIconUrlError[] = "Invalid icon url";
const char kInvalidIdError[] = "Invalid id";
const char kInvalidManifestError[] = "Invalid manifest";
const char kNoPreviousBeginInstallWithManifestError[] =
    "* does not match a previous call to beginInstallWithManifest3";
const char kUserCancelledError[] = "User cancelled install";

WebstoreInstaller::Delegate* test_webstore_installer_delegate = NULL;

// We allow the web store to set a string containing login information when a
// purchase is made, so that when a user logs into sync with a different
// account we can recognize the situation. The Get function returns the login if
// there was previously stored data, or an empty string otherwise. The Set will
// overwrite any previous login.
std::string GetWebstoreLogin(Profile* profile) {
  if (profile->GetPrefs()->HasPrefPath(kWebstoreLogin))
    return profile->GetPrefs()->GetString(kWebstoreLogin);
  return std::string();
}

void SetWebstoreLogin(Profile* profile, const std::string& login) {
  profile->GetPrefs()->SetString(kWebstoreLogin, login);
}

void RecordWebstoreExtensionInstallResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Webstore.ExtensionInstallResult", success);
}

}  // namespace

// static
void WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(
    WebstoreInstaller::Delegate* delegate) {
  test_webstore_installer_delegate = delegate;
}

// static
scoped_ptr<WebstoreInstaller::Approval>
WebstorePrivateApi::PopApprovalForTesting(
    Profile* profile, const std::string& extension_id) {
  return g_pending_approvals.Get().PopApproval(profile, extension_id);
}

WebstorePrivateInstallBundleFunction::WebstorePrivateInstallBundleFunction() {}
WebstorePrivateInstallBundleFunction::~WebstorePrivateInstallBundleFunction() {}

bool WebstorePrivateInstallBundleFunction::RunImpl() {
  scoped_ptr<InstallBundle::Params> params(
      InstallBundle::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  BundleInstaller::ItemList items;
  if (!ReadBundleInfo(*params, &items))
    return false;

  bundle_ = new BundleInstaller(GetCurrentBrowser(), items);

  AddRef();  // Balanced in OnBundleInstallCompleted / OnBundleInstallCanceled.

  bundle_->PromptForApproval(this);
  return true;
}

bool WebstorePrivateInstallBundleFunction::
    ReadBundleInfo(const InstallBundle::Params& params,
    BundleInstaller::ItemList* items) {
  for (size_t i = 0; i < params.details.size(); ++i) {
    BundleInstaller::Item item;
    item.id = params.details[i]->id;
    item.manifest = params.details[i]->manifest;
    item.localized_name = params.details[i]->localized_name;
    items->push_back(item);
  }

  return true;
}

void WebstorePrivateInstallBundleFunction::OnBundleInstallApproved() {
  bundle_->CompleteInstall(
      dispatcher()->delegate()->GetAssociatedWebContents(),
      this);
}

void WebstorePrivateInstallBundleFunction::OnBundleInstallCanceled(
    bool user_initiated) {
  if (user_initiated)
    error_ = "user_canceled";
  else
    error_ = "unknown_error";

  SendResponse(false);

  Release();  // Balanced in RunImpl().
}

void WebstorePrivateInstallBundleFunction::OnBundleInstallCompleted() {
  SendResponse(true);

  Release();  // Balanced in RunImpl().
}

WebstorePrivateBeginInstallWithManifest3Function::
    WebstorePrivateBeginInstallWithManifest3Function() {}

WebstorePrivateBeginInstallWithManifest3Function::
    ~WebstorePrivateBeginInstallWithManifest3Function() {}

bool WebstorePrivateBeginInstallWithManifest3Function::RunImpl() {
  params_ = BeginInstallWithManifest3::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (!extensions::Extension::IdIsValid(params_->details.id)) {
    SetResultCode(INVALID_ID);
    error_ = kInvalidIdError;
    return false;
  }

  if (params_->details.icon_data && params_->details.icon_url) {
    SetResultCode(ICON_ERROR);
    error_ = kCannotSpecifyIconDataAndUrlError;
    return false;
  }

  GURL icon_url;
  if (params_->details.icon_url) {
    std::string tmp_url;
    icon_url = source_url().Resolve(*params_->details.icon_url);
    if (!icon_url.is_valid()) {
      SetResultCode(INVALID_ICON_URL);
      error_ = kInvalidIconUrlError;
      return false;
    }
  }

  std::string icon_data = params_->details.icon_data ?
      *params_->details.icon_data : std::string();

  Profile* profile = GetProfile();
  if (util::IsExtensionInstalledPermanently(params_->details.id, profile) ||
      !g_pending_installs.Get().InsertInstall(profile, params_->details.id)) {
    SetResultCode(ALREADY_INSTALLED);
    error_ = kAlreadyInstalledError;
    return false;
  }

  net::URLRequestContextGetter* context_getter = NULL;
  if (!icon_url.is_empty())
    context_getter = GetProfile()->GetRequestContext();

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this, params_->details.id, params_->details.manifest, icon_data, icon_url,
          context_getter);

  // The helper will call us back via OnWebstoreParseSuccess or
  // OnWebstoreParseFailure.
  helper->Start();

  // Matched with a Release in OnWebstoreParseSuccess/OnWebstoreParseFailure.
  AddRef();

  // The response is sent asynchronously in OnWebstoreParseSuccess/
  // OnWebstoreParseFailure.
  return true;
}

const char* WebstorePrivateBeginInstallWithManifest3Function::
    ResultCodeToString(ResultCode code) {
  switch (code) {
    case ERROR_NONE:
      return "";
    case UNKNOWN_ERROR:
      return "unknown_error";
    case USER_CANCELLED:
      return "user_cancelled";
    case MANIFEST_ERROR:
      return "manifest_error";
    case ICON_ERROR:
      return "icon_error";
    case INVALID_ID:
      return "invalid_id";
    case PERMISSION_DENIED:
      return "permission_denied";
    case INVALID_ICON_URL:
      return "invalid_icon_url";
    case SIGNIN_FAILED:
      return "signin_failed";
    case ALREADY_INSTALLED:
      return "already_installed";
  }
  NOTREACHED();
  return "";
}

void WebstorePrivateBeginInstallWithManifest3Function::SetResultCode(
    ResultCode code) {
  results_ = BeginInstallWithManifest3::Results::Create(
      ResultCodeToString(code));
}

void WebstorePrivateBeginInstallWithManifest3Function::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* parsed_manifest) {
  CHECK_EQ(params_->details.id, id);
  CHECK(parsed_manifest);
  icon_ = icon;
  parsed_manifest_.reset(parsed_manifest);

  std::string localized_name = params_->details.localized_name ?
      *params_->details.localized_name : std::string();

  std::string error;
  dummy_extension_ = ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
      parsed_manifest_.get(),
      Extension::FROM_WEBSTORE,
      id,
      localized_name,
      std::string(),
      &error);

  if (!dummy_extension_.get()) {
    OnWebstoreParseFailure(params_->details.id,
                           WebstoreInstallHelper::Delegate::MANIFEST_ERROR,
                           kInvalidManifestError);
    return;
  }

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(GetProfile());
  if (dummy_extension_->is_platform_app() &&
      signin_manager &&
      signin_manager->GetAuthenticatedUsername().empty() &&
      signin_manager->AuthInProgress()) {
    signin_tracker_ =
        SigninTrackerFactory::CreateForProfile(GetProfile(), this);
    return;
  }

  SigninCompletedOrNotNeeded();
}

void WebstorePrivateBeginInstallWithManifest3Function::OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result_code,
    const std::string& error_message) {
  CHECK_EQ(params_->details.id, id);

  // Map from WebstoreInstallHelper's result codes to ours.
  switch (result_code) {
    case WebstoreInstallHelper::Delegate::UNKNOWN_ERROR:
      SetResultCode(UNKNOWN_ERROR);
      break;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      SetResultCode(ICON_ERROR);
      break;
    case WebstoreInstallHelper::Delegate::MANIFEST_ERROR:
      SetResultCode(MANIFEST_ERROR);
      break;
    default:
      CHECK(false);
  }
  error_ = error_message;
  g_pending_installs.Get().EraseInstall(GetProfile(), id);
  SendResponse(false);

  // Matches the AddRef in RunImpl().
  Release();
}

void WebstorePrivateBeginInstallWithManifest3Function::SigninFailed(
    const GoogleServiceAuthError& error) {
  signin_tracker_.reset();

  SetResultCode(SIGNIN_FAILED);
  error_ = error.ToString();
  g_pending_installs.Get().EraseInstall(GetProfile(), params_->details.id);
  SendResponse(false);

  // Matches the AddRef in RunImpl().
  Release();
}

void WebstorePrivateBeginInstallWithManifest3Function::SigninSuccess() {
  signin_tracker_.reset();

  SigninCompletedOrNotNeeded();
}

void WebstorePrivateBeginInstallWithManifest3Function::MergeSessionComplete(
    const GoogleServiceAuthError& error) {
  // TODO(rogerta): once the embeded inline flow is enabled, the code in
  // WebstorePrivateBeginInstallWithManifest3Function::SigninSuccess()
  // should move to here.
}

void WebstorePrivateBeginInstallWithManifest3Function::
    SigninCompletedOrNotNeeded() {
  content::WebContents* web_contents = GetAssociatedWebContents();
  if (!web_contents)  // The browser window has gone away.
    return;
  install_prompt_.reset(new ExtensionInstallPrompt(web_contents));
  install_prompt_->ConfirmWebstoreInstall(
      this,
      dummy_extension_.get(),
      &icon_,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void WebstorePrivateBeginInstallWithManifest3Function::InstallUIProceed() {
  // This gets cleared in CrxInstaller::ConfirmInstall(). TODO(asargent) - in
  // the future we may also want to add time-based expiration, where a whitelist
  // entry is only valid for some number of minutes.
  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          GetProfile(), params_->details.id, parsed_manifest_.Pass(), false));
  approval->use_app_installed_bubble = params_->details.app_install_bubble;
  approval->enable_launcher = params_->details.enable_launcher;
  // If we are enabling the launcher, we should not show the app list in order
  // to train the user to open it themselves at least once.
  approval->skip_post_install_ui = params_->details.enable_launcher;
  approval->dummy_extension = dummy_extension_;
  approval->installing_icon = gfx::ImageSkia::CreateFrom1xBitmap(icon_);
  g_pending_approvals.Get().PushApproval(approval.Pass());

  SetResultCode(ERROR_NONE);
  SendResponse(true);

  // The Permissions_Install histogram is recorded from the ExtensionService
  // for all extension installs, so we only need to record the web store
  // specific histogram here.
  ExtensionService::RecordPermissionMessagesHistogram(
      dummy_extension_.get(), "Extensions.Permissions_WebStoreInstall");

  // Matches the AddRef in RunImpl().
  Release();
}

void WebstorePrivateBeginInstallWithManifest3Function::InstallUIAbort(
    bool user_initiated) {
  error_ = kUserCancelledError;
  SetResultCode(USER_CANCELLED);
  g_pending_installs.Get().EraseInstall(GetProfile(), params_->details.id);
  SendResponse(false);

  // The web store install histograms are a subset of the install histograms.
  // We need to record both histograms here since CrxInstaller::InstallUIAbort
  // is never called for web store install cancellations.
  std::string histogram_name = user_initiated ?
      "Extensions.Permissions_WebStoreInstallCancel" :
      "Extensions.Permissions_WebStoreInstallAbort";
  ExtensionService::RecordPermissionMessagesHistogram(dummy_extension_.get(),
                                                      histogram_name.c_str());

  histogram_name = user_initiated ?
      "Extensions.Permissions_InstallCancel" :
      "Extensions.Permissions_InstallAbort";
  ExtensionService::RecordPermissionMessagesHistogram(dummy_extension_.get(),
                                                      histogram_name.c_str());

  // Matches the AddRef in RunImpl().
  Release();
}

WebstorePrivateCompleteInstallFunction::
    WebstorePrivateCompleteInstallFunction() {}

WebstorePrivateCompleteInstallFunction::
    ~WebstorePrivateCompleteInstallFunction() {}

bool WebstorePrivateCompleteInstallFunction::RunImpl() {
  scoped_ptr<CompleteInstall::Params> params(
      CompleteInstall::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!extensions::Extension::IdIsValid(params->expected_id)) {
    error_ = kInvalidIdError;
    return false;
  }

  approval_ = g_pending_approvals.Get()
                  .PopApproval(GetProfile(), params->expected_id)
                  .Pass();
  if (!approval_) {
    error_ = ErrorUtils::FormatErrorMessage(
        kNoPreviousBeginInstallWithManifestError, params->expected_id);
    return false;
  }

  // Balanced in OnExtensionInstallSuccess() or OnExtensionInstallFailure().
  AddRef();
  AppListService* app_list_service =
      AppListService::Get(GetCurrentBrowser()->host_desktop_type());

  if (approval_->enable_launcher) {
    app_list_service->EnableAppList(GetProfile(),
                                    AppListService::ENABLE_FOR_APP_INSTALL);
  }

  if (IsAppLauncherEnabled() && approval_->manifest->is_app()) {
    // Show the app list to show download is progressing. Don't show the app
    // list on first app install so users can be trained to open it themselves.
    if (approval_->enable_launcher)
      app_list_service->CreateForProfile(GetProfile());
    else
      app_list_service->AutoShowForProfile(GetProfile());
  }

  // The extension will install through the normal extension install flow, but
  // the whitelist entry will bypass the normal permissions install dialog.
  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      GetProfile(),
      this,
      dispatcher()->delegate()->GetAssociatedWebContents(),
      params->expected_id,
      approval_.Pass(),
      WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();

  return true;
}

void WebstorePrivateCompleteInstallFunction::OnExtensionInstallSuccess(
    const std::string& id) {
  if (test_webstore_installer_delegate)
    test_webstore_installer_delegate->OnExtensionInstallSuccess(id);

  VLOG(1) << "Install success, sending response";
  g_pending_installs.Get().EraseInstall(GetProfile(), id);
  SendResponse(true);

  RecordWebstoreExtensionInstallResult(true);

  // Matches the AddRef in RunImpl().
  Release();
}

void WebstorePrivateCompleteInstallFunction::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  if (test_webstore_installer_delegate) {
    test_webstore_installer_delegate->OnExtensionInstallFailure(
        id, error, reason);
  }

  error_ = error;
  VLOG(1) << "Install failed, sending response";
  g_pending_installs.Get().EraseInstall(GetProfile(), id);
  SendResponse(false);

  RecordWebstoreExtensionInstallResult(false);

  // Matches the AddRef in RunImpl().
  Release();
}

WebstorePrivateEnableAppLauncherFunction::
    WebstorePrivateEnableAppLauncherFunction() {}

WebstorePrivateEnableAppLauncherFunction::
    ~WebstorePrivateEnableAppLauncherFunction() {}

bool WebstorePrivateEnableAppLauncherFunction::RunImpl() {
  AppListService::Get(GetCurrentBrowser()->host_desktop_type())
      ->EnableAppList(GetProfile(), AppListService::ENABLE_VIA_WEBSTORE_LINK);
  return true;
}

bool WebstorePrivateGetBrowserLoginFunction::RunImpl() {
  GetBrowserLogin::Results::Info info;
  info.login = GetProfile()->GetOriginalProfile()->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  results_ = GetBrowserLogin::Results::Create(info);
  return true;
}

bool WebstorePrivateGetStoreLoginFunction::RunImpl() {
  results_ = GetStoreLogin::Results::Create(GetWebstoreLogin(GetProfile()));
  return true;
}

bool WebstorePrivateSetStoreLoginFunction::RunImpl() {
  scoped_ptr<SetStoreLogin::Params> params(
      SetStoreLogin::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  SetWebstoreLogin(GetProfile(), params->login);
  return true;
}

WebstorePrivateGetWebGLStatusFunction::WebstorePrivateGetWebGLStatusFunction() {
  feature_checker_ = new GPUFeatureChecker(
      gpu::GPU_FEATURE_TYPE_WEBGL,
      base::Bind(&WebstorePrivateGetWebGLStatusFunction::OnFeatureCheck,
          base::Unretained(this)));
}

WebstorePrivateGetWebGLStatusFunction::
    ~WebstorePrivateGetWebGLStatusFunction() {}

void WebstorePrivateGetWebGLStatusFunction::CreateResult(bool webgl_allowed) {
  results_ = GetWebGLStatus::Results::Create(GetWebGLStatus::Results::
      ParseWebgl_status(webgl_allowed ? "webgl_allowed" : "webgl_blocked"));
}

bool WebstorePrivateGetWebGLStatusFunction::RunImpl() {
  feature_checker_->CheckGPUFeatureAvailability();
  return true;
}

void WebstorePrivateGetWebGLStatusFunction::
    OnFeatureCheck(bool feature_allowed) {
  CreateResult(feature_allowed);
  SendResponse(true);
}

bool WebstorePrivateGetIsLauncherEnabledFunction::RunImpl() {
  results_ = GetIsLauncherEnabled::Results::Create(IsAppLauncherEnabled());
  return true;
}

bool WebstorePrivateIsInIncognitoModeFunction::RunImpl() {
  results_ = IsInIncognitoMode::Results::Create(
      GetProfile() != GetProfile()->GetOriginalProfile());
  return true;
}

}  // namespace extensions
