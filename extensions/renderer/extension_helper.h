// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_HELPER_H_

#include <vector>

#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "extensions/common/view_type.h"

class GURL;
class SkBitmap;
struct ExtensionMsg_ExternalConnectionInfo;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class Dispatcher;
struct Message;

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
  virtual ~ExtensionHelper();

  int tab_id() const { return tab_id_; }
  int browser_window_id() const { return browser_window_id_; }
  ViewType view_type() const { return view_type_; }
  Dispatcher* dispatcher() const { return dispatcher_; }

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCreateDocumentElement(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidMatchCSS(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      OVERRIDE;
  virtual void DraggableRegionsChanged(blink::WebFrame* frame) OVERRIDE;

  void OnExtensionResponse(int request_id, bool success,
                           const base::ListValue& response,
                           const std::string& error);
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::ListValue& args,
                                bool user_gesture);
  void OnExtensionDispatchOnConnect(
      int target_port_id,
      const std::string& channel_name,
      const base::DictionaryValue& source_tab,
      const ExtensionMsg_ExternalConnectionInfo& info,
      const std::string& tls_channel_id);
  void OnExtensionDeliverMessage(int target_port_id,
                                 const Message& message);
  void OnExtensionDispatchOnDisconnect(int port_id,
                                       const std::string& error_message);
  void OnNotifyRendererViewType(ViewType view_type);
  void OnSetTabId(int tab_id);
  void OnUpdateBrowserWindowId(int window_id);
  void OnAddMessageToConsole(content::ConsoleMessageLevel level,
                             const std::string& message);
  void OnAppWindowClosed();

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
