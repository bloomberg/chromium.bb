// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
#define CONTENT_RENDERER_RENDER_FRAME_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "content/common/content_export.h"
#include "content/common/feature_policy/feature_policy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebFocusType.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "url/origin.h"

#if defined(USE_AURA)
#include "content/renderer/mus/mus_embedded_frame_delegate.h"
#endif

namespace blink {
struct WebRect;
}

namespace viz {
class SurfaceInfo;
struct SurfaceSequence;
}

namespace content {

class ChildFrameCompositingHelper;
class RenderFrameImpl;
class RenderViewImpl;
class RenderWidget;
struct ContentSecurityPolicyHeader;
struct FrameOwnerProperties;
struct FramePolicy;
struct FrameReplicationState;

#if defined(USE_AURA)
class MusEmbeddedFrame;
#endif

// When a page's frames are rendered by multiple processes, each renderer has a
// full copy of the frame tree. It has full RenderFrames for the frames it is
// responsible for rendering and placeholder objects for frames rendered by
// other processes. This class is the renderer-side object for the placeholder.
// RenderFrameProxy allows us to keep existing window references valid over
// cross-process navigations and route cross-site asynchronous JavaScript calls,
// such as postMessage.
//
// For now, RenderFrameProxy is created when RenderFrame is swapped out. It
// acts as a wrapper and is used for sending and receiving IPC messages. It is
// deleted when the RenderFrame is swapped back in or the node of the frame
// tree is deleted.
//
// Long term, RenderFrameProxy will be created to replace the RenderFrame in the
// frame tree and the RenderFrame will be deleted after its unload handler has
// finished executing. It will still be responsible for routing IPC messages
// which are valid for cross-site interactions between frames.
// RenderFrameProxy will be deleted when the node in the frame tree is deleted
// or when navigating the frame causes it to return to this process and a new
// RenderFrame is created for it.
class CONTENT_EXPORT RenderFrameProxy : public IPC::Listener,
                                        public IPC::Sender,
#if defined(USE_AURA)
                                        public MusEmbeddedFrameDelegate,
#endif
                                        public blink::WebRemoteFrameClient {
 public:
  // This method should be used to create a RenderFrameProxy, which will replace
  // an existing RenderFrame during its cross-process navigation from the
  // current process to a different one. |routing_id| will be ID of the newly
  // created RenderFrameProxy. |frame_to_replace| is the frame that the new
  // proxy will eventually swap places with.
  static RenderFrameProxy* CreateProxyToReplaceFrame(
      RenderFrameImpl* frame_to_replace,
      int routing_id,
      blink::WebTreeScopeType scope);

  // This method should be used to create a RenderFrameProxy, when there isn't
  // an existing RenderFrame. It should be called to construct a local
  // representation of a RenderFrame that has been created in another process --
  // for example, after a cross-process navigation or after the addition of a
  // new frame local to some other process. |routing_id| will be the ID of the
  // newly created RenderFrameProxy. |render_view_routing_id| identifies the
  // RenderView to be associated with this frame.  |opener|, if supplied, is the
  // new frame's opener.  |parent_routing_id| is the routing ID of the
  // RenderFrameProxy to which the new frame is parented.
  //
  // |parent_routing_id| always identifies a RenderFrameProxy (never a
  // RenderFrame) because a new child of a local frame should always start out
  // as a frame, not a proxy.
  static RenderFrameProxy* CreateFrameProxy(
      int routing_id,
      int render_view_routing_id,
      blink::WebFrame* opener,
      int parent_routing_id,
      const FrameReplicationState& replicated_state);

  // Returns the RenderFrameProxy for the given routing ID.
  static RenderFrameProxy* FromRoutingID(int routing_id);

  // Returns the RenderFrameProxy given a WebRemoteFrame. |web_frame| must not
  // be null, nor will this method return null.
  static RenderFrameProxy* FromWebFrame(blink::WebRemoteFrame* web_frame);

  ~RenderFrameProxy() override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // Out-of-process child frames receive a signal from RenderWidgetCompositor
  // when a compositor frame will begin.
  void WillBeginCompositorFrame();

  // Out-of-process child frames receive a signal from RenderWidgetCompositor
  // when a compositor frame has committed.
  void DidCommitCompositorFrame();

  // Pass replicated information, such as security origin, to this
  // RenderFrameProxy's WebRemoteFrame.
  void SetReplicatedState(const FrameReplicationState& state);

  int routing_id() { return routing_id_; }
  RenderViewImpl* render_view() { return render_view_; }
  blink::WebRemoteFrame* web_frame() { return web_frame_; }
  const std::string& unique_name() const { return unique_name_; }

  void set_provisional_frame_routing_id(int routing_id) {
    provisional_frame_routing_id_ = routing_id;
  }

  int provisional_frame_routing_id() { return provisional_frame_routing_id_; }

  // Returns the widget used for the local frame root.
  RenderWidget* render_widget() { return render_widget_; }

#if defined(USE_AURA)
  void SetMusEmbeddedFrame(
      std::unique_ptr<MusEmbeddedFrame> mus_embedded_frame);
#endif

  // blink::WebRemoteFrameClient implementation:
  void FrameDetached(DetachType type) override;
  void ForwardPostMessage(blink::WebLocalFrame* sourceFrame,
                          blink::WebRemoteFrame* targetFrame,
                          blink::WebSecurityOrigin target,
                          blink::WebDOMMessageEvent event) override;
  void Navigate(const blink::WebURLRequest& request,
                bool should_replace_current_entry) override;
  void FrameRectsChanged(const blink::WebRect& frame_rect) override;
  void UpdateRemoteViewportIntersection(
      const blink::WebRect& viewportIntersection) override;
  void VisibilityChanged(bool visible) override;
  void SetIsInert(bool) override;
  void DidChangeOpener(blink::WebFrame* opener) override;
  void AdvanceFocus(blink::WebFocusType type,
                    blink::WebLocalFrame* source) override;
  void FrameFocused() override;

  // IPC handlers
  void OnDidStartLoading();

 private:
  RenderFrameProxy(int routing_id);

  void Init(blink::WebRemoteFrame* frame,
            RenderViewImpl* render_view,
            RenderWidget* render_widget);

  void ResendFrameRects();

  void MaybeUpdateCompositingHelper();

  void SetChildFrameSurface(const viz::SurfaceInfo& surface_info,
                            const viz::SurfaceSequence& sequence);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC handlers
  void OnDeleteProxy();
  void OnChildFrameProcessGone();
  void OnCompositorFrameSwapped(const IPC::Message& message);
  void OnSetChildFrameSurface(const viz::SurfaceInfo& surface_info,
                              const viz::SurfaceSequence& sequence);
  void OnUpdateOpener(int opener_routing_id);
  void OnViewChanged(const viz::FrameSinkId& frame_sink_id);
  void OnDidStopLoading();
  void OnDidUpdateFramePolicy(const FramePolicy& frame_policy);
  void OnDispatchLoad();
  void OnCollapse(bool collapsed);
  void OnDidUpdateName(const std::string& name, const std::string& unique_name);
  void OnAddContentSecurityPolicies(
      const std::vector<ContentSecurityPolicyHeader>& header);
  void OnResetContentSecurityPolicy();
  void OnEnforceInsecureRequestPolicy(blink::WebInsecureRequestPolicy policy);
  void OnSetFrameOwnerProperties(const FrameOwnerProperties& properties);
  void OnDidUpdateOrigin(const url::Origin& origin,
                         bool is_potentially_trustworthy_unique_origin);
  void OnSetPageFocus(bool is_focused);
  void OnSetFocusedFrame();
  void OnWillEnterFullscreen();
  void OnSetHasReceivedUserGesture();

#if defined(USE_AURA)
  // MusEmbeddedFrameDelegate
  void OnMusEmbeddedFrameSurfaceChanged(
      const viz::SurfaceInfo& surface_info) override;
  void OnMusEmbeddedFrameSinkIdAllocated(
      const viz::FrameSinkId& frame_sink_id) override;
#endif

  // The routing ID by which this RenderFrameProxy is known.
  const int routing_id_;

  // The routing ID of the provisional RenderFrame (if any) that is meant to
  // replace this RenderFrameProxy in the frame tree.
  int provisional_frame_routing_id_;

  // Stores the WebRemoteFrame we are associated with.
  blink::WebRemoteFrame* web_frame_;
  std::string unique_name_;
  std::unique_ptr<ChildFrameCompositingHelper> compositing_helper_;

  RenderViewImpl* render_view_;
  RenderWidget* render_widget_;

  gfx::Rect frame_rect_;
  viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId local_surface_id_;
  viz::LocalSurfaceIdAllocator local_surface_id_allocator_;

  bool enable_surface_synchronization_ = false;

#if defined(USE_AURA)
  std::unique_ptr<MusEmbeddedFrame> mus_embedded_frame_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RenderFrameProxy);
};

}  // namespace

#endif  // CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
