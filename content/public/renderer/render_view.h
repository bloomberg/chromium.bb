// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageVisibilityState.h"

class FilePath;
struct WebPreferences;

namespace WebKit {
class WebFrame;
class WebNode;
class WebPlugin;
class WebString;
class WebView;
struct WebContextMenuData;
struct WebPluginParams;
}

namespace webkit {
struct WebPluginInfo;
}

namespace content : public IPC::Message::Sender {

class CONTENT_EXPORT RenderView {
 public:
  // Returns the RenderView containing the given WebView.
  static RenderView* FromWebView(WebKit::WebView* webview);

  virtual ~RenderView() {}

  // Get the routing ID of the view.
  virtual int GetRoutingId() const = 0;

  // Page IDs allow the browser to identify pages in each renderer process for
  // keeping back/forward history in sync.
  // Note that this is NOT updated for every main frame navigation, only for
  // "regular" navigations that go into session history. In particular, client
  // redirects, like the page cycler uses (document.location.href="foo") do not
  // count as regular navigations and do not increment the page id.
  virtual int GetPageId() = 0;

  // Gets WebKit related preferences associated with this view.
  virtual WebPreferences& GetWebkitPreferences() = 0;

  // Returns the associated WebView. May return NULL when the view is closing.
  virtual WebKit::WebView* GetWebView() = 0;

  // Gets the focused node. If no such node exists then the node will be isNull.
  virtual WebKit::WebNode GetFocusedNode() const = 0;

  // Gets the node that the context menu was pressed over.
  virtual WebKit::WebNode GetContextMenuNode() const = 0;

  // Returns true if the parameter node is a textfield, text area or a content
  // editable div.
  virtual bool IsEditableNode(const WebKit::WebNode& node) = 0;

  // Create a new NPAPI/Pepper plugin depending on |info|. Returns NULL if no
  // plugin was found.
  virtual WebKit::WebPlugin* CreatePlugin(
      WebKit::WebFrame* frame,
      const webkit::WebPluginInfo& info,
      const WebKit::WebPluginParams& params) = 0;

  // Shows a context menu with commands relevant to a specific element on
  // the given frame. Additional context data is supplied.
  virtual void ShowContextMenu(WebKit::WebFrame* frame,
                               const WebKit::WebContextMenuData& data) = 0;

  // Returns the current visibility of the WebView.
  virtual WebPageVisibilityState GetVisibilityState() const = 0;

  // Displays a modal alert dialog containing the given message.  Returns
  // once the user dismisses the dialog.
  virtual void RunModalAlertDialog(WebKit::WebFrame* frame,
                                   const WebKit::WebString& message) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
