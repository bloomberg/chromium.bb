// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_standalone_installer.h"

#include "base/values.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

using content::WebContents;

namespace extensions {

const char kManifestKey[] = "manifest";
const char kIconUrlKey[] = "icon_url";
const char kLocalizedNameKey[] = "localized_name";
const char kLocalizedDescriptionKey[] = "localized_description";
const char kUsersKey[] = "users";
const char kAverageRatingKey[] = "average_rating";
const char kRatingCountKey[] = "rating_count";
const char kRedirectUrlKey[] = "redirect_url";

const char kInvalidWebstoreItemId[] = "Invalid Chrome Web Store item ID";
const char kWebstoreRequestError[] =
    "Could not fetch data from the Chrome Web Store";
const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";


WebstoreStandaloneInstaller::WebstoreStandaloneInstaller(
    const std::string& webstore_item_id,
    Profile* profile,
    const Callback& callback)
    : id_(webstore_item_id),
      callback_(callback),
      profile_(profile),
      average_rating_(0.0),
      rating_count_(0) {
  CHECK(!callback_.is_null());
}

WebstoreStandaloneInstaller::~WebstoreStandaloneInstaller() {}

//
// Private interface implementation.
//

void WebstoreStandaloneInstaller::BeginInstall() {
  AddRef();  // Balanced in CompleteInstall or WebContentsDestroyed.

  if (!Extension::IdIsValid(id_)) {
    CompleteInstall(kInvalidWebstoreItemId);
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

void WebstoreStandaloneInstaller::OnWebstoreRequestFailure() {
  CompleteInstall(kWebstoreRequestError);
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseSuccess(
    DictionaryValue* webstore_data) {
  if (!CheckRequestorAlive()) {
    CompleteInstall(std::string());
    return;
  }

  std::string error;

  if (!CheckInlineInstallPermitted(*webstore_data, &error)) {
    CompleteInstall(error);
    return;
  }

  if (!CheckRequestorPermitted(*webstore_data, &error)) {
    CompleteInstall(error);
    return;
  }

  // Manifest, number of users, average rating and rating count are required.
  std::string manifest;
  if (!webstore_data->GetString(kManifestKey, &manifest) ||
      !webstore_data->GetString(kUsersKey, &localized_user_count_) ||
      !webstore_data->GetDouble(kAverageRatingKey, &average_rating_) ||
      !webstore_data->GetInteger(kRatingCountKey, &rating_count_)) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  if (average_rating_ < ExtensionInstallPrompt::kMinExtensionRating ||
      average_rating_ > ExtensionInstallPrompt::kMaxExtensionRating) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  // Localized name and description are optional.
  if ((webstore_data->HasKey(kLocalizedNameKey) &&
      !webstore_data->GetString(kLocalizedNameKey, &localized_name_)) ||
      (webstore_data->HasKey(kLocalizedDescriptionKey) &&
      !webstore_data->GetString(
          kLocalizedDescriptionKey, &localized_description_))) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  // Icon URL is optional.
  GURL icon_url;
  if (webstore_data->HasKey(kIconUrlKey)) {
    std::string icon_url_string;
    if (!webstore_data->GetString(kIconUrlKey, &icon_url_string)) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }
    icon_url = GURL(extension_urls::GetWebstoreLaunchURL()).Resolve(
        icon_url_string);
    if (!icon_url.is_valid()) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }
  }

  // Assume ownership of webstore_data.
  webstore_data_.reset(webstore_data);

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
  CompleteInstall(error);
}

void WebstoreStandaloneInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* manifest) {
  CHECK_EQ(id_, id);

  if (!CheckRequestorAlive()) {
    CompleteInstall(std::string());
    return;
  }

  manifest_.reset(manifest);
  icon_ = icon;

  install_prompt_ = CreateInstallPrompt();
  if (install_prompt_) {
    CreateInstallUI();
    // Control flow finishes up in InstallUIProceed or InstallUIAbort.
  } else {
    InstallUIProceed();
  }
}

void WebstoreStandaloneInstaller::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  CompleteInstall(error_message);
}

void WebstoreStandaloneInstaller::InstallUIProceed() {
  if (!CheckRequestorAlive()) {
    CompleteInstall(std::string());
    return;
  }

  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          profile_,
          id_,
          scoped_ptr<base::DictionaryValue>(manifest_.get()->DeepCopy())));
  approval->skip_post_install_ui = !ShouldShowPostInstallUI();
  approval->use_app_installed_bubble = ShouldShowAppInstalledBubble();

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile_,
      this,
      &(GetWebContents()->GetController()),
      id_,
      approval.Pass(),
      WebstoreInstaller::FLAG_INLINE_INSTALL);
  installer->Start();
}

void WebstoreStandaloneInstaller::InstallUIAbort(bool user_initiated) {
  CompleteInstall(kUserCancelledError);
}

void WebstoreStandaloneInstaller::OnExtensionInstallSuccess(
    const std::string& id) {
  CHECK_EQ(id_, id);
  CompleteInstall(std::string());
}

void WebstoreStandaloneInstaller::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason cancelled) {
  CHECK_EQ(id_, id);
  CompleteInstall(error);
}

void WebstoreStandaloneInstaller::AbortInstall() {
  callback_.Reset();
  // Abort any in-progress fetches.
  if (webstore_data_fetcher_) {
    webstore_data_fetcher_.reset();
    Release();  // Matches the AddRef in BeginInstall.
  }
}

void WebstoreStandaloneInstaller::CompleteInstall(const std::string& error) {
  // Clear webstore_data_fetcher_ so that WebContentsDestroyed will no longer
  // call Release in case the WebContents is destroyed before this object.
  scoped_ptr<WebstoreDataFetcher> webstore_data_fetcher(
      webstore_data_fetcher_.Pass());
  if (!callback_.is_null())
    callback_.Run(error.empty(), error);

  Release();  // Matches the AddRef in BeginInstall.
}

void
WebstoreStandaloneInstaller::CreateInstallUI() {
  std::string error;
  localized_extension_for_display_ =
      ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
          manifest_.get(),
          Extension::REQUIRE_KEY | Extension::FROM_WEBSTORE,
          id_,
          localized_name_,
          localized_description_,
          &error);
  if (!localized_extension_for_display_) {
    CompleteInstall(kInvalidManifestError);
    return;
  }

  install_ui_.reset(new ExtensionInstallPrompt(GetWebContents()));
  install_ui_->ConfirmStandaloneInstall(this,
                                        localized_extension_for_display_,
                                        &icon_,
                                        *install_prompt_);
}

}  // namespace extensions
