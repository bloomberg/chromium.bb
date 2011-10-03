// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_internals_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/media/media_internals_proxy.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

MediaInternalsMessageHandler::MediaInternalsMessageHandler()
    : proxy_(new MediaInternalsProxy()) {}

MediaInternalsMessageHandler::~MediaInternalsMessageHandler() {
  proxy_->Detach();
}

WebUIMessageHandler* MediaInternalsMessageHandler::Attach(WebUI* web_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);
  proxy_->Attach(this);
  return result;
}

void MediaInternalsMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui_->RegisterMessageCallback("getEverything",
      base::Bind(&MediaInternalsMessageHandler::OnGetEverything,
                 base::Unretained(this)));
}

void MediaInternalsMessageHandler::OnGetEverything(const ListValue* list) {
  proxy_->GetEverything();
}

void MediaInternalsMessageHandler::OnUpdate(const string16& update) {
  // Don't try to execute JavaScript in a RenderView that no longer exists.
  RenderViewHost* host = web_ui_->tab_contents()->render_view_host();
  if (host)
    host->ExecuteJavascriptInWebFrame(string16(), update);
}
