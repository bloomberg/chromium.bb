// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/auto_login_prompter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/auto_login_parser/auto_login_parser.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::BrowserContext;
using content::WebContents;

AutoLoginPrompter::AutoLoginPrompter(WebContents* web_contents,
                                     const Params& params,
                                     const GURL& url)
    : WebContentsObserver(web_contents),
      params_(params),
      url_(url),
      infobar_shown_(false) {
  if (!web_contents->IsLoading()) {
    // If the WebContents isn't loading a page, the load notification will never
    // be triggered.  Try adding the InfoBar now.
    AddInfoBarToWebContents();
  }
}

AutoLoginPrompter::~AutoLoginPrompter() {
}

// static
void AutoLoginPrompter::ShowInfoBarIfPossible(net::URLRequest* request,
                                              int child_id,
                                              int route_id) {
  // See if the response contains the X-Auto-Login header.  If so, this was
  // a request for a login page, and the server is allowing the browser to
  // suggest auto-login, if available.
  Params params;
  // Currently we only accept GAIA credentials in Chrome.
  if (!auto_login_parser::ParserHeaderInResponse(
          request, auto_login_parser::ONLY_GOOGLE_COM, &params.header))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowInfoBarUIThread,
                 params, request->url(), child_id, route_id));
}


// static
void AutoLoginPrompter::ShowInfoBarUIThread(Params params,
                                            const GURL& url,
                                            int child_id,
                                            int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

  BrowserContext* context = web_contents->GetBrowserContext();
  if (context->IsOffTheRecord())
    return;

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile->GetPrefs()->GetBoolean(prefs::kAutologinEnabled))
    return;

  // Make sure that |account|, if specified, matches the logged in user.
  // However, |account| is usually empty.
  if (!params.username.empty() && !params.header.account.empty() &&
      params.username != params.header.account)
    return;
  // We can't add the infobar just yet, since we need to wait for the tab to
  // finish loading.  If we don't, the info bar appears and then disappears
  // immediately.  Create an AutoLoginPrompter instance to listen for the
  // relevant notifications; it will delete itself.
  new AutoLoginPrompter(web_contents, params, url);
}

void AutoLoginPrompter::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  AddInfoBarToWebContents();
  delete this;
}

void AutoLoginPrompter::WebContentsDestroyed() {
  // The WebContents was destroyed before the navigation completed.
  delete this;
}

void AutoLoginPrompter::AddInfoBarToWebContents() {
  if (!infobar_shown_)
    infobar_shown_ = AutoLoginInfoBarDelegate::Create(web_contents(), params_);
}
