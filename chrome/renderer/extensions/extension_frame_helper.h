// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_FRAME_HELPER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_FRAME_HELPER_H_

#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace extensions {

class Dispatcher;

// RenderFrame-level plumbing for extension features.
class ExtensionFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ExtensionFrameHelper> {
 public:
  ExtensionFrameHelper(content::RenderFrame* render_frame,
                       Dispatcher* extension_dispatcher);
  virtual ~ExtensionFrameHelper();

 private:
  // RenderFrameObserver implementation.
  virtual void WillReleaseScriptContext(v8::Handle<v8::Context>,
                                        int world_id) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC handlers.
  void OnAddMessageToConsole(content::ConsoleMessageLevel level,
                             const std::string& message);

  Dispatcher* extension_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFrameHelper);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_FRAME_HELPER_H_
