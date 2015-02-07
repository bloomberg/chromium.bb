// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_HELPER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "extensions/common/view_type.h"

class GURL;
class SkBitmap;

namespace base {
class ListValue;
}

namespace extensions {
class AutomationApiHelper;
class Dispatcher;
class URLPatternSet;

// RenderView-level plumbing for extension features.
class ExtensionHelper
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<ExtensionHelper> {
 public:
  // Returns a list of extension RenderViews that match the given filter
  // criteria. If |browser_window_id| is not extension_misc::kUnknownWindowId,
  // the list is restricted to views in that browser window.
  static std::vector<content::RenderView*> GetExtensionViews(
      const std::string& extension_id,
      int browser_window_id,
      ViewType view_type);

  // Returns the given extension's background page, or NULL if none.
  static content::RenderView* GetBackgroundPage(
      const std::string& extension_id);

  ExtensionHelper(content::RenderView* render_view, Dispatcher* dispatcher);
  ~ExtensionHelper() override;

  int tab_id() const { return tab_id_; }
  int browser_window_id() const { return browser_window_id_; }
  ViewType view_type() const { return view_type_; }
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

  void OnExtensionResponse(int request_id, bool success,
                           const base::ListValue& response,
                           const std::string& error);
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::ListValue& args,
                                bool user_gesture);
  void OnNotifyRendererViewType(ViewType view_type);
  void OnSetTabId(int tab_id);
  void OnUpdateBrowserWindowId(int window_id);
  void OnAddMessageToConsole(content::ConsoleMessageLevel level,
                             const std::string& message);
  void OnAppWindowClosed();
  void OnSetFrameName(const std::string& name);

  Dispatcher* dispatcher_;

  // Type of view attached with RenderView.
  ViewType view_type_;

  // Id of the tab which the RenderView is attached to.
  int tab_id_;

  // Id number of browser window which RenderView is attached to.
  int browser_window_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_HELPER_H_
