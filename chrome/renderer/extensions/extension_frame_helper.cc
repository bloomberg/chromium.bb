// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_frame_helper.h"

#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/console.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace extensions {

ExtensionFrameHelper::ExtensionFrameHelper(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ExtensionFrameHelper>(render_frame) {
}

ExtensionFrameHelper::~ExtensionFrameHelper() {
}

bool ExtensionFrameHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionFrameHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AddMessageToConsole,
                        OnAddMessageToConsole)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionFrameHelper::OnAddMessageToConsole(
    content::ConsoleMessageLevel level,
    const std::string& message) {
  console::AddMessage(render_frame()->GetRenderView(), level, message);
}

}  // namespace extensions
