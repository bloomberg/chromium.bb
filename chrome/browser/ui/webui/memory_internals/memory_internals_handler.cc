// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals/memory_internals_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/memory_internals/memory_internals_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

MemoryInternalsHandler::MemoryInternalsHandler()
    : proxy_(new MemoryInternalsProxy()) {}

MemoryInternalsHandler::~MemoryInternalsHandler() {
  proxy_->Detach();
}

void MemoryInternalsHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_->Attach(this);

  // Set callback functions called by JavaScript messages.
  web_ui()->RegisterMessageCallback(
      "update",
      base::Bind(&MemoryInternalsHandler::OnJSUpdate,
                 base::Unretained(this)));
}

void MemoryInternalsHandler::OnJSUpdate(const base::ListValue* list) {
  proxy_->StartFetch(list);
}

void MemoryInternalsHandler::OnUpdate(const base::string16& update) {
  // Don't try to execute JavaScript in a RenderFrame that no longer exists.
  content::RenderFrameHost* frame = web_ui()->GetWebContents()->GetMainFrame();
  if (frame)
    frame->ExecuteJavaScript(update);
}
