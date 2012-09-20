// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
#define  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"

#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/renderer/browser_plugin/browser_plugin_backing_store.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/render_view_impl.h"

struct BrowserPluginMsg_UpdateRect_Params;

namespace content {

class BrowserPluginManager;
class MockBrowserPlugin;

class CONTENT_EXPORT BrowserPlugin :
    NON_EXPORTED_BASE(public WebKit::WebPlugin) {
 public:
  // Called only by tests to clean up before we blow away the MockRenderProcess.
  void Cleanup();

  // Get the src attribute value of the BrowserPlugin instance if the guest
  // has not crashed.
  std::string GetSrcAttribute() const;
  // Set the src attribute value of the BrowserPlugin instance and reset
  // the guest_crashed_ flag.
  void SetSrcAttribute(const std::string& src);

  // Inform the BrowserPlugin to update its backing store with the pixels in
  // its damage buffer.
  void UpdateRect(int message_id,
                  const BrowserPluginMsg_UpdateRect_Params& params);
  // Inform the BrowserPlugin that its guest has crashed.
  void GuestCrashed();
  // Informs the BrowserPlugin that the guest has navigated to a new URL.
  void DidNavigate(const GURL& url);
  // Tells the BrowserPlugin to advance the focus to the next (or previous)
  // element.
  void AdvanceFocus(bool reverse);

  // Indicates whether there are any Javascript listeners attached to a
  // provided event_name.
  bool HasListeners(const std::string& event_name);
  // Add a custom event listener to this BrowserPlugin instance.
  bool AddEventListener(const std::string& event_name,
                        v8::Local<v8::Function> function);
  // Remove a custom event listener from this BrowserPlugin instance.
  bool RemoveEventListener(const std::string& event_name,
                        v8::Local<v8::Function> function);

  // WebKit::WebPlugin implementation.
  virtual WebKit::WebPluginContainer* container() const OVERRIDE;
  virtual bool initialize(WebKit::WebPluginContainer* container) OVERRIDE;
  virtual void destroy() OVERRIDE;
  virtual NPObject* scriptableObject() OVERRIDE;
  virtual bool supportsKeyboardFocus() const OVERRIDE;
  virtual void paint(
      WebKit::WebCanvas* canvas,
      const WebKit::WebRect& rect) OVERRIDE;
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect,
      const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_outs_rects,
      bool is_visible) OVERRIDE;
  virtual void updateFocus(bool focused) OVERRIDE;
  virtual void updateVisibility(bool visible) OVERRIDE;
  virtual bool acceptsInputEvents() OVERRIDE;
  virtual bool handleInputEvent(
      const WebKit::WebInputEvent& event,
      WebKit::WebCursorInfo& cursor_info) OVERRIDE;
  virtual void didReceiveResponse(
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual void didReceiveData(const char* data, int data_length) OVERRIDE;
  virtual void didFinishLoading() OVERRIDE;
  virtual void didFailLoading(const WebKit::WebURLError& error) OVERRIDE;
  virtual void didFinishLoadingFrameRequest(
      const WebKit::WebURL& url,
      void* notify_data) OVERRIDE;
  virtual void didFailLoadingFrameRequest(
      const WebKit::WebURL& url,
      void* notify_data,
      const WebKit::WebURLError& error) OVERRIDE;
 protected:
  friend class base::DeleteHelper<BrowserPlugin>;
  // Only the manager is allowed to create a BrowserPlugin.
  friend class BrowserPluginManagerImpl;
  friend class MockBrowserPluginManager;

  // For unit/integration tests.
  friend class MockBrowserPlugin;

  // A BrowserPlugin object is a controller that represents an instance of a
  // browser plugin within the embedder renderer process. Each BrowserPlugin
  // within a process has a unique instance_id that is used to route messages
  // to it. It takes in a RenderViewImpl that it's associated with along
  // with the frame within which it lives and the initial attributes assigned
  // to it on creation.
  BrowserPlugin(
      int instance_id,
      RenderViewImpl* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  virtual ~BrowserPlugin();

  int width() const { return plugin_rect_.width(); }
  int height() const { return plugin_rect_.height(); }

  // Virtual to allow for mocking in tests.
  virtual float GetDeviceScaleFactor() const;

  // Parses the source URL of the browser plugin from the element's attributes
  // and outputs them.
  bool ParseSrcAttribute(const WebKit::WebPluginParams& params,
                         std::string* src);

  // Cleanup event listener state to free v8 resources when a BrowserPlugin
  // is destroyed.
  void RemoveEventListeners();

  int instance_id_;
  RenderViewImpl* render_view_;
  WebKit::WebPluginContainer* container_;
  scoped_ptr<BrowserPluginBindings> bindings_;
  scoped_ptr<BrowserPluginBackingStore> backing_store_;
  TransportDIB* damage_buffer_;
  gfx::Rect plugin_rect_;
  // Bitmap for crashed plugin. Lazily initialized, non-owning pointer.
  SkBitmap* sad_guest_;
  bool guest_crashed_;
  bool resize_pending_;
  long long parent_frame_;
  std::string src_;
  typedef std::vector<v8::Persistent<v8::Function> > EventListeners;
  typedef std::map<std::string, EventListeners> EventListenerMap;
  EventListenerMap event_listener_map_;
  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
