// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_inline_installer.h"

#include <vector>

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/utility_process_host.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"

const char kManifestKey[] = "manifest";
const char kIconUrlKey[] = "icon_url";
const char kLocalizedNameKey[] = "localized_name";
const char kLocalizedDescriptionKey[] = "localized_description";
const char kUsersKey[] = "users";
const char kAverageRatingKey[] = "average_rating";
const char kRatingCountKey[] = "rating_count";
const char kVerifiedSiteKey[] = "verified_site";

const char kInvalidWebstoreItemId[] = "Invalid Chrome Web Store item ID";
const char kWebstoreRequestError[] =
    "Could not fetch data from the Chrome Web Store";
const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";
const char kNotFromVerifiedSite[] =
    "Installs can only be initiated by the Chrome Web Store item's verified "
    "site";

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
        NewRunnableMethod(this,
                          &SafeWebstoreResponseParser::StartWorkOnIOThread));
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
        NewRunnableMethod(this,
                          &SafeWebstoreResponseParser::ReportResultOnUIThread));
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
      delegate_(delegate) {}

WebstoreInlineInstaller::~WebstoreInlineInstaller() {
}

void WebstoreInlineInstaller::BeginInstall() {
  AddRef(); // Balanced in CompleteInstall or TabContentsDestroyed.

  if (!Extension::IdIsValid(id_)) {
    CompleteInstall(kInvalidWebstoreItemId);
    return;
  }

  GURL webstore_data_url(extension_urls::GetWebstoreItemJsonDataURL(id_));

  webstore_data_url_fetcher_.reset(
      new URLFetcher(webstore_data_url, URLFetcher::GET, this));
  Profile* profile = Profile::FromBrowserContext(
      tab_contents()->browser_context());
  webstore_data_url_fetcher_->set_request_context(
      profile->GetRequestContext());
  webstore_data_url_fetcher_->Start();
}

void WebstoreInlineInstaller::OnURLFetchComplete(const URLFetcher* source) {
  CHECK_EQ(webstore_data_url_fetcher_.get(), source);
  // We shouldn't be getting UrlFetcher callbacks if the TabContents has gone
  // away; we stop any in in-progress fetches in TabContentsDestroyed.
  CHECK(tab_contents());

  if (!webstore_data_url_fetcher_->status().is_success() ||
      webstore_data_url_fetcher_->response_code() != 200) {
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
  webstore_data_.reset(webstore_data);

  std::string manifest;
  if (!webstore_data->GetString(kManifestKey, &manifest)) {
    CompleteInstall(kInvalidWebstoreResponseError);
    return;
  }

  // Number of users, average rating and rating count are required.
  if (!webstore_data->GetString(kUsersKey, &localized_user_count_) ||
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

  // Verified site is optional
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
      CompleteInstall(kNotFromVerifiedSite);
      return;
    }
  }

  scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
      this,
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
    const SkBitmap& icon,
    base::DictionaryValue* manifest) {
  // Check if the tab has gone away in the meantime.
  if (!tab_contents()) {
    CompleteInstall("");
    return;
  }

  manifest_.reset(manifest);
  icon_ = icon;

  Profile* profile = Profile::FromBrowserContext(
      tab_contents()->browser_context());

  ExtensionInstallUI::Prompt prompt(ExtensionInstallUI::INLINE_INSTALL_PROMPT);
  prompt.SetInlineInstallWebstoreData(localized_user_count_,
                                      average_rating_,
                                      rating_count_);

  ShowExtensionInstallDialogForManifest(profile,
                                        this,
                                        manifest,
                                        id_,
                                        localized_name_,
                                        localized_description_,
                                        &icon_,
                                        prompt,
                                        &dummy_extension_);

  if (!dummy_extension_.get()) {
    CompleteInstall(kInvalidManifestError);
    return;
  }

  // Control flow finishes up in InstallUIProceed or InstallUIAbort.
}

void WebstoreInlineInstaller::OnWebstoreParseFailure(
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

  GURL install_url(extension_urls::GetWebstoreInstallUrl(
      id_, g_browser_process->GetApplicationLocale()));

  NavigationController& controller = tab_contents()->controller();
  // TODO(mihaip): we pretend like the referrer is the gallery in order to pass
  // the checks in ExtensionService::IsDownloadFromGallery. We should instead
  // pass the real referrer, track that this is an inline install in the
  // whitelist entry and look that up when checking that this is a valid
  // download.
  GURL referrer(extension_urls::GetWebstoreItemDetailURLPrefix() + id_);
  controller.LoadURL(install_url, referrer, PageTransition::LINK);

  // TODO(mihaip): the success message should happen later, when the extension
  // is actually downloaded and installed (when NOTIFICATION_EXTENSION_INSTALLED
  // or NOTIFICATION_EXTENSION_INSTALL_ERROR fire).
  CompleteInstall("");
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
