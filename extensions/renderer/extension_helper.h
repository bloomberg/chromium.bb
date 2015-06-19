// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_HELPER_H_

#include <vector>

#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"

namespace base {
class ListValue;
}

namespace extensions {
class Dispatcher;

// RenderView-level plumbing for extension features.
class ExtensionHelper
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<ExtensionHelper> {
 public:
  ExtensionHelper(content::RenderView* render_view, Dispatcher* dispatcher);
  ~ExtensionHelper() override;

  int browser_window_id() const { return browser_window_id_; }
  Dispatcher* dispatcher() const { return dispatcher_; }

 private:
  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidCreateDocumentElement(blink::WebLocalFrame* frame) override;
  void DidMatchCSS(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      override;
  void DraggableRegionsChanged(blink::WebFrame* frame) override;

  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::ListValue& args,
                                bool user_gesture);
  void OnAddMessageToConsole(content::ConsoleMessageLevel level,
                             const std::string& message);
  void OnAppWindowClosed();
  void OnSetFrameName(const std::string& name);

  Dispatcher* dispatcher_;

  // Id number of browser window which RenderView is attached to.
  int browser_window_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_HELPER_H_
