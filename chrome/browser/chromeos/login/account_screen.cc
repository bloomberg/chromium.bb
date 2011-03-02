// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_screen.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/account_creation_view.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "views/widget/widget_gtk.h"

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
  view()->SetWebPageDelegate(this);
  view()->SetAccountCreationViewDelegate(this);
}

void AccountScreen::Refresh() {
  StartTimeoutTimer();
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
    CloseScreen(ScreenObserver::ACCOUNT_CREATED);
  } else if (url == kCreateAccountBackUrl) {
    CloseScreen(ScreenObserver::ACCOUNT_CREATE_BACK);
  } else if (check_for_https_ && !source->GetURL().SchemeIsSecure()) {
    CloseScreen(ScreenObserver::CONNECTION_FAILED);
  }
}

void AccountScreen::NavigationStateChanged(const TabContents* source,
                                           unsigned changed_flags) {
  if (source->render_view_host()) {
    source->render_view_host()->InsertCSSInWebFrame(
        L"", kCreateAccountCSS, "");
    source->render_view_host()->ExecuteJavascriptInWebFrame(
        string16(), ASCIIToUTF16(kCreateAccountJS));
  }
}

void AccountScreen::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  views::Widget* widget = view()->GetWidget();
  if (widget && event.os_event && !event.skip_in_browser)
    static_cast<views::WidgetGtk*>(widget)->HandleKeyboardEvent(event.os_event);
}

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, WebPageDelegate implementation:
void AccountScreen::OnPageLoaded() {
  StopTimeoutTimer();
  // Enable input methods (e.g. Chinese, Japanese) so that users could input
  // their first and last names.
  if (g_browser_process) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    input_method::EnableInputMethods(
        locale, input_method::kAllInputMethods, "");
  }
  view()->ShowPageContent();
}

void AccountScreen::OnPageLoadFailed(const std::string& url) {
  CloseScreen(ScreenObserver::CONNECTION_FAILED);
}

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, AccountCreationViewDelegate implementation:
void AccountScreen::OnUserCreated(const std::string& username,
                                  const std::string& password) {
  delegate()->GetObserver(this)->OnSetUserNamePassword(username, password);
}

///////////////////////////////////////////////////////////////////////////////
// AccountScreen, private:
void AccountScreen::CloseScreen(ScreenObserver::ExitCodes code) {
  StopTimeoutTimer();
  // Disable input methods since they are not necessary to input username and
  // password.
  if (g_browser_process) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    input_method::EnableInputMethods(
        locale, input_method::kKeyboardLayoutsOnly, "");
  }
  delegate()->GetObserver(this)->OnExit(code);
}

}  // namespace chromeos
