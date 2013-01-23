// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/webrtc_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/media/webrtc_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"

using content::WebContents;

namespace {

content::WebUIDataSource* CreateWebRTCInternalsHTMLSource() {
  content::WebUIDataSource* source =
      ChromeWebUIDataSource::Create(chrome::kChromeUIWebRTCInternalsHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("webrtc_internals.js", IDR_WEBRTC_INTERNALS_JS);
  source->SetDefaultResource(IDR_WEBRTC_INTERNALS_HTML);
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// WebRTCInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

WebRTCInternalsUI::WebRTCInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new WebRTCInternalsMessageHandler());

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddWebUIDataSource(profile,
                                           CreateWebRTCInternalsHTMLSource());
}
