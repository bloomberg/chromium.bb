// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include "base/location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"

using content::LoadNotificationDetails;
using content::NavigationController;
using content::RenderViewHost;
using content::ResourceRedirectDetails;
using content::WebContents;
using content::WebContentsObserver;

namespace extensions {

WebAuthFlow::WebAuthFlow(
    Delegate* delegate,
    Profile* profile,
    const GURL& provider_url,
    Mode mode,
    const gfx::Rect& initial_bounds,
    chrome::HostDesktopType host_desktop_type)
    : delegate_(delegate),
      profile_(profile),
      provider_url_(provider_url),
      mode_(mode),
      initial_bounds_(initial_bounds),
      host_desktop_type_(host_desktop_type),
      popup_shown_(false),
      contents_(NULL) {
}

WebAuthFlow::~WebAuthFlow() {
  // Stop listening to notifications first since some of the code
  // below may generate notifications.
  registrar_.RemoveAll();
  WebContentsObserver::Observe(NULL);

  if (contents_) {
    // The popup owns the contents if it was displayed.
    if (popup_shown_)
      contents_->Close();
    else
      delete contents_;
  }
}

void WebAuthFlow::Start() {
  contents_ = CreateWebContents();
  WebContentsObserver::Observe(contents_);

  NavigationController* controller = &(contents_->GetController());

  // Register for appropriate notifications to intercept navigation to the
  // redirect URLs.
  registrar_.Add(
      this,
      content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<WebContents>(contents_));

  controller->LoadURL(
      provider_url_,
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

WebContents* WebAuthFlow::CreateWebContents() {
  return WebContents::Create(WebContents::CreateParams(profile_));
}

void WebAuthFlow::ShowAuthFlowPopup() {
  Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile_,
                                       host_desktop_type_);
  browser_params.initial_bounds = initial_bounds_;
  Browser* browser = new Browser(browser_params);
  chrome::NavigateParams params(browser, contents_);
  params.disposition = CURRENT_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
  // Observe method and WebContentsObserver::* methods will be called
  // for varous navigation events. That is where we check for redirect
  // to the right URL.
  popup_shown_ = true;
}

void WebAuthFlow::BeforeUrlLoaded(const GURL& url) {
  delegate_->OnAuthFlowURLChange(url);
}

void WebAuthFlow::AfterUrlLoaded() {
  // Do nothing if a popup is already created.
  if (popup_shown_)
    return;

  // Report results directly if not in interactive mode.
  if (mode_ != WebAuthFlow::INTERACTIVE) {
    delegate_->OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED);
    return;
  }

  // We are in interactive mode and window is not shown yet; show the window.
  ShowAuthFlowPopup();
}

void WebAuthFlow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      ResourceRedirectDetails* redirect_details =
          content::Details<ResourceRedirectDetails>(details).ptr();
      if (redirect_details != NULL)
        BeforeUrlLoaded(redirect_details->new_url);
    }
    break;
    default:
      NOTREACHED() << "Got a notification that we did not register for: "
                   << type;
      break;
  }
}

void WebAuthFlow::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    RenderViewHost* render_view_host) {
  BeforeUrlLoaded(url);
}

void WebAuthFlow::DidStopLoading(RenderViewHost* render_view_host) {
  AfterUrlLoaded();
}

void WebAuthFlow::WebContentsDestroyed(WebContents* web_contents) {
  contents_ = NULL;
  delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
}

}  // namespace extensions
