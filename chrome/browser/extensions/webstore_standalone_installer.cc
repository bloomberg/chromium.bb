// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_standalone_installer.h"

#include <vector>

#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::WebContents;

namespace extensions {

const char kManifestKey[] = "manifest";
const char kIconUrlKey[] = "icon_url";
const char kLocalizedNameKey[] = "localized_name";
const char kLocalizedDescriptionKey[] = "localized_description";
const char kUsersKey[] = "users";
const char kAverageRatingKey[] = "average_rating";
const char kRatingCountKey[] = "rating_count";
const char kVerifiedSiteKey[] = "verified_site";
const char kInlineInstallNotSupportedKey[] = "inline_install_not_supported";
const char kRedirectUrlKey[] = "redirect_url";

const char kInvalidWebstoreItemId[] = "Invalid Chrome Web Store item ID";
const char kWebstoreRequestError[] =
    "Could not fetch data from the Chrome Web Store";
const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";
const char kNoVerifiedSiteError[] =
    "Inline installs can only be initiated for Chrome Web Store items that "
    "have a verified site";
const char kNotFromVerifiedSiteError[] =
    "Installs can only be initiated by the Chrome Web Store item's verified "
    "site";
const char kInlineInstallSupportedError[] =
    "Inline installation is not supported for this item. The user will be "
    "redirected to the Chrome Web Store.";

WebstoreStandaloneInstaller::WebstoreStandaloneInstaller(
    std::string webstore_item_id,
    VerifiedSiteRequired require_verified_site,
    PromptType prompt_type,
    GURL requestor_url,
    Profile* profile,
    WebContents* web_contents,
    const Callback& callback)
    : content::WebContentsObserver(
          prompt_type == INLINE_PROMPT ?
              web_contents :
              content::WebContents::Create(
                  content::WebContents::CreateParams(profile))),
      id_(webstore_item_id),
      require_verified_site_(require_verified_site == REQUIRE_VERIFIED_SITE),
      prompt_type_(prompt_type),
      requestor_url_(requestor_url),
      profile_(profile),
      callback_(callback),
      average_rating_(0.0),
      rating_count_(0) {
  DCHECK((prompt_type == SKIP_PROMPT &&
          profile != NULL &&
          web_contents == NULL) ||
         (prompt_type == STANDARD_PROMPT &&
          profile != NULL &&
          web_contents == NULL) ||
         (prompt_type == INLINE_PROMPT &&
          profile != NULL &&
          web_contents != NULL &&
          profile == Profile::FromBrowserContext(
              web_contents->GetBrowserContext()))
  ) << " prompt_type=" << prompt_type
    << " profile=" << profile
    << " web_contents=" << web_contents;
  DCHECK(this->web_contents() != NULL);
  CHECK(!callback_.is_null());
}

WebstoreStandaloneInstaller::~WebstoreStandaloneInstaller() {
  if (prompt_type_ != INLINE_PROMPT) {
    // We create and own the web_contents in this case.
    delete web_contents();
  }
}

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
      requestor_url_,
      id_));
  webstore_data_fetcher_->Start();
}

void WebstoreStandaloneInstaller::OnWebstoreRequestFailure() {
  CompleteInstall(kWebstoreRequestError);
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseSuccess(
    DictionaryValue* webstore_data) {
  // Check if the tab has gone away in the meantime.
  if (prompt_type_ == INLINE_PROMPT && !web_contents()) {
    CompleteInstall("");
    return;
  }

  webstore_data_.reset(webstore_data);

  // The store may not support inline installs for this item, in which case
  // we open the store-provided redirect URL in a new tab and abort the
  // installation process.
  bool inline_install_not_supported = false;
  if (webstore_data->HasKey(kInlineInstallNotSupportedKey) &&
      !webstore_data->GetBoolean(
          kInlineInstallNotSupportedKey, &inline_install_not_supported)) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }
  if (inline_install_not_supported) {
    std::string redirect_url;
    if (!webstore_data->GetString(kRedirectUrlKey, &redirect_url)) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }

    if (prompt_type_ == INLINE_PROMPT) {
      web_contents()->OpenURL(OpenURLParams(
          GURL(redirect_url),
          content::Referrer(web_contents()->GetURL(),
                            WebKit::WebReferrerPolicyDefault),
          NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_AUTO_BOOKMARK,
          false));
    }
    CompleteInstall(kInlineInstallSupportedError);
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

  // Check for a verified site if required.
  if (require_verified_site_) {
    if (!webstore_data->HasKey(kVerifiedSiteKey)) {
      CompleteInstall(kNoVerifiedSiteError);
      return;
    }
    std::string verified_site;
    if (!webstore_data->GetString(kVerifiedSiteKey, &verified_site)) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }
    if (!IsRequestorURLInVerifiedSite(requestor_url_, verified_site)) {
      CompleteInstall(kNotFromVerifiedSiteError);
      return;
    }
  }

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this,
      id_,
      manifest,
      "",  // We don't have any icon data.
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
  // Check if the tab has gone away in the meantime.
  if (prompt_type_ == INLINE_PROMPT && !web_contents()) {
    CompleteInstall("");
    return;
  }

  CHECK_EQ(id_, id);
  manifest_.reset(manifest);
  icon_ = icon;

  if (prompt_type_ == SKIP_PROMPT) {
    InstallUIProceed();
    return;
  }

  ExtensionInstallPrompt::PromptType prompt_type =
      prompt_type_ == INLINE_PROMPT ?
          ExtensionInstallPrompt::INLINE_INSTALL_PROMPT :
          ExtensionInstallPrompt::INSTALL_PROMPT;
  ExtensionInstallPrompt::Prompt prompt(prompt_type);
  if (prompt_type_ == INLINE_PROMPT) {
    prompt.SetInlineInstallWebstoreData(localized_user_count_,
                                        average_rating_,
                                        rating_count_);
  }
  std::string error;
  dummy_extension_ = ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
      manifest,
      Extension::REQUIRE_KEY | Extension::FROM_WEBSTORE,
      id_,
      localized_name_,
      localized_description_,
      &error);
  if (!dummy_extension_) {
    OnWebstoreParseFailure(id_, WebstoreInstallHelper::Delegate::MANIFEST_ERROR,
                           kInvalidManifestError);
    return;
  }

  install_ui_.reset(new ExtensionInstallPrompt(web_contents()));
  install_ui_->ConfirmStandaloneInstall(this,
                                        dummy_extension_,
                                        &icon_,
                                        prompt);
  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void WebstoreStandaloneInstaller::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  CompleteInstall(error_message);
}

void WebstoreStandaloneInstaller::InstallUIProceed() {
  // Check if the tab has gone away in the meantime.
  if (prompt_type_ == INLINE_PROMPT && !web_contents()) {
    CompleteInstall("");
    return;
  }

  scoped_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          profile_,
          id_,
          scoped_ptr<base::DictionaryValue>(manifest_.get()->DeepCopy())));
  approval->skip_post_install_ui = (prompt_type_ != INLINE_PROMPT);
  approval->use_app_installed_bubble = (prompt_type_ == INLINE_PROMPT);

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile_, this, &(web_contents()->GetController()), id_, approval.Pass(),
      WebstoreInstaller::FLAG_INLINE_INSTALL);
  installer->Start();
}

void WebstoreStandaloneInstaller::InstallUIAbort(bool user_initiated) {
  CompleteInstall(kUserCancelledError);
}

void WebstoreStandaloneInstaller::WebContentsDestroyed(
    WebContents* web_contents) {
  callback_.Reset();
  // Abort any in-progress fetches.
  if (webstore_data_fetcher_.get()) {
    webstore_data_fetcher_.reset();
    Release();  // Matches the AddRef in BeginInstall.
  }
}

void WebstoreStandaloneInstaller::OnExtensionInstallSuccess(
    const std::string& id) {
  CHECK_EQ(id_, id);
  CompleteInstall("");
}

void WebstoreStandaloneInstaller::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason cancelled) {
  CHECK_EQ(id_, id);
  CompleteInstall(error);
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

// static
bool WebstoreStandaloneInstaller::IsRequestorURLInVerifiedSite(
    const GURL& requestor_url,
    const std::string& verified_site) {
  // Turn the verified site (which may be a bare domain, or have a port and/or a
  // path) into a URL that can be parsed by URLPattern.
  std::string verified_site_url =
      StringPrintf("http://*.%s%s",
          verified_site.c_str(),
          verified_site.find('/') == std::string::npos ? "/*" : "*");

  URLPattern verified_site_pattern(
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS);
  URLPattern::ParseResult parse_result =
      verified_site_pattern.Parse(verified_site_url);
  if (parse_result != URLPattern::PARSE_SUCCESS) {
    DLOG(WARNING) << "Could not parse " << verified_site_url <<
        " as URL pattern " << parse_result;
    return false;
  }
  verified_site_pattern.SetScheme("*");

  return verified_site_pattern.MatchesURL(requestor_url);
}

}  // namespace extensions
