// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
#define COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace blink {
class WebMouseEvent;
}

namespace content {
class RenderView;
struct WebPreferences;
}

// This class implements the WebPlugin interface by forwarding drawing and
// handling input events to a WebView.
// It can be used as a placeholder for an actual plugin, using HTML for the UI.
// To show HTML data inside the WebViewPlugin,
// call web_view->mainFrame()->loadHTMLString() with the HTML data and a fake
// chrome:// URL as origin.

class WebViewPlugin : public blink::WebPlugin,
                      public blink::WebViewClient,
                      public blink::WebFrameClient {
 public:
  class Delegate {
   public:
    // Bind |frame| to a Javascript object, enabling the delegate to receive
    // callback methods from Javascript inside the WebFrame.
    // This method is called from WebFrameClient::didClearWindowObject.
    virtual void BindWebFrame(blink::WebFrame* frame) = 0;

    // Called upon a context menu event.
    virtual void ShowContextMenu(const blink::WebMouseEvent&) = 0;

    // Called when the WebViewPlugin is destroyed.
    virtual void PluginDestroyed() = 0;
  };

  // Convenience method to set up a new WebViewPlugin using |preferences|
  // and displaying |html_data|. |url| should be a (fake) chrome:// URL; it is
  // only used for navigation and never actually resolved.
  static WebViewPlugin* Create(Delegate* delegate,
                               const content::WebPreferences& preferences,
                               const std::string& html_data,
                               const GURL& url);

  blink::WebView* web_view() { return web_view_; }

  // When loading a plug-in document (i.e. a full page plug-in not embedded in
  // another page), we save all data that has been received, and replay it with
  // this method on the actual plug-in.
  void ReplayReceivedData(blink::WebPlugin* plugin);

  void RestoreTitleText();

  // WebPlugin methods:
  virtual blink::WebPluginContainer* container() const;
  virtual bool initialize(blink::WebPluginContainer*);
  virtual void destroy();

  virtual NPObject* scriptableObject();
  virtual struct _NPP* pluginNPP();

  virtual bool getFormValue(blink::WebString& value);

  virtual void paint(blink::WebCanvas* canvas, const blink::WebRect& rect);

  // Coordinates are relative to the containing window.
  virtual void updateGeometry(
      const blink::WebRect& frame_rect,
      const blink::WebRect& clip_rect,
      const blink::WebVector<blink::WebRect>& cut_out_rects,
      bool is_visible);

  virtual void updateFocus(bool);
  virtual void updateVisibility(bool) {}

  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(const blink::WebInputEvent& event,
                                blink::WebCursorInfo& cursor_info);

  virtual void didReceiveResponse(const blink::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const blink::WebURLError& error);

  // Called in response to WebPluginContainer::loadFrameRequest
  virtual void didFinishLoadingFrameRequest(const blink::WebURL& url,
                                            void* notifyData) {}
  virtual void didFailLoadingFrameRequest(const blink::WebURL& url,
                                          void* notify_data,
                                          const blink::WebURLError& error) {}

  // WebViewClient methods:
  virtual bool acceptsLoadDrops();

  virtual void setToolTipText(const blink::WebString&,
                              blink::WebTextDirection);

  virtual void startDragging(blink::WebLocalFrame* frame,
                             const blink::WebDragData& drag_data,
                             blink::WebDragOperationsMask mask,
                             const blink::WebImage& image,
                             const blink::WebPoint& point);

  // TODO(ojan): Remove this override and have this class use a non-null
  // layerTreeView.
  virtual bool allowsBrokenNullLayerTreeView() const;

  // WebWidgetClient methods:
  virtual void didInvalidateRect(const blink::WebRect&);
  virtual void didChangeCursor(const blink::WebCursorInfo& cursor);
  virtual void scheduleAnimation();

  // WebFrameClient methods:
  virtual void didClearWindowObject(blink::WebLocalFrame* frame);

  // This method is defined in WebPlugin as well as in WebFrameClient, but with
  // different parameters. We only care about implementing the WebPlugin
  // version, so we implement this method and call the default in WebFrameClient
  // (which does nothing) to correctly overload it.
  virtual void didReceiveResponse(blink::WebLocalFrame* frame,
                                  unsigned identifier,
                                  const blink::WebURLResponse& response);

 private:
  friend class base::DeleteHelper<WebViewPlugin>;
  WebViewPlugin(Delegate* delegate, const content::WebPreferences& preferences);
  virtual ~WebViewPlugin();

  // Manages its own lifetime.
  Delegate* delegate_;

  blink::WebCursorInfo current_cursor_;

  // Owns us.
  blink::WebPluginContainer* container_;

  // Owned by us, deleted via |close()|.
  blink::WebView* web_view_;

  // Owned by us, deleted via |close()|.
  blink::WebFrame* web_frame_;
  gfx::Rect rect_;

  blink::WebURLResponse response_;
  std::list<std::string> data_;
  bool finished_loading_;
  scoped_ptr<blink::WebURLError> error_;
  blink::WebString old_title_;
  bool focused_;
};

#endif  // COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
