// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_resources_provider.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace media_router {

MediaRouterUI::MediaRouterUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      handler_(new MediaRouterWebUIMessageHandler()) {
  // Create a WebUIDataSource containing the chrome://media-router page's
  // content.
  scoped_ptr<content::WebUIDataSource> html_source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaRouterHost));
  AddLocalizedStrings(html_source.get());
  AddMediaRouterUIResources(html_source.get());
  Profile* profile = Profile::FromWebUI(web_ui);
  // Ownership of |html_source| is transferred to the BrowserContext.
  content::WebUIDataSource::Add(profile, html_source.release());
  // Ownership of |handler_| is transferred to |web_ui|.
  web_ui->AddMessageHandler(handler_);
}

MediaRouterUI::~MediaRouterUI() {
}

void MediaRouterUI::Close() {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (delegate) {
    delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
    delegate->OnDialogCloseFromWebUI();
  }
}

}  // namespace media_router

