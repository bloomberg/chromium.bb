// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginEmbedder has a list of guests it manages.
// In the beginning when a renderer sees one or more guests (BrowserPlugin
// instance(s)) and there is a request to navigate to them, the WebContents for
// that renderer creates a BrowserPluginEmbedder for itself. The
// BrowserPluginEmbedder, in turn, manages a set of BrowserPluginGuests -- one
// BrowserPluginGuest for each guest in the embedding WebContents. Note that
// each of these BrowserPluginGuest objects has its own WebContents.
// BrowserPluginEmbedder routes any messages directed to a guest from the
// renderer (BrowserPlugin) to the appropriate guest (identified by the guest's
// |instance_id|).
//
// BrowserPluginEmbedder is responsible for cleaning up the guests when the
// embedder frame navigates away to a different page or deletes the guests from
// the existing page.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_

#include <map>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragStatus.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/surface/transport_dib.h"
#include "webkit/glue/webdropdata.h"

struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_CreateGuest_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;

namespace WebKit {
class WebInputEvent;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace content {

class BrowserPluginGuest;
class BrowserPluginHostFactory;
class WebContentsImpl;

// A browser plugin embedder provides functionality for WebContents to operate
// in the 'embedder' role. It manages list of guests inside the embedder.
//
// The embedder's WebContents manages the lifetime of the embedder. They are
// created when a renderer asks WebContents to navigate (for the first time) to
// some guest. It gets destroyed when either the WebContents goes away or there
// is a RenderViewHost swap in WebContents.
class CONTENT_EXPORT BrowserPluginEmbedder : public WebContentsObserver,
                                             public NotificationObserver {
 public:
  typedef std::map<int, WebContents*> ContainerInstanceMap;

  virtual ~BrowserPluginEmbedder();

  static BrowserPluginEmbedder* Create(WebContentsImpl* web_contents,
                                       RenderViewHost* render_view_host);

  // Creates a new guest.
  void CreateGuest(RenderViewHost* render_view_host,
                   int instance_id,
                   const BrowserPluginHostMsg_CreateGuest_Params& params);

  // Navigates in a guest (new or existing).
  void NavigateGuest(
      RenderViewHost* render_view_host,
      int instance_id,
      const std::string& src);

  void ResizeGuest(RenderViewHost* render_view_host,
                   int instance_id,
                   const BrowserPluginHostMsg_ResizeGuest_Params& params);

  void Go(int instance_id, int relative_index);
  void Stop(int instance_id);
  void Reload(int instance_id);
  void TerminateGuest(int instance_id);

  // WebContentsObserver implementation.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // NotificationObserver method override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Message handlers (direct/indirect via BrowserPluginEmbedderHelper).
  // Routes update rect ack message to the appropriate guest.
  void UpdateRectACK(
      int instance_id,
      int message_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);
  void SetFocus(int instance_id, bool focused);
  // Handles input events sent from the BrowserPlugin (embedder's renderer
  // process) by passing them to appropriate guest's input handler. The
  // BrowserPlugin behaves like a black hole for events, so the embedder does
  // not see them in the capture or bubble phase.
  // Currently scroll events do not propagate back to the embedder process
  // and so even if the guest discards a scroll event, it won't make its
  // way back to the embedder. This may change in the future.
  void HandleInputEvent(int instance_id,
                        RenderViewHost* render_view_host,
                        const gfx::Rect& guest_window_rect,
                        const WebKit::WebInputEvent& event);
  void PluginDestroyed(int instance_id);
  void SetGuestVisibility(int instance_id,
                          bool guest_visible);
  void DragStatusUpdate(int instance_id,
                        WebKit::WebDragStatus drag_status,
                        const WebDropData& drop_data,
                        WebKit::WebDragOperationsMask drag_mask,
                        const gfx::Point& location);
  void SetAutoSize(
      int instance_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);

  bool visible() const { return visible_; }

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    factory_ = factory;
  }

  // Returns the RenderViewHost at a point (|x|, |y|) asynchronously via
  // |callback|. We need a roundtrip to renderer process to get this
  // information.
  void GetRenderViewHostAtPosition(
      int x,
      int y,
      const WebContents::GetRenderViewHostCallback& callback);
  void PluginAtPositionResponse(int instance_id,
                                int request_id,
                                const gfx::Point& position);

 private:
  friend class TestBrowserPluginEmbedder;

  BrowserPluginEmbedder(WebContentsImpl* web_contents,
                        RenderViewHost* render_view_host);

  // Returns a guest browser plugin delegate by its container ID specified
  // in BrowserPlugin.
  BrowserPluginGuest* GetGuestByInstanceID(int instance_id) const;
  // Adds a new guest web_contents to the embedder (overridable in test).
  virtual void AddGuest(int instance_id, WebContents* guest_web_contents);
  void DestroyGuestByInstanceID(int instance_id);
  void CleanUp();

  // Called when visiblity of web_contents changes, so the embedder will
  // show/hide its guest.
  void WebContentsVisibilityChanged(bool visible);

  // Static factory instance (always NULL for non-test).
  static BrowserPluginHostFactory* factory_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  // Contains guests' WebContents, mapping from their instance ids.
  ContainerInstanceMap guest_web_contents_by_instance_id_;
  RenderViewHost* render_view_host_;
  // Tracks the visibility state of the embedder.
  bool visible_;
  // Map that contains outstanding queries to |GetBrowserPluginAt|.
  // We need a roundtrip to renderer process to know the answer, therefore
  // storing these callbacks is required.
  typedef std::map<int, WebContents::GetRenderViewHostCallback>
      GetRenderViewHostCallbackMap;
  GetRenderViewHostCallbackMap pending_get_render_view_callbacks_;
  // Next request id for BrowserPluginMsg_PluginAtPositionRequest query.
  int next_get_render_view_request_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginEmbedder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
