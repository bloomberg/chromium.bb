// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_inline_installer.h"

#include <vector>

#include "base/bind.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/utility_process_host.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

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

class SafeWebstoreResponseParser : public UtilityProcessHost::Client {
 public:
  SafeWebstoreResponseParser(WebstoreInlineInstaller *client,
                             const std::string& webstore_data)
      : client_(client),
        webstore_data_(webstore_data),
        utility_host_(NULL) {}

  void Start() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeWebstoreResponseParser::StartWorkOnIOThread, this));
  }

  void StartWorkOnIOThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    utility_host_ = new UtilityProcessHost(this, BrowserThread::IO);
    utility_host_->Send(new ChromeUtilityMsg_ParseJSON(webstore_data_));
  }

  // Implementing pieces of the UtilityProcessHost::Client interface.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SafeWebstoreResponseParser, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                          OnJSONParseSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                          OnJSONParseFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnJSONParseSucceeded(const ListValue& wrapper) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    if (value->IsType(Value::TYPE_DICTIONARY)) {
      parsed_webstore_data_.reset(
          static_cast<DictionaryValue*>(value)->DeepCopy());
    } else {
      error_ = kInvalidWebstoreResponseError;
    }

    ReportResults();
  }

  virtual void OnJSONParseFailed(const std::string& error_message) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    error_ = error_message;
    ReportResults();
  }

  void ReportResults() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // The utility_host_ will take care of deleting itself after this call.
    utility_host_ = NULL;

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SafeWebstoreResponseParser::ReportResultOnUIThread, this));
  }

  void ReportResultOnUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (error_.empty() && parsed_webstore_data_.get()) {
      client_->OnWebstoreResponseParseSuccess(parsed_webstore_data_.release());
    } else {
      client_->OnWebstoreResponseParseFailure(error_);
    }
  }

 private:
  virtual ~SafeWebstoreResponseParser() {}

  WebstoreInlineInstaller* client_;

  std::string webstore_data_;

  UtilityProcessHost* utility_host_;

  std::string error_;
  scoped_ptr<DictionaryValue> parsed_webstore_data_;
};

WebstoreInlineInstaller::WebstoreInlineInstaller(TabContents* tab_contents,
                                                 int install_id,
                                                 std::string webstore_item_id,
                                                 GURL requestor_url,
                                                 Delegate* delegate)
    : TabContentsObserver(tab_contents),
      install_id_(install_id),
      id_(webstore_item_id),
      requestor_url_(requestor_url),
      delegate_(delegate),
      average_rating_(0.0),
      rating_count_(0) {}

WebstoreInlineInstaller::~WebstoreInlineInstaller() {
}

void WebstoreInlineInstaller::BeginInstall() {
  AddRef(); // Balanced in CompleteInstall or TabContentsDestroyed.

  if (!Extension::IdIsValid(id_)) {
    CompleteInstall(kInvalidWebstoreItemId);
    return;
  }

  GURL webstore_data_url(extension_urls::GetWebstoreItemJsonDataURL(id_));

  webstore_data_url_fetcher_.reset(content::URLFetcher::Create(
      webstore_data_url, content::URLFetcher::GET, this));
  Profile* profile = Profile::FromBrowserContext(
      tab_contents()->browser_context());
  webstore_data_url_fetcher_->SetRequestContext(
      profile->GetRequestContext());
  // Use the requesting page as the referrer both since that is more correct
  // (it is the page that caused this request to happen) and so that we can
  // track top sites that trigger inline install requests.
  webstore_data_url_fetcher_->SetReferrer(requestor_url_.spec());
  webstore_data_url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                           net::LOAD_DO_NOT_SAVE_COOKIES |
                                           net::LOAD_DISABLE_CACHE);
  webstore_data_url_fetcher_->Start();
}

void WebstoreInlineInstaller::OnURLFetchComplete(
    const content::URLFetcher* source) {
  CHECK_EQ(webstore_data_url_fetcher_.get(), source);
  // We shouldn't be getting UrlFetcher callbacks if the TabContents has gone
  // away; we stop any in in-progress fetches in TabContentsDestroyed.
  CHECK(tab_contents());

  if (!webstore_data_url_fetcher_->GetStatus().is_success() ||
      webstore_data_url_fetcher_->GetResponseCode() != 200) {
    CompleteInstall(kWebstoreRequestError);
    return;
  }

  std::string webstore_json_data;
  webstore_data_url_fetcher_->GetResponseAsString(&webstore_json_data);
  webstore_data_url_fetcher_.reset();

  scoped_refptr<SafeWebstoreResponseParser> parser =
      new SafeWebstoreResponseParser(this, webstore_json_data);
  // The parser will call us back via OnWebstoreResponseParseSucces or
  // OnWebstoreResponseParseFailure.
  parser->Start();
}

void WebstoreInlineInstaller::OnWebstoreResponseParseSuccess(
    DictionaryValue* webstore_data) {
  // Check if the tab has gone away in the meantime.
  if (!tab_contents()) {
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

    tab_contents()->OpenURL(OpenURLParams(
        GURL(redirect_url),
        tab_contents()->GetURL(),
        NEW_FOREGROUND_TAB,
        content::PAGE_TRANSITION_AUTO_BOOKMARK,
        false));
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

  if (average_rating_ < ExtensionInstallUI::kMinExtensionRating ||
      average_rating_ >ExtensionInstallUI::kMaxExtensionRating) {
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

  // Verified site is required
  if (webstore_data->HasKey(kVerifiedSiteKey)) {
    std::string verified_site_domain;
    if (!webstore_data->GetString(kVerifiedSiteKey, &verified_site_domain)) {
      CompleteInstall(kInvalidWebstoreResponseError);
      return;
    }

    URLPattern verified_site_pattern(URLPattern::SCHEME_ALL);
    verified_site_pattern.SetScheme("*");
    verified_site_pattern.SetHost(verified_site_domain);
    verified_site_pattern.SetMatchSubdomains(true);
    verified_site_pattern.SetPath("/*");

    if (!verified_site_pattern.MatchesURL(requestor_url_)) {
      CompleteInstall(kNotFromVerifiedSiteError);
      return;
    }
  } else {
    CompleteInstall(kNoVerifiedSiteError);
    return;
  }

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this,
      id_,
      manifest,
      "", // We don't have any icon data.
      icon_url,
      Profile::FromBrowserContext(tab_contents()->browser_context())->
          GetRequestContext());
  // The helper will call us back via OnWebstoreParseSucces or
  // OnWebstoreParseFailure.
  helper->Start();
}

void WebstoreInlineInstaller::OnWebstoreResponseParseFailure(
    const std::string& error) {
  CompleteInstall(error);
}

void WebstoreInlineInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* manifest) {
  // Check if the tab has gone away in the meantime.
  if (!tab_contents()) {
    CompleteInstall("");
    return;
  }

  CHECK_EQ(id_, id);
  manifest_.reset(manifest);
  icon_ = icon;

  Profile* profile = Profile::FromBrowserContext(
      tab_contents()->browser_context());

  ExtensionInstallUI::Prompt prompt(ExtensionInstallUI::INLINE_INSTALL_PROMPT);
  prompt.SetInlineInstallWebstoreData(localized_user_count_,
                                      average_rating_,
                                      rating_count_);

  if (!ShowExtensionInstallDialogForManifest(profile,
                                             this,
                                             manifest,
                                             id_,
                                             localized_name_,
                                             localized_description_,
                                             &icon_,
                                             prompt,
                                             &dummy_extension_)) {
    CompleteInstall(kInvalidManifestError);
    return;
  }

  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void WebstoreInlineInstaller::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  CompleteInstall(error_message);
}

void WebstoreInlineInstaller::InstallUIProceed() {
  // Check if the tab has gone away in the meantime.
  if (!tab_contents()) {
    CompleteInstall("");
    return;
  }

  CrxInstaller::WhitelistEntry* entry = new CrxInstaller::WhitelistEntry;

  entry->parsed_manifest.reset(manifest_.get()->DeepCopy());
  entry->localized_name = localized_name_;
  entry->use_app_installed_bubble = true;
  CrxInstaller::SetWhitelistEntry(id_, entry);

  Profile* profile = Profile::FromBrowserContext(
      tab_contents()->browser_context());

  scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
      profile, this, &(tab_contents()->controller()), id_,
      WebstoreInstaller::FLAG_INLINE_INSTALL);
  installer->Start();
}

void WebstoreInlineInstaller::InstallUIAbort(bool user_initiated) {
  CompleteInstall(kUserCancelledError);
}

void WebstoreInlineInstaller::TabContentsDestroyed(TabContents* tab_contents) {
  // Abort any in-progress fetches.
  if (webstore_data_url_fetcher_.get()) {
    webstore_data_url_fetcher_.reset();
    Release(); // Matches the AddRef in BeginInstall.
  }
}

void WebstoreInlineInstaller::OnExtensionInstallSuccess(const std::string& id) {
  CHECK_EQ(id_, id);
  CompleteInstall("");
}

void WebstoreInlineInstaller::OnExtensionInstallFailure(
    const std::string& id, const std::string& error) {
  CHECK_EQ(id_, id);
  CompleteInstall(error);
}

void WebstoreInlineInstaller::CompleteInstall(const std::string& error) {
  // Only bother responding if there's still a tab contents to send back the
  // response to.
  if (tab_contents()) {
    if (error.empty()) {
      delegate_->OnInlineInstallSuccess(install_id_);
    } else {
      delegate_->OnInlineInstallFailure(install_id_, error);
    }
  }

  Release(); // Matches the AddRef in BeginInstall.
}
