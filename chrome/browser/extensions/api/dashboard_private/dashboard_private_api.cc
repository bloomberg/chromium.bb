// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dashboard_private/dashboard_private_api.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace extensions {

namespace ShowPermissionPromptForDelegatedInstall =
    api::dashboard_private::ShowPermissionPromptForDelegatedInstall;
namespace ShowPermissionPromptForDelegatedBundleInstall =
    api::dashboard_private::ShowPermissionPromptForDelegatedBundleInstall;

namespace {

// Error messages that can be returned by the API.
const char kAlreadyInstalledError[] = "This item is already installed";
const char kCannotSpecifyIconDataAndUrlError[] =
    "You cannot specify both icon data and an icon url";
const char kInvalidBundleError[] = "Invalid bundle";
const char kInvalidIconUrlError[] = "Invalid icon url";
const char kInvalidIdError[] = "Invalid id";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";

api::dashboard_private::Result WebstoreInstallHelperResultToApiResult(
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result) {
  switch (result) {
    case WebstoreInstallHelper::Delegate::UNKNOWN_ERROR:
      return api::dashboard_private::RESULT_UNKNOWN_ERROR;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      return api::dashboard_private::RESULT_ICON_ERROR;
    case WebstoreInstallHelper::Delegate::MANIFEST_ERROR:
      return api::dashboard_private::RESULT_MANIFEST_ERROR;
  }
  NOTREACHED();
  return api::dashboard_private::RESULT_NONE;
}

}  // namespace

DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::
    DashboardPrivateShowPermissionPromptForDelegatedInstallFunction() {
}

DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::
    ~DashboardPrivateShowPermissionPromptForDelegatedInstallFunction() {
}

ExtensionFunction::ResponseAction
DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::Run() {
  params_ = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (!crx_file::id_util::IdIsValid(params_->details.id)) {
    return RespondNow(BuildResponse(api::dashboard_private::RESULT_INVALID_ID,
                                    kInvalidIdError));
  }

  if (params_->details.icon_data && params_->details.icon_url) {
    return RespondNow(BuildResponse(api::dashboard_private::RESULT_ICON_ERROR,
                                    kCannotSpecifyIconDataAndUrlError));
  }

  GURL icon_url;
  if (params_->details.icon_url) {
    icon_url = source_url().Resolve(*params_->details.icon_url);
    if (!icon_url.is_valid()) {
      return RespondNow(BuildResponse(
          api::dashboard_private::RESULT_INVALID_ICON_URL,
          kInvalidIconUrlError));
    }
  }

  net::URLRequestContextGetter* context_getter = nullptr;
  if (!icon_url.is_empty())
    context_getter = browser_context()->GetRequestContext();

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this, params_->details.id, params_->details.manifest, icon_url,
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

void DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::
    OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* parsed_manifest) {
  CHECK_EQ(params_->details.id, id);
  CHECK(parsed_manifest);

  std::string localized_name = params_->details.localized_name ?
      *params_->details.localized_name : std::string();

  std::string error;
  dummy_extension_ = ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
      parsed_manifest,
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
    Respond(BuildResponse(api::dashboard_private::RESULT_USER_CANCELLED,
                          kUserCancelledError));
    // Matches the AddRef in Run().
    Release();
    return;
  }
  install_prompt_.reset(new ExtensionInstallPrompt(web_contents));
  install_prompt_->ConfirmPermissionsForDelegatedInstall(
      this, dummy_extension_.get(), details().delegated_user, &icon);
  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::
    OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result,
    const std::string& error_message) {
  CHECK_EQ(params_->details.id, id);

  Respond(BuildResponse(WebstoreInstallHelperResultToApiResult(result),
                        error_message));

  // Matches the AddRef in Run().
  Release();
}

void DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::
    InstallUIProceed() {
  Respond(BuildResponse(api::dashboard_private::RESULT_SUCCESS, std::string()));

  // Matches the AddRef in Run().
  Release();
}

void DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::
    InstallUIAbort(bool user_initiated) {
  Respond(BuildResponse(api::dashboard_private::RESULT_USER_CANCELLED,
                        kUserCancelledError));

  // Matches the AddRef in Run().
  Release();
}

ExtensionFunction::ResponseValue
DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::BuildResponse(
    api::dashboard_private::Result result, const std::string& error) {
  if (result != api::dashboard_private::RESULT_SUCCESS)
    return ErrorWithArguments(CreateResults(result), error);

  // The web store expects an empty string on success, so don't use
  // RESULT_SUCCESS here.
  return ArgumentList(
      CreateResults(api::dashboard_private::RESULT_EMPTY_STRING));
}

scoped_ptr<base::ListValue>
DashboardPrivateShowPermissionPromptForDelegatedInstallFunction::CreateResults(
    api::dashboard_private::Result result) const {
  return ShowPermissionPromptForDelegatedInstall::Results::Create(result);
}

DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::
    DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction()
    : chrome_details_(this) {
}

DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::
    ~DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction() {
}

ExtensionFunction::ResponseAction
DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::Run() {
  params_ = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (params_->contents.empty())
    return RespondNow(Error(kInvalidBundleError));

  if (params_->details.icon_url) {
    GURL icon_url = source_url().Resolve(*params_->details.icon_url);
    if (!icon_url.is_valid())
      return RespondNow(Error(kInvalidIconUrlError));

    // The bitmap fetcher will call us back via OnFetchComplete.
    icon_fetcher_.reset(new chrome::BitmapFetcher(icon_url, this));
    icon_fetcher_->Init(
        browser_context()->GetRequestContext(), std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES);
    icon_fetcher_->Start();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(
        &DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::
            OnFetchComplete,
        this, GURL(), nullptr));
  }

  AddRef();  // Balanced in OnFetchComplete.

  // The response is sent in OnFetchComplete or OnInstallApproval.
  return RespondLater();
}

void DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::
    OnFetchComplete(const GURL& url, const SkBitmap* bitmap) {
  BundleInstaller::ItemList items;
  for (const auto& entry : params_->contents) {
    BundleInstaller::Item item;
    item.id = entry->id;
    item.manifest = entry->manifest;
    item.localized_name = entry->localized_name;
    if (entry->icon_url)
      item.icon_url = source_url().Resolve(*entry->icon_url);
    items.push_back(item);
  }
  if (items.empty()) {
    Respond(Error(kAlreadyInstalledError));
    Release();  // Matches the AddRef in Run.
    return;
  }

  bundle_.reset(new BundleInstaller(chrome_details_.GetCurrentBrowser(),
                                    params_->details.localized_name,
                                    bitmap ? *bitmap : SkBitmap(),
                                    std::string(), details().delegated_user,
                                    items));

  bundle_->PromptForApproval(base::Bind(
      &DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::
          OnInstallApproval,
      this));

  Release();  // Matches the AddRef in Run.
}

void DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction::
    OnInstallApproval(BundleInstaller::ApprovalState state) {
  if (state != BundleInstaller::APPROVED) {
    Respond(Error(state == BundleInstaller::USER_CANCELED
                      ? kUserCancelledError
                      : kInvalidBundleError));
    return;
  }

  Respond(NoArguments());
}

}  // namespace extensions

