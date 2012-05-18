// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/html_page_screen.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "googleurl/src/gurl.h"
#include "ui/views/events/event.h"

using content::NativeWebKeyboardEvent;
using content::SiteInstance;
using content::WebContents;

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// HTMLPageView
HTMLPageView::HTMLPageView(content::BrowserContext* browser_context)
    : dom_view_(new WebPageDomView(browser_context)) {
}

WebPageDomView* HTMLPageView::dom_view() {
  return dom_view_;
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, public:
HTMLPageScreen::HTMLPageScreen(ViewScreenDelegate* delegate,
                               const std::string& url)
    : ViewScreen<HTMLPageView>(delegate), url_(url) {
}

HTMLPageScreen::~HTMLPageScreen() {}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, ViewScreen implementation:
void HTMLPageScreen::CreateView() {
  ViewScreen<HTMLPageView>::CreateView();
}

void HTMLPageScreen::Refresh() {
  VLOG(1) << "HTMLPageScreen::Refresh(): " << url_;
  StartTimeoutTimer();
  GURL url(url_);
  Profile* profile = ProfileManager::GetDefaultProfile();
  view()->InitWebView(SiteInstance::CreateForURL(profile, url));
  view()->SetWebContentsDelegate(this);
  view()->LoadURL(url);
}

HTMLPageView* HTMLPageScreen::AllocateView() {
  return new HTMLPageView(ProfileManager::GetDefaultProfile());
}

void HTMLPageScreen::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  unhandled_keyboard_handler_.HandleKeyboardEvent(event,
                                                  view()->GetFocusManager());
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, WebPageScreen implementation:
void HTMLPageScreen::OnNetworkTimeout() {
  VLOG(1) << "HTMLPageScreen::OnNetworkTimeout";
  // Just show what we have now. We shouldn't exit from the screen on timeout.
  StopTimeoutTimer();
  view()->ShowPageContent();
}

///////////////////////////////////////////////////////////////////////////////
// HTMLPageScreen, private:
void HTMLPageScreen::CloseScreen(ScreenObserver::ExitCodes code) {
  StopTimeoutTimer();
  delegate()->GetObserver()->OnExit(code);
}

}  // namespace chromeos
