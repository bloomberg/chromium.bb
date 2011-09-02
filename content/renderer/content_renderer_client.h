// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ipc/ipc_message.h"
#include "content/common/content_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageVisibilityState.h"

class FilePath;
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

namespace v8 {
class Context;
template<class T> class Handle;
}

namespace content {

// Embedder API for participating in renderer logic.
class ContentRendererClient {
 public:
  virtual ~ContentRendererClient() {}

  // Notifies us that the RenderThread has been created.
  virtual void RenderThreadStarted() = 0;

  // Notifies that a new RenderView has been created.
  virtual void RenderViewCreated(RenderView* render_view) = 0;

  // Sets a number of views/tabs opened in this process.
  virtual void SetNumberOfViews(int number_of_views) = 0;

  // Returns the bitmap to show when a plugin crashed, or NULL for none.
  virtual SkBitmap* GetSadPluginBitmap() = 0;

  // Returns the default text encoding.
  virtual std::string GetDefaultEncoding() = 0;

  // Create a plugin in the given frame.  Can return NULL, in which case
  // RenderView will create a plugin itself.
  virtual WebKit::WebPlugin* CreatePlugin(
      RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params) = 0;

  // Give the embedder the ability to set an error page.
  virtual void ShowErrorPage(RenderView* render_view,
                             WebKit::WebFrame* frame,
                             int http_status_code) = 0;

  // Returns the html to display when a navigation error occurs.
  virtual std::string GetNavigationErrorHtml(
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error) = 0;

  // Returns true if the renderer process should schedule the idle handler when
  // all widgets are hidden.
  virtual bool RunIdleHandlerWhenWidgetsHidden() = 0;

  // Returns true if the given url can create popup windows.
  virtual bool AllowPopup(const GURL& creator) = 0;

  // Returns true if we should fork a new process for the given navigation.
  virtual bool ShouldFork(WebKit::WebFrame* frame,
                          const GURL& url,
                          bool is_content_initiated,
                          bool is_initial_navigation,
                          bool* send_referrer) = 0;

  // Notifies the embedder that the given frame is requesting the resource at
  // |url|.  If the function returns true, the url is changed to |new_url|.
  virtual bool WillSendRequest(WebKit::WebFrame* frame,
                               const GURL& url,
                               GURL* new_url) = 0;

  // Whether to pump events when sending sync cookie messages.  Needed if the
  // embedder can potentiall put up a modal dialog on the UI thread as a result.
  virtual bool ShouldPumpEventsDuringCookieMessage() = 0;

  // See the corresponding functions in WebKit::WebFrameClient.
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame) = 0;
  virtual void DidDestroyScriptContext(WebKit::WebFrame* frame) = 0;
  virtual void DidCreateIsolatedScriptContext(
      WebKit::WebFrame* frame,
      int world_id,
      v8::Handle<v8::Context> context) = 0;

  // See WebKit::WebKitPlatformSupport.
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) = 0;
  virtual bool IsLinkVisited(unsigned long long link_hash) = 0;
  virtual void PrefetchHostName(const char* hostname, size_t length) = 0;
  virtual bool ShouldOverridePageVisibilityState(
      const RenderView* render_view,
      WebKit::WebPageVisibilityState* override_state) const = 0;

  // Return true if the GetCookie request will be handled by the embedder.
  // Cookies are returned in the cookie parameter.
  virtual bool HandleGetCookieRequest(RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      std::string* cookies) = 0;

  // Return true if the SetCookie request will be handled by the embedder.
  // Cookies to be set are passed in the value parameter.
  virtual bool HandleSetCookieRequest(RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& value) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
