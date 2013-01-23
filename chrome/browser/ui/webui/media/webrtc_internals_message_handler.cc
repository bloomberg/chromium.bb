// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/webrtc_internals_message_handler.h"

#include "chrome/browser/media/chrome_webrtc_internals.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

using content::BrowserThread;

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler() {
  ChromeWebRTCInternals::GetInstance()->AddObserver(this);
}

WebRTCInternalsMessageHandler::~WebRTCInternalsMessageHandler() {
  ChromeWebRTCInternals::GetInstance()->RemoveObserver(this);
}

void WebRTCInternalsMessageHandler::RegisterMessages() {
}

void WebRTCInternalsMessageHandler::OnUpdate(const std::string& command,
                                            const base::Value* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<const Value*> args_vector;
  args_vector.push_back(args);
  string16 update = content::WebUI::GetJavascriptCall(command, args_vector);

  content::RenderViewHost* host =
      web_ui()->GetWebContents()->GetRenderViewHost();
  if (host)
    host->ExecuteJavascriptInWebFrame(string16(), update);
}
