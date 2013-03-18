// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginEmbedder handles messages coming from a BrowserPlugin's
// embedder that are not directed at any particular existing guest process.
// In the beginning, when a BrowserPlugin instance in the embedder renderer
// process requests an initial navigation, the WebContents for that renderer
// renderer creates a BrowserPluginEmbedder for itself. The
// BrowserPluginEmbedder, in turn, forwards the requests to a
// BrowserPluginGuestManager, which creates and manages the lifetime of the new
// guest.

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

class BrowserPluginGuestManager;
class BrowserPluginHostFactory;
class WebContentsImpl;

class CONTENT_EXPORT BrowserPluginEmbedder : public WebContentsObserver {
 public:
  virtual ~BrowserPluginEmbedder();

  static BrowserPluginEmbedder* Create(WebContentsImpl* web_contents);

  // Returns the RenderViewHost at a point (|x|, |y|) asynchronously via
  // |callback|. We need a roundtrip to renderer process to get this
  // information.
  void GetRenderViewHostAtPosition(
      int x,
      int y,
      const WebContents::GetRenderViewHostCallback& callback);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    factory_ = factory;
  }

  // WebContentsObserver implementation.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  friend class TestBrowserPluginEmbedder;

  BrowserPluginEmbedder(WebContentsImpl* web_contents);

  void CleanUp();

  BrowserPluginGuestManager* GetBrowserPluginGuestManager();

  // Message handlers.

  void OnAllocateInstanceID(int request_id);
  void OnAttach(int instance_id,
                const BrowserPluginHostMsg_CreateGuest_Params& params);
  void OnCreateGuest(int instance_id,
                     const BrowserPluginHostMsg_CreateGuest_Params& params);
  void OnPluginAtPositionResponse(int instance_id,
                                  int request_id,
                                  const gfx::Point& position);

  // Static factory instance (always NULL for non-test).
  static BrowserPluginHostFactory* factory_;

  // Map that contains outstanding queries to |GetRenderViewHostAtPosition|.
  // We need a roundtrip to the renderer process to retrieve the answer,
  // so we store these callbacks until we hear back from the renderer.
  typedef std::map<int, WebContents::GetRenderViewHostCallback>
      GetRenderViewHostCallbackMap;
  GetRenderViewHostCallbackMap pending_get_render_view_callbacks_;
  // Next request id for BrowserPluginMsg_PluginAtPositionRequest query.
  int next_get_render_view_request_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginEmbedder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
