// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"

#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/apps/ephemeral_app_launcher.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/gpu/gpu_feature_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace BeginInstallWithManifest3 =
    api::webstore_private::BeginInstallWithManifest3;
namespace CompleteInstall = api::webstore_private::CompleteInstall;
namespace GetBrowserLogin = api::webstore_private::GetBrowserLogin;
namespace GetEphemeralAppsEnabled =
    api::webstore_private::GetEphemeralAppsEnabled;
namespace GetIsLauncherEnabled = api::webstore_private::GetIsLauncherEnabled;
namespace GetStoreLogin = api::webstore_private::GetStoreLogin;
namespace GetWebGLStatus = api::webstore_private::GetWebGLStatus;
namespace IsInIncognitoMode = api::webstore_private::IsInIncognitoMode;
namespace LaunchEphemeralApp = api::webstore_private::LaunchEphemeralApp;
namespace SetStoreLogin = api::webstore_private::SetStoreLogin;
namespace ShowPermissionPromptForDelegatedInstall =
    api::webstore_private::ShowPermissionPromptForDelegatedInstall;

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

chrome::HostDesktopType GetHostDesktopTypeForWebContents(
    content::WebContents* contents) {
  return chrome::GetHostDesktopTypeForNativeWindow(
      contents->GetTopLevelNativeWindow());
}

api::webstore_private::Result WebstoreInstallResultToApiResult(
    webstore_install::Result result) {
  switch (result) {
    case webstore_install::SUCCESS:
      return api::webstore_private::RESULT_SUCCESS;
    case webstore_install::OTHER_ERROR:
      return api::webstore_private::RESULT_UNKNOWN_ERROR;
    case webstore_install::INVALID_ID:
      return api::webstore_private::RESULT_INVALID_ID;
    case webstore_install::NOT_PERMITTED:
    case webstore_install::WEBSTORE_REQUEST_ERROR:
    case webstore_install::INVALID_WEBSTORE_RESPONSE:
      return api::webstore_private::RESULT_INSTALL_ERROR;
    case webstore_install::INVALID_MANIFEST:
      return api::webstore_private::RESULT_MANIFEST_ERROR;
    case webstore_install::ICON_ERROR:
      return api::webstore_private::RESULT_ICON_ERROR;
    case webstore_install::ABORTED:
    case webstore_install::USER_CANCELLED:
      return api::webstore_private::RESULT_USER_CANCELLED;
    case webstore_install::BLACKLISTED:
      return api::webstore_private::RESULT_BLACKLISTED;
    case webstore_install::MISSING_DEPENDENCIES:
    case webstore_install::REQUIREMENT_VIOLATIONS:
      return api::webstore_private::RESULT_MISSING_DEPENDENCIES;
    case webstore_install::BLOCKED_BY_POLICY:
      return api::webstore_private::RESULT_BLOCKED_BY_POLICY;
    case webstore_install::LAUNCH_FEATURE_DISABLED:
      return api::webstore_private::RESULT_FEATURE_DISABLED;
    case webstore_install::LAUNCH_UNSUPPORTED_EXTENSION_TYPE:
      return api::webstore_private::RESULT_UNSUPPORTED_EXTENSION_TYPE;
    case webstore_install::INSTALL_IN_PROGRESS:
      return api::webstore_private::RESULT_INSTALL_IN_PROGRESS;
    case webstore_install::LAUNCH_IN_PROGRESS:
      return api::webstore_private::RESULT_LAUNCH_IN_PROGRESS;
  }
  NOTREACHED();
  return api::webstore_private::RESULT_NONE;
}

api::webstore_private::Result WebstoreInstallHelperResultToApiResult(
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result) {
  switch (result) {
    case WebstoreInstallHelper::Delegate::UNKNOWN_ERROR:
      return api::webstore_private::RESULT_UNKNOWN_ERROR;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      return api::webstore_private::RESULT_ICON_ERROR;
    case WebstoreInstallHelper::Delegate::MANIFEST_ERROR:
      return api::webstore_private::RESULT_MANIFEST_ERROR;
  }
  NOTREACHED();
  return api::webstore_private::RESULT_NONE;
}

static base::LazyInstance<PendingApprovals> g_pending_approvals =
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

template<typename Params>
WebstorePrivateFunctionWithPermissionPrompt<Params>::
    WebstorePrivateFunctionWithPermissionPrompt() {
}

template<typename Params>
WebstorePrivateFunctionWithPermissionPrompt<Params>::
    ~WebstorePrivateFunctionWithPermissionPrompt() {
}

template<typename Params>
ExtensionFunction::ResponseValue
WebstorePrivateFunctionWithPermissionPrompt<Params>::RunExtraForResponse() {
  return ExtensionFunction::ResponseValue();
}

template<typename Params>
ExtensionFunction::ResponseValue
WebstorePrivateFunctionWithPermissionPrompt<Params>::BuildResponse(
    api::webstore_private::Result result, const std::string& error) {
  if (result != api::webstore_private::RESULT_SUCCESS)
    return ErrorWithArguments(CreateResults(result), error);

  // The web store expects an empty string on success, so don't use
  // RESULT_SUCCESS here.
  return ArgumentList(
      CreateResults(api::webstore_private::RESULT_EMPTY_STRING));
}

template<typename Params>
scoped_ptr<base::DictionaryValue>
WebstorePrivateFunctionWithPermissionPrompt<Params>::PassParsedManifest() {
  return parsed_manifest_.Pass();
}

template<typename Params>
ExtensionFunction::ResponseAction
WebstorePrivateFunctionWithPermissionPrompt<Params>::Run() {
  params_ = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (!crx_file::id_util::IdIsValid(params_->details.id)) {
    return RespondNow(BuildResponse(api::webstore_private::RESULT_INVALID_ID,
                                    kInvalidIdError));
  }

  if (params_->details.icon_data && params_->details.icon_url) {
    return RespondNow(BuildResponse(api::webstore_private::RESULT_ICON_ERROR,
                                    kCannotSpecifyIconDataAndUrlError));
  }

  GURL icon_url;
  if (params_->details.icon_url) {
    icon_url = source_url().Resolve(*params_->details.icon_url);
    if (!icon_url.is_valid()) {
      return RespondNow(BuildResponse(
          api::webstore_private::RESULT_INVALID_ICON_URL,
          kInvalidIconUrlError));
    }
  }

  std::string icon_data = params_->details.icon_data ?
      *params_->details.icon_data : std::string();

  ExtensionFunction::ResponseValue response = RunExtraForResponse();
  if (response)
    return RespondNow(response.Pass());

  net::URLRequestContextGetter* context_getter = NULL;
  if (!icon_url.is_empty())
    context_getter = browser_context()->GetRequestContext();

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
  return RespondLater();
}

template<typename Params>
void
WebstorePrivateFunctionWithPermissionPrompt<Params>::OnWebstoreParseSuccess(
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

  content::WebContents* web_contents = GetAssociatedWebContents();
  if (!web_contents) {
    // The browser window has gone away.
    Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                          kUserCancelledError));
    // Matches the AddRef in Run().
    Release();
    return;
  }
  install_prompt_.reset(new ExtensionInstallPrompt(web_contents));
  ShowPrompt(install_prompt_.get());
  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

template<typename Params>
void
WebstorePrivateFunctionWithPermissionPrompt<Params>::OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result,
    const std::string& error_message) {
  CHECK_EQ(params_->details.id, id);

  Respond(BuildResponse(WebstoreInstallHelperResultToApiResult(result),
                        error_message));

  // Matches the AddRef in Run().
  Release();
}

template<typename Params>
void WebstorePrivateFunctionWithPermissionPrompt<Params>::InstallUIProceed() {
  InstallUIProceedHook();

  Respond(BuildResponse(api::webstore_private::RESULT_SUCCESS, std::string()));

  // Matches the AddRef in Run().
  Release();
}

template<typename Params>
void WebstorePrivateFunctionWithPermissionPrompt<Params>::InstallUIAbort(
    bool user_initiated) {
  InstallUIAbortHook(user_initiated);

  Respond(BuildResponse(api::webstore_private::RESULT_USER_CANCELLED,
                        kUserCancelledError));

  // Matches the AddRef in Run().
  Release();
}

WebstorePrivateBeginInstallWithManifest3Function::
    WebstorePrivateBeginInstallWithManifest3Function() : chrome_details_(this) {
}

WebstorePrivateBeginInstallWithManifest3Function::
    ~WebstorePrivateBeginInstallWithManifest3Function() {
}

ExtensionFunction::ResponseValue
WebstorePrivateBeginInstallWithManifest3Function::RunExtraForResponse() {
  InstallTracker* tracker = InstallTracker::Get(browser_context());
  DCHECK(tracker);
  if (util::IsExtensionInstalledPermanently(params().details.id,
                                            browser_context()) ||
      tracker->GetActiveInstall(params().details.id)) {
    return BuildResponse(api::webstore_private::RESULT_ALREADY_INSTALLED,
                         kAlreadyInstalledError);
  }
  ActiveInstallData install_data(params().details.id);
  scoped_active_install_.reset(new ScopedActiveInstall(tracker, install_data));
  return ExtensionFunction::ResponseValue();
}

void WebstorePrivateBeginInstallWithManifest3Function::InstallUIProceedHook() {
  // This gets cleared in CrxInstaller::ConfirmInstall(). TODO(asargent) - in
  // the future we may also want to add time-based expiration, where a whitelist
  // entry is only valid for some number of minutes.
  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          chrome_details_.GetProfile(),
          params().details.id,
          PassParsedManifest(),
          false));
  approval->use_app_installed_bubble = params().details.app_install_bubble;
  approval->enable_launcher = params().details.enable_launcher;
  // If we are enabling the launcher, we should not show the app list in order
  // to train the user to open it themselves at least once.
  approval->skip_post_install_ui = params().details.enable_launcher;
  approval->dummy_extension = dummy_extension();
  approval->installing_icon = gfx::ImageSkia::CreateFrom1xBitmap(icon());
  if (params().details.authuser)
    approval->authuser = *params().details.authuser;
  g_pending_approvals.Get().PushApproval(approval.Pass());

  DCHECK(scoped_active_install_.get());
  scoped_active_install_->CancelDeregister();

  // The Permissions_Install histogram is recorded from the ExtensionService
  // for all extension installs, so we only need to record the web store
  // specific histogram here.
  ExtensionService::RecordPermissionMessagesHistogram(
      dummy_extension().get(), "Extensions.Permissions_WebStoreInstall2");
}

void WebstorePrivateBeginInstallWithManifest3Function::InstallUIAbortHook(
    bool user_initiated) {
  // The web store install histograms are a subset of the install histograms.
  // We need to record both histograms here since CrxInstaller::InstallUIAbort
  // is never called for web store install cancellations.
  std::string histogram_name =
      user_initiated ? "Extensions.Permissions_WebStoreInstallCancel2"
                     : "Extensions.Permissions_WebStoreInstallAbort2";
  ExtensionService::RecordPermissionMessagesHistogram(dummy_extension().get(),
                                                      histogram_name.c_str());

  histogram_name = user_initiated ? "Extensions.Permissions_InstallCancel2"
                                  : "Extensions.Permissions_InstallAbort2";
  ExtensionService::RecordPermissionMessagesHistogram(dummy_extension().get(),
                                                      histogram_name.c_str());
}

void WebstorePrivateBeginInstallWithManifest3Function::ShowPrompt(
    ExtensionInstallPrompt* install_prompt) {
  install_prompt->ConfirmWebstoreInstall(
      this, dummy_extension().get(), &icon(),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

scoped_ptr<base::ListValue>
WebstorePrivateBeginInstallWithManifest3Function::CreateResults(
    api::webstore_private::Result result) const {
  return BeginInstallWithManifest3::Results::Create(result);
}

WebstorePrivateCompleteInstallFunction::
    WebstorePrivateCompleteInstallFunction() : chrome_details_(this) {}

WebstorePrivateCompleteInstallFunction::
    ~WebstorePrivateCompleteInstallFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateCompleteInstallFunction::Run() {
  scoped_ptr<CompleteInstall::Params> params(
      CompleteInstall::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!crx_file::id_util::IdIsValid(params->expected_id))
    return RespondNow(Error(kInvalidIdError));

  approval_ =
      g_pending_approvals.Get().PopApproval(chrome_details_.GetProfile(),
                                            params->expected_id).Pass();
  if (!approval_) {
    return RespondNow(Error(kNoPreviousBeginInstallWithManifestError,
                            params->expected_id));
  }

  scoped_active_install_.reset(new ScopedActiveInstall(
      InstallTracker::Get(browser_context()), params->expected_id));

  AppListService* app_list_service = AppListService::Get(
      GetHostDesktopTypeForWebContents(GetAssociatedWebContents()));

  if (approval_->enable_launcher) {
    app_list_service->EnableAppList(chrome_details_.GetProfile(),
                                    AppListService::ENABLE_FOR_APP_INSTALL);
  }

  if (IsAppLauncherEnabled() && approval_->manifest->is_app()) {
    // Show the app list to show download is progressing. Don't show the app
    // list on first app install so users can be trained to open it themselves.
    app_list_service->ShowForAppInstall(
        chrome_details_.GetProfile(),
        params->expected_id,
        approval_->enable_launcher);
  }

  // If the target extension has already been installed ephemerally and is
  // up to date, it can be promoted to a regular installed extension and
  // downloading from the Web Store is not necessary.
  const Extension* extension = ExtensionRegistry::Get(browser_context())->
      GetExtensionById(params->expected_id, ExtensionRegistry::EVERYTHING);
  if (extension && approval_->dummy_extension.get() &&
      util::IsEphemeralApp(extension->id(), browser_context()) &&
      extension->version()->CompareTo(*approval_->dummy_extension->version()) >=
          0) {
    install_ui::ShowPostInstallUIForApproval(
        browser_context(), *approval_, extension);

    ExtensionService* extension_service =
        ExtensionSystem::Get(browser_context())->extension_service();
    extension_service->PromoteEphemeralApp(extension, false);
    OnInstallSuccess(extension->id());
    VLOG(1) << "Install success, sending response";
    return RespondNow(NoArguments());
  }

  // Balanced in OnExtensionInstallSuccess() or OnExtensionInstallFailure().
  AddRef();

  // The extension will install through the normal extension install flow, but
  // the whitelist entry will bypass the normal permissions install dialog.
  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      chrome_details_.GetProfile(),
      this,
      chrome_details_.GetAssociatedWebContents(),
      params->expected_id,
      approval_.Pass(),
      WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();

  return RespondLater();
}

void WebstorePrivateCompleteInstallFunction::OnExtensionInstallSuccess(
    const std::string& id) {
  OnInstallSuccess(id);
  VLOG(1) << "Install success, sending response";
  Respond(NoArguments());

  RecordWebstoreExtensionInstallResult(true);

  // Matches the AddRef in Run().
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

  VLOG(1) << "Install failed, sending response";
  Respond(Error(error));

  RecordWebstoreExtensionInstallResult(false);

  // Matches the AddRef in Run().
  Release();
}

void WebstorePrivateCompleteInstallFunction::OnInstallSuccess(
    const std::string& id) {
  if (test_webstore_installer_delegate)
    test_webstore_installer_delegate->OnExtensionInstallSuccess(id);
}

WebstorePrivateShowPermissionPromptForDelegatedInstallFunction::
    WebstorePrivateShowPermissionPromptForDelegatedInstallFunction() {
}

WebstorePrivateShowPermissionPromptForDelegatedInstallFunction::
    ~WebstorePrivateShowPermissionPromptForDelegatedInstallFunction() {
}

void WebstorePrivateShowPermissionPromptForDelegatedInstallFunction::ShowPrompt(
    ExtensionInstallPrompt* install_prompt) {
  install_prompt->ConfirmPermissionsForDelegatedInstall(
      this, dummy_extension().get(), params().details.delegated_user, &icon());
}

scoped_ptr<base::ListValue>
WebstorePrivateShowPermissionPromptForDelegatedInstallFunction::CreateResults(
    api::webstore_private::Result result) const {
  return ShowPermissionPromptForDelegatedInstall::Results::Create(result);
}

WebstorePrivateEnableAppLauncherFunction::
    WebstorePrivateEnableAppLauncherFunction() : chrome_details_(this) {}

WebstorePrivateEnableAppLauncherFunction::
    ~WebstorePrivateEnableAppLauncherFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateEnableAppLauncherFunction::Run() {
  AppListService* app_list_service = AppListService::Get(
      GetHostDesktopTypeForWebContents(
          chrome_details_.GetAssociatedWebContents()));
  app_list_service->EnableAppList(chrome_details_.GetProfile(),
                                  AppListService::ENABLE_VIA_WEBSTORE_LINK);
  return RespondNow(NoArguments());
}

WebstorePrivateGetBrowserLoginFunction::
    WebstorePrivateGetBrowserLoginFunction() : chrome_details_(this) {}

WebstorePrivateGetBrowserLoginFunction::
    ~WebstorePrivateGetBrowserLoginFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetBrowserLoginFunction::Run() {
  GetBrowserLogin::Results::Info info;
  info.login = SigninManagerFactory::GetForProfile(
                   chrome_details_.GetProfile()->GetOriginalProfile())
                       ->GetAuthenticatedUsername();
  return RespondNow(ArgumentList(GetBrowserLogin::Results::Create(info)));
}

WebstorePrivateGetStoreLoginFunction::
    WebstorePrivateGetStoreLoginFunction() : chrome_details_(this) {}

WebstorePrivateGetStoreLoginFunction::
    ~WebstorePrivateGetStoreLoginFunction() {}

ExtensionFunction::ResponseAction WebstorePrivateGetStoreLoginFunction::Run() {
  return RespondNow(ArgumentList(GetStoreLogin::Results::Create(
      GetWebstoreLogin(chrome_details_.GetProfile()))));
}

WebstorePrivateSetStoreLoginFunction::
    WebstorePrivateSetStoreLoginFunction() : chrome_details_(this) {}

WebstorePrivateSetStoreLoginFunction::
    ~WebstorePrivateSetStoreLoginFunction() {}

ExtensionFunction::ResponseAction WebstorePrivateSetStoreLoginFunction::Run() {
  scoped_ptr<SetStoreLogin::Params> params(
      SetStoreLogin::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  SetWebstoreLogin(chrome_details_.GetProfile(), params->login);
  return RespondNow(NoArguments());
}

WebstorePrivateGetWebGLStatusFunction::WebstorePrivateGetWebGLStatusFunction()
  : feature_checker_(new GPUFeatureChecker(
        gpu::GPU_FEATURE_TYPE_WEBGL,
        base::Bind(&WebstorePrivateGetWebGLStatusFunction::OnFeatureCheck,
                   base::Unretained(this)))) {
}

WebstorePrivateGetWebGLStatusFunction::
    ~WebstorePrivateGetWebGLStatusFunction() {}

ExtensionFunction::ResponseAction WebstorePrivateGetWebGLStatusFunction::Run() {
  feature_checker_->CheckGPUFeatureAvailability();
  return RespondLater();
}

void WebstorePrivateGetWebGLStatusFunction::OnFeatureCheck(
    bool feature_allowed) {
  Respond(ArgumentList(GetWebGLStatus::Results::Create(
      GetWebGLStatus::Results::ParseWebgl_status(
          feature_allowed ? "webgl_allowed" : "webgl_blocked"))));
}

WebstorePrivateGetIsLauncherEnabledFunction::
    WebstorePrivateGetIsLauncherEnabledFunction() {}

WebstorePrivateGetIsLauncherEnabledFunction::
    ~WebstorePrivateGetIsLauncherEnabledFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetIsLauncherEnabledFunction::Run() {
  return RespondNow(ArgumentList(
      GetIsLauncherEnabled::Results::Create(IsAppLauncherEnabled())));
}

WebstorePrivateIsInIncognitoModeFunction::
    WebstorePrivateIsInIncognitoModeFunction() : chrome_details_(this) {}

WebstorePrivateIsInIncognitoModeFunction::
    ~WebstorePrivateIsInIncognitoModeFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateIsInIncognitoModeFunction::Run() {
  Profile* profile = chrome_details_.GetProfile();
  return RespondNow(ArgumentList(IsInIncognitoMode::Results::Create(
      profile != profile->GetOriginalProfile())));
}

WebstorePrivateLaunchEphemeralAppFunction::
    WebstorePrivateLaunchEphemeralAppFunction() : chrome_details_(this) {}

WebstorePrivateLaunchEphemeralAppFunction::
    ~WebstorePrivateLaunchEphemeralAppFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateLaunchEphemeralAppFunction::Run() {
  // Check whether the browser window still exists.
  content::WebContents* web_contents =
      chrome_details_.GetAssociatedWebContents();
  if (!web_contents)
    return RespondNow(Error("aborted"));

  if (!user_gesture()) {
    return RespondNow(BuildResponse(
        api::webstore_private::RESULT_USER_GESTURE_REQUIRED,
        "User gesture is required"));
  }

  scoped_ptr<LaunchEphemeralApp::Params> params(
      LaunchEphemeralApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  AddRef();  // Balanced in OnLaunchComplete()

  scoped_refptr<EphemeralAppLauncher> launcher =
      EphemeralAppLauncher::CreateForWebContents(
          params->id,
          web_contents,
          base::Bind(
              &WebstorePrivateLaunchEphemeralAppFunction::OnLaunchComplete,
              base::Unretained(this)));
  launcher->Start();

  return RespondLater();
}

void WebstorePrivateLaunchEphemeralAppFunction::OnLaunchComplete(
    webstore_install::Result result, const std::string& error) {
  Respond(BuildResponse(WebstoreInstallResultToApiResult(result), error));
  Release();  // Matches AddRef() in Run()
}

ExtensionFunction::ResponseValue
WebstorePrivateLaunchEphemeralAppFunction::BuildResponse(
    api::webstore_private::Result result, const std::string& error) {
  if (result != api::webstore_private::RESULT_SUCCESS) {
    std::string error_message;
    if (error.empty()) {
      error_message = base::StringPrintf(
          "[%s]", api::webstore_private::ToString(result).c_str());
    } else {
      error_message = base::StringPrintf(
          "[%s]: %s",
          api::webstore_private::ToString(result).c_str(),
          error.c_str());
    }
    return ErrorWithArguments(LaunchEphemeralApp::Results::Create(result),
                              error_message);
  }
  return ArgumentList(LaunchEphemeralApp::Results::Create(result));
}

WebstorePrivateGetEphemeralAppsEnabledFunction::
    WebstorePrivateGetEphemeralAppsEnabledFunction() {}

WebstorePrivateGetEphemeralAppsEnabledFunction::
    ~WebstorePrivateGetEphemeralAppsEnabledFunction() {}

ExtensionFunction::ResponseAction
WebstorePrivateGetEphemeralAppsEnabledFunction::Run() {
  return RespondNow(ArgumentList(GetEphemeralAppsEnabled::Results::Create(
      EphemeralAppLauncher::IsFeatureEnabled())));
}

}  // namespace extensions
