// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/registration_screen.h"

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

namespace {
// TODO(nkostylev): Create host page in resources.
const char kRegistrationHostPageUrl[] = "about:";
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, public:
RegistrationScreen::RegistrationScreen(WizardScreenDelegate* delegate)
    : ViewScreen<RegistrationView>(delegate) {
  if (!host_page_url_.get())
    set_registration_host_page_url(GURL(kRegistrationHostPageUrl));
}

// static
void RegistrationScreen::set_registration_host_page_url(const GURL& url) {
  host_page_url_.reset(new GURL(url));
}

// static
scoped_ptr<GURL> RegistrationScreen::host_page_url_;

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, ViewScreen implementation:
void RegistrationScreen::CreateView() {
  ViewScreen<RegistrationView>::CreateView();
  view()->SetWebPageDelegate(this);
}

void RegistrationScreen::Refresh() {
  StartTimeoutTimer();
  GURL url(*host_page_url_);
  Profile* profile = ProfileManager::GetDefaultProfile();
  view()->InitDOM(profile,
                  SiteInstance::CreateSiteInstanceForURL(profile, url));
  view()->SetTabContentsDelegate(this);
  view()->LoadURL(url);
}

RegistrationView* RegistrationScreen::AllocateView() {
  return new RegistrationView();
}

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, WebPageDelegate implementation:
void RegistrationScreen::OnPageLoaded() {
  StopTimeoutTimer();
  // Enable input methods (e.g. Chinese, Japanese) so that users could input
  // their first and last names.
  if (g_browser_process) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    input_method::EnableInputMethods(
        locale, input_method::kKeyboardLayoutsOnly, "");
    // TODO(yusukes,suzhe): Change the 2nd argument to kAllInputMethods when
    // crosbug.com/2670 is fixed.
  }
  view()->ShowPageContent();
}

void RegistrationScreen::OnPageLoadFailed(const std::string& url) {
  CloseScreen(ScreenObserver::CONNECTION_FAILED);
}

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, private:
void RegistrationScreen::CloseScreen(ScreenObserver::ExitCodes code) {
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
