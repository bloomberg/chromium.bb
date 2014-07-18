// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_standalone_installer.h"

#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using content::WebContents;

namespace extensions {

const char kInvalidWebstoreItemId[] = "Invalid Chrome Web Store item ID";
const char kWebstoreRequestError[] =
    "Could not fetch data from the Chrome Web Store";
const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";
const char kExtensionIsBlacklisted[] = "Extension is blacklisted";
const char kInstallInProgressError[] = "An install is already in progress";
const char kLaunchInProgressError[] = "A launch is already in progress";

WebstoreStandaloneInstaller::WebstoreStandaloneInstaller(
    const std::string& webstore_item_id,
    Profile* profile,
    const Callback& callback)
    : id_(webstore_item_id),
      callback_(callback),
      profile_(profile),
      install_source_(WebstoreInstaller::INSTALL_SOURCE_INLINE),
      show_user_count_(true),
      average_rating_(0.0),
      rating_count_(0) {
}

void WebstoreStandaloneInstaller::BeginInstall() {
  // Add a ref to keep this alive for WebstoreDataFetcher.
  // All code paths from here eventually lead to either CompleteInstall or
  // AbortInstall, which both release this ref.
  AddRef();

  if (!Extension::IdIsValid(id_)) {
    CompleteInstall(webstore_install::INVALID_ID, kInvalidWebstoreItemId);
    return;
  }

  webstore_install::Result result = webstore_install::UNKNOWN_ERROR;
  std::string error;
  if (!EnsureUniqueInstall(&result, &error)) {
    CompleteInstall(result, error);
    return;
  }

  // Use the requesting page as the referrer both since that is more correct
  // (it is the page that caused this request to happen) and so that we can
  // track top sites that trigger inline install requests.
  webstore_data_fetcher_.reset(new WebstoreDataFetcher(
      this,
      profile_->GetRequestContext(),
      GetRequestorURL(),
      id_));
  webstore_data_fetcher_->Start();
}

//
// Private interface implementation.
//

WebstoreStandaloneInstaller::~WebstoreStandaloneInstaller() {
}

void WebstoreStandaloneInstaller::AbortInstall() {
  callback_.Reset();
  // Abort any in-progress fetches.
  if (webstore_data_fetcher_) {
    webstore_data_fetcher_.reset();
    scoped_active_install_.reset();
    Release();  // Matches the AddRef in BeginInstall.
  }
}

bool WebstoreStandaloneInstaller::EnsureUniqueInstall(
    webstore_install::Result* reason,
    std::string* error) {
  InstallTracker* tracker = InstallTracker::Get(profile_);
  DCHECK(tracker);

  const ActiveInstallData* existing_install_data =
      tracker->GetActiveInstall(id_);
  if (existing_install_data) {
    if (existing_install_data->is_ephemeral) {
      *reason = webstore_install::LAUNCH_IN_PROGRESS;
      *error = kLaunchInProgressError;
    } else {
      *reason = webstore_install::INSTALL_IN_PROGRESS;
      *error = kInstallInProgressError;
    }
    return false;
  }

  ActiveInstallData install_data(id_);
  InitInstallData(&install_data);
  scoped_active_install_.reset(new ScopedActiveInstall(tracker, install_data));
  return true;
}

void WebstoreStandaloneInstaller::CompleteInstall(
    webstore_install::Result result,
    const std::string& error) {
  scoped_active_install_.reset();
  if (!callback_.is_null())
    callback_.Run(result == webstore_install::SUCCESS, error);
  Release();  // Matches the AddRef in BeginInstall.
}

void WebstoreStandaloneInstaller::ProceedWithInstallPrompt() {
  install_prompt_ = CreateInstallPrompt();
  if (install_prompt_) {
    ShowInstallUI();
    // Control flow finishes up in InstallUIProceed or InstallUIAbort.
  } else {
    InstallUIProceed();
  }
}

scoped_refptr<const Extension>
WebstoreStandaloneInstaller::GetLocalizedExtensionForDisplay() {
  if (!localized_extension_for_display_.get()) {
    DCHECK(manifest_.get());
    if (!manifest_.get())
      return NULL;

    std::string error;
    localized_extension_for_display_ =
        ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
            manifest_.get(),
            Extension::REQUIRE_KEY | Extension::FROM_WEBSTORE,
            id_,
            localized_name_,
            localized_description_,
            &error);
  }
  return localized_extension_for_display_.get();
}

void WebstoreStandaloneInstaller::InitInstallData(
    ActiveInstallData* install_data) const {
  // Default implementation sets no properties.
}

void WebstoreStandaloneInstaller::OnManifestParsed() {
  ProceedWithInstallPrompt();
}

scoped_ptr<ExtensionInstallPrompt>
WebstoreStandaloneInstaller::CreateInstallUI() {
  return make_scoped_ptr(new ExtensionInstallPrompt(GetWebContents()));
}

scoped_ptr<WebstoreInstaller::Approval>
WebstoreStandaloneInstaller::CreateApproval() const {
  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          profile_,
          id_,
          scoped_ptr<base::DictionaryValue>(manifest_.get()->DeepCopy()),
          true));
  approval->skip_post_install_ui = !ShouldShowPostInstallUI();
  approval->use_app_installed_bubble = ShouldShowAppInstalledBubble();
  approval->installing_icon = gfx::ImageSkia::CreateFrom1xBitmap(icon_);
  return approval.Pass();
}

void WebstoreStandaloneInstaller::OnWebstoreRequestFailure() {
  OnWebStoreDataFetcherDone();
  CompleteInstall(webstore_install::WEBSTORE_REQUEST_ERROR,
                  kWebstoreRequestError);
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseSuccess(
    scoped_ptr<base::DictionaryValue> webstore_data) {
  OnWebStoreDataFetcherDone();

  if (!CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  std::string error;

  if (!CheckInlineInstallPermitted(*webstore_data, &error)) {
    CompleteInstall(webstore_install::NOT_PERMITTED, error);
    return;
  }

  if (!CheckRequestorPermitted(*webstore_data, &error)) {
    CompleteInstall(webstore_install::NOT_PERMITTED, error);
    return;
  }

  // Manifest, number of users, average rating and rating count are required.
  std::string manifest;
  if (!webstore_data->GetString(kManifestKey, &manifest) ||
      !webstore_data->GetString(kUsersKey, &localized_user_count_) ||
      !webstore_data->GetDouble(kAverageRatingKey, &average_rating_) ||
      !webstore_data->GetInteger(kRatingCountKey, &rating_count_)) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    kInvalidWebstoreResponseError);
    return;
  }

  // Optional.
  show_user_count_ = true;
  webstore_data->GetBoolean(kShowUserCountKey, &show_user_count_);

  if (average_rating_ < ExtensionInstallPrompt::kMinExtensionRating ||
      average_rating_ > ExtensionInstallPrompt::kMaxExtensionRating) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    kInvalidWebstoreResponseError);
    return;
  }

  // Localized name and description are optional.
  if ((webstore_data->HasKey(kLocalizedNameKey) &&
      !webstore_data->GetString(kLocalizedNameKey, &localized_name_)) ||
      (webstore_data->HasKey(kLocalizedDescriptionKey) &&
      !webstore_data->GetString(
          kLocalizedDescriptionKey, &localized_description_))) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    kInvalidWebstoreResponseError);
    return;
  }

  // Icon URL is optional.
  GURL icon_url;
  if (webstore_data->HasKey(kIconUrlKey)) {
    std::string icon_url_string;
    if (!webstore_data->GetString(kIconUrlKey, &icon_url_string)) {
      CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                      kInvalidWebstoreResponseError);
      return;
    }
    icon_url = GURL(extension_urls::GetWebstoreLaunchURL()).Resolve(
        icon_url_string);
    if (!icon_url.is_valid()) {
      CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                      kInvalidWebstoreResponseError);
      return;
    }
  }

  // Assume ownership of webstore_data.
  webstore_data_ = webstore_data.Pass();

  scoped_refptr<WebstoreInstallHelper> helper =
      new WebstoreInstallHelper(this,
                                id_,
                                manifest,
                                std::string(),  // We don't have any icon data.
                                icon_url,
                                profile_->GetRequestContext());
  // The helper will call us back via OnWebstoreParseSucces or
  // OnWebstoreParseFailure.
  helper->Start();
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseFailure(
    const std::string& error) {
  OnWebStoreDataFetcherDone();
  CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE, error);
}

void WebstoreStandaloneInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* manifest) {
  CHECK_EQ(id_, id);

  if (!CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  manifest_.reset(manifest);
  icon_ = icon;

  OnManifestParsed();
}

void WebstoreStandaloneInstaller::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  webstore_install::Result install_result = webstore_install::UNKNOWN_ERROR;
  switch (result_code) {
    case WebstoreInstallHelper::Delegate::MANIFEST_ERROR:
      install_result = webstore_install::INVALID_MANIFEST;
      break;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      install_result = webstore_install::ICON_ERROR;
      break;
    default:
      break;
  }

  CompleteInstall(install_result, error_message);
}

void WebstoreStandaloneInstaller::InstallUIProceed() {
  if (!CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  scoped_ptr<WebstoreInstaller::Approval> approval = CreateApproval();

  ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  const Extension* installed_extension =
      extension_service->GetExtensionById(id_, true /* include disabled */);
  if (installed_extension) {
    std::string install_message;
    webstore_install::Result install_result = webstore_install::SUCCESS;
    bool done = true;

    if (ExtensionPrefs::Get(profile_)->IsExtensionBlacklisted(id_)) {
      // Don't install a blacklisted extension.
      install_result = webstore_install::BLACKLISTED;
      install_message = kExtensionIsBlacklisted;
    } else if (util::IsEphemeralApp(installed_extension->id(), profile_) &&
               !approval->is_ephemeral) {
      // If the target extension has already been installed ephemerally and is
      // up to date, it can be promoted to a regular installed extension and
      // downloading from the Web Store is not necessary.
      const Extension* extension_to_install = GetLocalizedExtensionForDisplay();
      if (!extension_to_install) {
        CompleteInstall(webstore_install::INVALID_MANIFEST,
                        kInvalidManifestError);
        return;
      }

      if (installed_extension->version()->CompareTo(
              *extension_to_install->version()) < 0) {
        // If the existing extension is out of date, proceed with the install
        // to update the extension.
        done = false;
      } else {
        extension_service->PromoteEphemeralApp(installed_extension, false);
      }
    } else if (!extension_service->IsExtensionEnabled(id_)) {
      // If the extension is installed but disabled, and not blacklisted,
      // enable it.
      extension_service->EnableExtension(id_);
    }  // else extension is installed and enabled; no work to be done.

    if (done) {
      CompleteInstall(install_result, install_message);
      return;
    }
  }

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile_,
      this,
      GetWebContents(),
      id_,
      approval.Pass(),
      install_source_);
  installer->Start();
}

void WebstoreStandaloneInstaller::InstallUIAbort(bool user_initiated) {
  CompleteInstall(webstore_install::USER_CANCELLED, kUserCancelledError);
}

void WebstoreStandaloneInstaller::OnExtensionInstallSuccess(
    const std::string& id) {
  CHECK_EQ(id_, id);
  CompleteInstall(webstore_install::SUCCESS, std::string());
}

void WebstoreStandaloneInstaller::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  CHECK_EQ(id_, id);

  webstore_install::Result install_result = webstore_install::UNKNOWN_ERROR;
  switch (reason) {
    case WebstoreInstaller::FAILURE_REASON_CANCELLED:
      install_result = webstore_install::USER_CANCELLED;
      break;
    case WebstoreInstaller::FAILURE_REASON_DEPENDENCY_NOT_FOUND:
    case WebstoreInstaller::FAILURE_REASON_DEPENDENCY_NOT_SHARED_MODULE:
      install_result = webstore_install::MISSING_DEPENDENCIES;
      break;
    default:
      break;
  }

  CompleteInstall(install_result, error);
}

void WebstoreStandaloneInstaller::ShowInstallUI() {
  const Extension* localized_extension = GetLocalizedExtensionForDisplay();
  if (!localized_extension) {
    CompleteInstall(webstore_install::INVALID_MANIFEST, kInvalidManifestError);
    return;
  }

  install_ui_ = CreateInstallUI();
  install_ui_->ConfirmStandaloneInstall(
      this, localized_extension, &icon_, install_prompt_);
}

void WebstoreStandaloneInstaller::OnWebStoreDataFetcherDone() {
  // An instance of this class is passed in as a delegate for the
  // WebstoreInstallHelper, ExtensionInstallPrompt and WebstoreInstaller, and
  // therefore needs to remain alive until they are done. Clear the webstore
  // data fetcher to avoid calling Release in AbortInstall while any of these
  // operations are in progress.
  webstore_data_fetcher_.reset();
}

}  // namespace extensions
