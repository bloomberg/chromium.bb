// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/registration_screen.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_about_job.h"
#include "net/url_request/url_request_filter.h"

using content::ChildProcessSecurityPolicy;
using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::SiteInstance;
using content::WebContents;

namespace chromeos {

namespace {

const char kRegistrationHostPageUrl[] = "chrome://register/";

// "Hostname" that is used for redirects from host registration page.
const char kRegistrationHostnameUrl[] = "register";

// Host page navigates to these URLs in case of success/skipped registration.
const char kRegistrationSuccessUrl[] = "cros://register/success";
const char kRegistrationSkippedUrl[] = "cros://register/skipped";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RegistrationView, protected:

RegistrationView::RegistrationView(content::BrowserContext* browser_context)
    : dom_view_(new WebPageDomView(browser_context)) {
}


WebPageDomView* RegistrationView::dom_view() {
  return dom_view_;
}

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, public:
RegistrationScreen::RegistrationScreen(ViewScreenDelegate* delegate)
    : ViewScreen<RegistrationView>(delegate) {
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kCrosScheme);
  net::URLRequestFilter::GetInstance()->AddHostnameHandler(
      chrome::kCrosScheme,
      kRegistrationHostnameUrl,
      &RegistrationScreen::Factory);
}

RegistrationScreen::~RegistrationScreen() {
}

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, ViewScreen implementation:
void RegistrationScreen::CreateView() {
  ViewScreen<RegistrationView>::CreateView();
}

void RegistrationScreen::Refresh() {
  StartTimeoutTimer();
  GURL url(kRegistrationHostPageUrl);
  Profile* profile = ProfileManager::GetDefaultProfile();
  view()->InitWebView(SiteInstance::CreateForURL(profile, url));
  view()->SetWebContentsDelegate(this);
  view()->LoadURL(url);
}

RegistrationView* RegistrationScreen::AllocateView() {
  return new RegistrationView(ProfileManager::GetDefaultProfile());
}

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, content::WebContentsDelegate implementation:

WebContents* RegistrationScreen::OpenURLFromTab(WebContents* source,
                                                const OpenURLParams& params) {
  if (params.url.spec() == kRegistrationSuccessUrl) {
    source->Stop();
    VLOG(1) << "Registration form completed.";
    CloseScreen(ScreenObserver::REGISTRATION_SUCCESS);
  } else if (params.url.spec() == kRegistrationSkippedUrl) {
    source->Stop();
    VLOG(1) << "Registration form skipped.";
    CloseScreen(ScreenObserver::REGISTRATION_SKIPPED);
  } else {
    source->Stop();
    // Host registration page and actual registration page hosted by
    // OEM partner doesn't contain links to external URLs.
    LOG(WARNING) << "Navigate to unsupported url: " << params.url.spec();
  }
  return NULL;
}

void RegistrationScreen::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  unhandled_keyboard_handler_.HandleKeyboardEvent(event,
                                                  view()->GetFocusManager());
}

///////////////////////////////////////////////////////////////////////////////
// RegistrationScreen, private:
void RegistrationScreen::CloseScreen(ScreenObserver::ExitCodes code) {
  StopTimeoutTimer();
  // Disable input methods since they are not necessary to input username and
  // password.
  if (g_browser_process) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::GetInstance();
    manager->EnableLayouts(locale, "");
  }
  delegate()->GetObserver()->OnExit(code);
}

// static
net::URLRequestJob* RegistrationScreen::Factory(net::URLRequest* request,
                                                const std::string& scheme) {
  VLOG(1) << "Handling url: " << request->url().spec().c_str();
  return new net::URLRequestAboutJob(request);
}

}  // namespace chromeos
