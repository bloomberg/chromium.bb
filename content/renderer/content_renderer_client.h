// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "content/common/content_client.h"

class GURL;
class RenderView;
class SkBitmap;

namespace WebKit {
class WebFrame;
class WebPlugin;
class WebURLRequest;
struct WebPluginParams;
struct WebURLError;
}

namespace content {

// Embedder API for participating in renderer logic.
class ContentRendererClient {
 public:
  // Notifies that a new RenderView has been created.
  virtual void RenderViewCreated(RenderView* render_view);

  // Returns the bitmap to show when a plugin crashed, or NULL for none.
  virtual SkBitmap* GetSadPluginBitmap();

  // Returns the default text encoding.
  virtual std::string GetDefaultEncoding();

  // Create a plugin in the given frame.  Can return NULL, in which case
  // RenderView will create a plugin itself.
  virtual WebKit::WebPlugin* CreatePlugin(
      RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  // Returns the html to display when a navigation error occurs.
  virtual std::string GetNavigationErrorHtml(
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error);

  // Returns true if the renderer process should schedule the idle handler when
  // all widgets are hidden.
  virtual bool RunIdleHandlerWhenWidgetsHidden();

  // Returns true if the given url can create popup windows.
  virtual bool AllowPopup(const GURL& creator);

  // Returns true if we should fork a new process for the given navigation.
  virtual bool ShouldFork(WebKit::WebFrame* frame,
                          const GURL& url,
                          bool is_content_initiated,
                          bool* send_referrer);

  // Notifies the embedder that the given frame is requesting the resource at
  // |url|.  If the function returns true, the url is changed to |new_url|.
  virtual bool WillSendRequest(WebKit::WebFrame* frame,
                               const GURL& url,
                               GURL* new_url);

  // See the corresponding functions in WebKit::WebFrameClient.
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame);
  virtual void DidDestroyScriptContext(WebKit::WebFrame* frame);
  virtual void DidCreateIsolatedScriptContext(WebKit::WebFrame* frame);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
