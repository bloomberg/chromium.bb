// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_frame_helper.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

ExtensionFrameHelper::ExtensionFrameHelper(content::RenderFrame* render_frame,
                                           Dispatcher* extension_dispatcher)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ExtensionFrameHelper>(render_frame),
      extension_dispatcher_(extension_dispatcher) {}

ExtensionFrameHelper::~ExtensionFrameHelper() {
}

void ExtensionFrameHelper::WillReleaseScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
// TODO(thestig): Remove scaffolding once this file no longer builds with
// extensions disabled.
#if defined(ENABLE_EXTENSIONS)
  extension_dispatcher_->WillReleaseScriptContext(
      render_frame()->GetWebFrame(), context, world_id);
#endif
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
