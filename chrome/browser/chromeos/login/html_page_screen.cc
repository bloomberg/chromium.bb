// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/html_page_screen.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "views/events/event.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

static const char kHTMLPageDoneUrl[] = "about:blank";

///////////////////////////////////////////////////////////////////////////////
// HTMLPageDomView
TabContents* HTMLPageDomView::CreateTabContents(Profile* profile,
                                                SiteInstance* instance) {
  return new WizardWebPageViewTabContents(profile,
                                          instance,
                                          page_delegate_);
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageView
HTMLPageView::HTMLPageView()
    : dom_view_(new HTMLPageDomView()) {
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, public:
HTMLPageScreen::HTMLPageScreen(WizardScreenDelegate* delegate,
                               const std::string& url)
    : ViewScreen<HTMLPageView>(delegate), url_(url) {
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, ViewScreen implementation:
void HTMLPageScreen::CreateView() {
  ViewScreen<HTMLPageView>::CreateView();
  view()->SetWebPageDelegate(this);
}

void HTMLPageScreen::Refresh() {
  VLOG(1) << "HTMLPageScreen::Refresh(): " << url_;
  StartTimeoutTimer();
  GURL url(url_);
  Profile* profile = ProfileManager::GetDefaultProfile();
  view()->InitDOM(profile,
                  SiteInstance::CreateSiteInstanceForURL(profile, url));
  view()->SetTabContentsDelegate(this);
  view()->LoadURL(url);
}

HTMLPageView* HTMLPageScreen::AllocateView() {
  return new HTMLPageView();
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, TabContentsDelegate implementation:
void HTMLPageScreen::LoadingStateChanged(TabContents* source) {
  std::string url = source->GetURL().spec();
  if (url == kHTMLPageDoneUrl) {
    source->Stop();
    // TODO(dpolukhin): use special code for this case but now
    // ACCOUNT_CREATE_BACK works as we would like, i.e. get to login page.
    VLOG(1) << "HTMLPageScreen::LoadingStateChanged: " << url;
    CloseScreen(ScreenObserver::ACCOUNT_CREATE_BACK);
  }
}

void HTMLPageScreen::NavigationStateChanged(const TabContents* source,
                                            unsigned changed_flags) {
}

void HTMLPageScreen::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  views::Widget* widget = view()->GetWidget();
  if (widget && event.os_event && !event.skip_in_browser) {
    views::KeyEvent views_event(reinterpret_cast<GdkEvent*>(event.os_event));
    static_cast<views::WidgetGtk*>(widget)->HandleKeyboardEvent(views_event);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, WebPageDelegate implementation:
void HTMLPageScreen::OnPageLoaded() {
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

void HTMLPageScreen::OnPageLoadFailed(const std::string& url) {
  VLOG(1) << "HTMLPageScreen::OnPageLoadFailed: " << url;
  CloseScreen(ScreenObserver::CONNECTION_FAILED);
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, WebPageScreen implementation:
void HTMLPageScreen::OnNetworkTimeout() {
  VLOG(1) << "HTMLPageScreen::OnNetworkTimeout";
  // Just show what we have now. We shouldn't exit from the screen on timeout.
  OnPageLoaded();
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, private:
void HTMLPageScreen::CloseScreen(ScreenObserver::ExitCodes code) {
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
