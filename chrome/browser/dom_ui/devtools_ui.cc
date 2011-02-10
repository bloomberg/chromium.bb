// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/devtools_ui.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/render_messages.h"

DevToolsUI::DevToolsUI(TabContents* contents) : WebUI(contents) {
}

void DevToolsUI::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(new ViewMsg_SetupDevToolsClient(
      render_view_host->routing_id()));
}
