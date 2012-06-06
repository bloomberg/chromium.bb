// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

using content::BrowserContext;
using content::WebContents;
using content::WebContentsDelegate;

namespace {

static const char kChromeExtensionSchemeUrlPattern[] =
    "chrome-extension://%s/";
static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

extensions::WebAuthFlow::InterceptorForTests* g_interceptor_for_tests = NULL;

}  // namespace

namespace extensions {

// static
void WebAuthFlow::SetInterceptorForTests(InterceptorForTests* interceptor) {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
  CHECK(NULL == g_interceptor_for_tests);  // Only one at a time.
  g_interceptor_for_tests = interceptor;
}

WebAuthFlow::WebAuthFlow(
    Delegate* delegate,
    BrowserContext* browser_context,
    const std::string& extension_id,
    const GURL& provider_url,
    Mode mode)
    : delegate_(delegate),
      browser_context_(browser_context),
      provider_url_(provider_url),
      mode_(mode),
      contents_(NULL),
      window_(NULL) {
  InitValidRedirectUrlPrefixes(extension_id);
}

WebAuthFlow::~WebAuthFlow() {
  if (window_)
    delete window_;
  if (contents_) {
    contents_->SetDelegate(NULL);
    contents_->Stop();
    // Tell message loop to delete contents_ instead of deleting it
    // directly since destructor can run in response to a callback from
    // contents_.
    MessageLoop::current()->DeleteSoon(FROM_HERE, contents_);
  }
}

void WebAuthFlow::Start() {
  if (g_interceptor_for_tests != NULL) {
    // We use PostTask, instead of calling the delegate directly, because the
    // message loop will run a few times before we notify the delegate in the
    // real implementation.
    GURL result = g_interceptor_for_tests->DoIntercept(provider_url_);
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&WebAuthFlow::ReportResult,
                   base::Unretained(this), result));
    return;
  }

  contents_ = CreateWebContents();
  contents_->SetDelegate(this);
  contents_->GetController().LoadURL(
      provider_url_,
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());
}

void WebAuthFlow::LoadingStateChanged(WebContents* source) {
  if (source->IsLoading())
    return;
  OnUrlLoaded();
}

void WebAuthFlow::NavigationStateChanged(
    const WebContents* source, unsigned changed_flags) {
  // If the URL has not changed, do not perform the check again (for
  // efficiency).
  if ((changed_flags & content::INVALIDATE_TYPE_URL) == 0)
    return;

  // If the URL is a valid extension redirect URL, capture the
  // results and end the flow.
  const GURL& url = source->GetURL();

  if (IsValidRedirectUrl(url)) {
    ReportResult(url);
  }
}

WebContents* WebAuthFlow::CreateWebContents() {
  return WebContents::Create(
      browser_context_, NULL, MSG_ROUTING_NONE, NULL, NULL);
}

WebAuthFlowWindow* WebAuthFlow::CreateAuthWindow() {
  return WebAuthFlowWindow::Create(this, browser_context_, contents_);
}

void WebAuthFlow::OnUrlLoaded() {
  // Do nothing if a window is already created.
  if (window_)
    return;

  // Report results directly if not in interactive mode.
  if (mode_ != WebAuthFlow::INTERACTIVE) {
    ReportResult(GURL());
    return;
  }

  // We are in interactive mode and window is not shown yet; create
  // and show the window.
  window_ = CreateAuthWindow();
  window_->Show();
}

void WebAuthFlow::OnClose() {
  ReportResult(GURL());
}

void WebAuthFlow::ReportResult(const GURL& result) {
  if (!delegate_)
    return;

  if (result.is_empty()) {
    delegate_->OnAuthFlowFailure();
  } else {
    // TODO(munjal): Consider adding code to parse out access token
    // from some common places (e.g. URL fragment) so the apps don't
    // have to do that work.
    delegate_->OnAuthFlowSuccess(result.spec());
  }

  // IMPORTANT: Do not access any members after calling the delegate
  // since the delegate can destroy |this| in the callback and hence
  // all data members are invalid after that.
}

void WebAuthFlow::InitValidRedirectUrlPrefixes(
    const std::string& extension_id) {
  valid_prefixes_.push_back(base::StringPrintf(
      kChromeExtensionSchemeUrlPattern, extension_id.c_str()));
  valid_prefixes_.push_back(base::StringPrintf(
      kChromiumDomainRedirectUrlPattern, extension_id.c_str()));
}

bool WebAuthFlow::IsValidRedirectUrl(const GURL& url) const {
  std::vector<std::string>::const_iterator iter;
  for (iter = valid_prefixes_.begin(); iter != valid_prefixes_.end(); ++iter) {
    if (StartsWithASCII(url.spec(), *iter, false))
      return true;
  }
  return false;
}

}  // namespace extensions
