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

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

struct BrowserPluginHostMsg_CreateGuest_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;

namespace gfx {
class Point;
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
class CONTENT_EXPORT BrowserPluginEmbedder : public WebContentsObserver {
 public:
  typedef std::map<int, WebContents*> ContainerInstanceMap;

  virtual ~BrowserPluginEmbedder();

  static BrowserPluginEmbedder* Create(WebContentsImpl* web_contents,
                                       RenderViewHost* render_view_host);

  // Creates a guest WebContents with the provided |instance_id| and |params|
  // and adds it to this BrowserPluginEmbedder. If params.src is present, the
  // new guest will also be navigated to the provided URL. Optionally, the new
  // guest may be attached to a |guest_opener|, and may be attached to a pre-
  // selected |routing_id|.
  void CreateGuest(int instance_id,
                   int routing_id,
                   BrowserPluginGuest* guest_opener,
                   const BrowserPluginHostMsg_CreateGuest_Params& params);

  // Returns a guest browser plugin delegate by its container ID specified
  // in BrowserPlugin.
  BrowserPluginGuest* GetGuestByInstanceID(int instance_id) const;

  // Adds a new guest web_contents to the embedder (overridable in test).
  virtual void AddGuest(int instance_id, WebContents* guest_web_contents);
  void RemoveGuest(int instance_id);

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

  // WebContentsObserver implementation.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  friend class TestBrowserPluginEmbedder;

  BrowserPluginEmbedder(WebContentsImpl* web_contents,
                        RenderViewHost* render_view_host);

  void CleanUp();

  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);

  // Message handlers.

  void OnAllocateInstanceID(int request_id);
  void OnCreateGuest(int instance_id,
                     const BrowserPluginHostMsg_CreateGuest_Params& params);
  void OnPluginAtPositionResponse(int instance_id,
                                  int request_id,
                                  const gfx::Point& position);
  void OnUnhandledSwapBuffersACK(int instance_id,
                                 int route_id,
                                 int gpu_host_id,
                                 const std::string& mailbox_name,
                                 uint32 sync_point);

  // Static factory instance (always NULL for non-test).
  static BrowserPluginHostFactory* factory_;

  // Contains guests' WebContents, mapping from their instance ids.
  ContainerInstanceMap guest_web_contents_by_instance_id_;
  RenderViewHost* render_view_host_;
  // Map that contains outstanding queries to |GetBrowserPluginAt|.
  // We need a roundtrip to renderer process to know the answer, therefore
  // storing these callbacks is required.
  typedef std::map<int, WebContents::GetRenderViewHostCallback>
      GetRenderViewHostCallbackMap;
  GetRenderViewHostCallbackMap pending_get_render_view_callbacks_;
  // Next request id for BrowserPluginMsg_PluginAtPositionRequest query.
  int next_get_render_view_request_id_;
  int next_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginEmbedder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
