// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/custom_launcher_page_contents.h"

#include <string>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/common/extension_messages.h"

namespace apps {

CustomLauncherPageContents::CustomLauncherPageContents() {
}

CustomLauncherPageContents::~CustomLauncherPageContents() {
}

void CustomLauncherPageContents::Initialize(content::BrowserContext* context,
                                            const GURL& url) {
  extension_function_dispatcher_.reset(
      new extensions::ExtensionFunctionDispatcher(context, this));

  web_contents_.reset(
      content::WebContents::Create(content::WebContents::CreateParams(
          context, content::SiteInstance::CreateForURL(context, url))));

  Observe(web_contents());
  web_contents_->GetMutableRendererPrefs()
      ->browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  // This observer will activate the extension when it is navigated to, which
  // allows Dispatcher to give it the proper context and makes it behave like an
  // extension.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents());

  web_contents_->GetController().LoadURL(url,
                                         content::Referrer(),
                                         content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                         std::string());
}

bool CustomLauncherPageContents::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CustomLauncherPageContents, message)
  IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

extensions::WindowController*
CustomLauncherPageContents::GetExtensionWindowController() const {
  return NULL;
}

content::WebContents* CustomLauncherPageContents::GetAssociatedWebContents()
    const {
  return web_contents();
}

void CustomLauncherPageContents::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(params,
                                           web_contents_->GetRenderViewHost());
}

}  // namespace apps
