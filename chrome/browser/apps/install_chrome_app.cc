// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/install_chrome_app.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "extensions/common/extension.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

// The URL to the webstore page for a specific app. "_asi=1" instructs webstore
// to immediately try to install the app if the referrer is the sign in page.
// This is actually the short form of the URL which just redirects to the full
// URL. Since "_asi=1" only works on the full url, we need to resolve it first
// before navigating the user to it.
const char kWebstoreUrlFormat[] =
    "https://chrome.google.com/webstore/detail/%s?_asi=1";

// The URL for the sign in page, set as the referrer to webstore.
const char kAccountsUrl[] = "https://accounts.google.com/ServiceLogin";

// Returns the webstore URL for an app.
GURL GetAppInstallUrl(const std::string& app_id) {
  return GURL(base::StringPrintf(kWebstoreUrlFormat, app_id.c_str()));
}

void NavigateToUrlWithAccountsReferrer(const GURL& url) {
  Browser* browser =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE)->get(0);
  if (!browser)
    return;

  chrome::NavigateParams params(
      browser, url, content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.referrer = content::Referrer();
  params.referrer.url = GURL(kAccountsUrl);
  chrome::Navigate(&params);
}

class AppURLFetcher : net::URLFetcherDelegate {
 public:
  explicit AppURLFetcher(const std::string& app_id);

  // net::URLFetcherDelegate OVERRIDES:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  virtual ~AppURLFetcher();

  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AppURLFetcher);
};

AppURLFetcher::AppURLFetcher(const std::string& app_id) {
  url_fetcher_.reset(net::URLFetcher::Create(
      GetAppInstallUrl(app_id), net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(g_browser_process->system_request_context());
  url_fetcher_->SetStopOnRedirect(true);
  url_fetcher_->Start();
}

AppURLFetcher::~AppURLFetcher() {
}

void AppURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source->GetResponseCode() == 301) {
    // Moved permanently.
    NavigateToUrlWithAccountsReferrer(source->GetURL());
  }

  delete this;
}

}  // namespace

namespace install_chrome_app {

void InstallChromeApp(const std::string& app_id) {
  if (!extensions::Extension::IdIsValid(app_id))
    return;

  new AppURLFetcher(app_id);
}

}  // namespace install_chrome_app
