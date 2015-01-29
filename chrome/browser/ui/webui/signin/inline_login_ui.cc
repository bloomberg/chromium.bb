// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "grit/browser_resources.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/ui/webui/chromeos/login/inline_login_handler_chromeos.h"
#else
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#endif

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIChromeSigninHost);
  source->OverrideContentSecurityPolicyFrameSrc("frame-src chrome-extension:;");
  source->OverrideContentSecurityPolicyObjectSrc("object-src *;");
  source->SetJsonPath("strings.js");

  bool is_webview_signin_enabled = switches::IsEnableWebviewBasedSignin();
  source->SetDefaultResource(is_webview_signin_enabled ?
      IDR_NEW_INLINE_LOGIN_HTML : IDR_INLINE_LOGIN_HTML);
  source->AddResourcePath("inline_login.css", IDR_INLINE_LOGIN_CSS);
  source->AddResourcePath("inline_login.js", IDR_INLINE_LOGIN_JS);
  source->AddResourcePath("gaia_auth_host.js", is_webview_signin_enabled ?
      IDR_GAIA_AUTH_AUTHENTICATOR_JS : IDR_GAIA_AUTH_HOST_JS);

  source->AddLocalizedString("title", IDS_CHROME_SIGNIN_TITLE);
  return source;
}

void AddToSetIfIsAuthIframe(std::set<content::RenderFrameHost*>* frame_set,
                            const GURL& parent_origin,
                            const std::string& parent_frame_name,
                            content::RenderFrameHost* frame) {
  content::RenderFrameHost* parent = frame->GetParent();
  if (parent && parent->GetFrameName() == parent_frame_name &&
      (parent_origin.is_empty() ||
       parent->GetLastCommittedURL().GetOrigin() == parent_origin)) {
    frame_set->insert(frame);
  }
}

bool AddToSetIfSigninWebview(std::set<content::RenderFrameHost*>* frame_set,
                             content::WebContents* web_contents) {
  frame_set->insert(web_contents->GetMainFrame());
  return false;
}

} // empty namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui),
      auth_extension_(Profile::FromWebUI(web_ui)) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());

#if defined(OS_CHROMEOS)
  web_ui->AddMessageHandler(new chromeos::InlineLoginHandlerChromeOS());
#else
  web_ui->AddMessageHandler(new InlineLoginHandlerImpl());
#endif
  content::WebContents* contents = web_ui->GetWebContents();
  // Required for intercepting extension function calls when the page is loaded
  // in a bubble (not a full tab, thus tab helpers are not registered
  // automatically).
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  extensions::TabHelper::CreateForWebContents(contents);
  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from iframes injected by
  // other extensions.
  SessionTabHelper::CreateForWebContents(contents);
}

InlineLoginUI::~InlineLoginUI() {}

// Gets the Gaia iframe within a WebContents.
content::RenderFrameHost* InlineLoginUI::GetAuthIframe(
    content::WebContents* web_contents,
    const GURL& parent_origin,
    const std::string& parent_frame_name) {
  std::set<content::RenderFrameHost*> frame_set;
  bool is_webview = switches::IsEnableWebviewBasedSignin();
#if defined(OS_CHROMEOS)
  is_webview = is_webview || chromeos::StartupUtils::IsWebviewSigninEnabled();
#endif
  if (is_webview) {
    extensions::GuestViewManager* manager =
        extensions::GuestViewManager::FromBrowserContext(
            web_contents->GetBrowserContext());
    manager->ForEachGuest(web_contents,
                          base::Bind(&AddToSetIfSigninWebview, &frame_set));
  } else {
    web_contents->ForEachFrame(
        base::Bind(&AddToSetIfIsAuthIframe, &frame_set,
                   parent_origin, parent_frame_name));
  }
  DCHECK_GE(1U, frame_set.size());
  if (!frame_set.empty())
    return *frame_set.begin();

  return NULL;
}
