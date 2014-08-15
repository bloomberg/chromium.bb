// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/login/inline_login_handler_chromeos.h"
#else
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#endif

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIChromeSigninHost);
  source->OverrideContentSecurityPolicyFrameSrc("frame-src chrome-extension:;");
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_INLINE_LOGIN_HTML);
  source->AddResourcePath("inline_login.css", IDR_INLINE_LOGIN_CSS);
  source->AddResourcePath("inline_login.js", IDR_INLINE_LOGIN_JS);

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
  web_contents->ForEachFrame(
      base::Bind(&AddToSetIfIsAuthIframe, &frame_set,
                 parent_origin, parent_frame_name));
  DCHECK_GE(1U, frame_set.size());
  if (!frame_set.empty())
    return *frame_set.begin();

  return NULL;
}
