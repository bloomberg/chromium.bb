// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_screen.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/login/account_creation_view.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

namespace {

const char kCreateAccountPageUrl[] =
    "https://www.google.com/accounts/NewAccount?service=mail&hl=en";

const char kCreateAccountDoneUrl[] =
    "http://mail.google.com/mail/help/intro.html";

const char kCreateAccountBackUrl[] =
    "about:blank";

const char kCreateAccountCSS[] =
    "body > table, div.body > h3, div.body > table, a, "
    "#cookieWarning1, #cookieWarning2 {\n"
    " display: none !important;\n"
    "}\n"
    "tbody tr:nth-child(7), tbody tr:nth-child(8), tbody tr:nth-child(9),"
    "tbody tr:nth-child(13), tbody tr:nth-child(16), tbody tr:nth-child(17),"
    "tbody tr:nth-child(18) {\n"
    " display: none !important;\n"
    "}\n"
    "body {\n"
    " padding: 0;\n"
    "}\n";

const char kCreateAccountJS[] =
  "try {\n"
  " var smhck = document.getElementById('smhck');\n"
  " smhck.checked = false;\n"
  " smhck.value = 0;\n"
  " var tables = document.getElementsByTagName('table');\n"
  " for (var i = 0; i < tables.length; i++) {\n"
  "   if (tables[i].bgColor == '#cbdced') tables[i].cellPadding = 0;\n"
  " }\n"
  " var submitbtn = document.getElementById('submitbutton');\n"
  " submitbtn.value = 'Create Account';\n"
  " submitbtn.parentNode.parentNode.firstElementChild.innerHTML ="
  "   \"<input type='button' style='width:8em' value='<< Back'"
  "      onclick='window.location=\\\"about:blank\\\";'/>\";\n"
  "} catch(err) {\n"
  "}\n";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, public:
AccountScreen::AccountScreen(WizardScreenDelegate* delegate)
    : ViewScreen<AccountCreationView>(delegate) {
  if (!new_account_page_url_.get())
    new_account_page_url_.reset(new GURL(kCreateAccountPageUrl));
}

AccountScreen::~AccountScreen() {
}

// static
void AccountScreen::set_new_account_page_url(const GURL& url) {
  new_account_page_url_.reset(new GURL(url));
}

// static
scoped_ptr<GURL> AccountScreen::new_account_page_url_;
// static
bool AccountScreen::check_for_https_ = true;

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, ViewScreen implementation:
void AccountScreen::CreateView() {
  ViewScreen<AccountCreationView>::CreateView();
  view()->SetAccountCreationViewDelegate(this);
}

void AccountScreen::Refresh() {
  GURL url(*new_account_page_url_);
  Profile* profile = ProfileManager::GetDefaultProfile();
  view()->InitDOM(profile,
                  SiteInstance::CreateSiteInstanceForURL(profile, url));
  view()->SetTabContentsDelegate(this);
  view()->LoadURL(url);
}

AccountCreationView* AccountScreen::AllocateView() {
  return new AccountCreationView();
}

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, TabContentsDelegate implementation:
void AccountScreen::LoadingStateChanged(TabContents* source) {
  std::string url = source->GetURL().spec();
  if (url == kCreateAccountDoneUrl) {
    source->Stop();
    delegate()->GetObserver(this)->OnExit(ScreenObserver::ACCOUNT_CREATED);
  } else if (url == kCreateAccountBackUrl) {
    delegate()->GetObserver(this)->OnExit(ScreenObserver::ACCOUNT_CREATE_BACK);
  } else if (check_for_https_ && !source->GetURL().SchemeIsSecure()) {
    delegate()->GetObserver(this)->OnExit(ScreenObserver::CONNECTION_FAILED);
  }
}

void AccountScreen::NavigationStateChanged(const TabContents* source,
                                           unsigned changed_flags) {
  if (source->render_view_host()) {
    source->render_view_host()->InsertCSSInWebFrame(
        L"", kCreateAccountCSS, "");
    source->render_view_host()->ExecuteJavascriptInWebFrame(
        L"", ASCIIToWide(kCreateAccountJS));
  }
}

bool AccountScreen::HandleContextMenu(const ContextMenuParams& params) {
  // Just return true because we don't want to show context menue.
  return true;
}

void AccountScreen::OnUserCreated(const std::string& username,
                                  const std::string& password) {
  delegate()->GetObserver(this)->OnSetUserNamePassword(username, password);
}

void AccountScreen::OnPageLoadFailed(const std::string& url) {
  delegate()->GetObserver(this)->OnExit(ScreenObserver::CONNECTION_FAILED);
}

}  // namespace chromeos
