// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <stdint.h>
#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

namespace {

// Facilitates lookup of RenderFrameProxy by routing_id.
typedef std::map<int, RenderFrameProxy*> RoutingIDProxyMap;
static base::LazyInstance<RoutingIDProxyMap> g_routing_id_proxy_map =
    LAZY_INSTANCE_INITIALIZER;

// Facilitates lookup of RenderFrameProxy by WebFrame.
typedef std::map<blink::WebFrame*, RenderFrameProxy*> FrameMap;
base::LazyInstance<FrameMap> g_frame_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
RenderFrameProxy* RenderFrameProxy::CreateProxyToReplaceFrame(
    RenderFrameImpl* frame_to_replace,
    int routing_id,
    blink::WebTreeScopeType scope) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);

  scoped_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(routing_id, frame_to_replace->GetRoutingID()));

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame =
      blink::WebRemoteFrame::create(scope, proxy.get());
  proxy->Init(web_frame, frame_to_replace->render_view());
  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    int routing_id,
    int render_view_routing_id,
    int opener_routing_id,
    int parent_routing_id,
    const FrameReplicationState& replicated_state) {
  RenderFrameProxy* parent = nullptr;
  if (parent_routing_id != MSG_ROUTING_NONE) {
    parent = RenderFrameProxy::FromRoutingID(parent_routing_id);
    // It is possible that the parent proxy has been detached in this renderer
    // process, just as the parent's real frame was creating this child frame.
    // In this case, do not create the proxy. See https://crbug.com/568670.
    if (!parent)
      return nullptr;
  }

  scoped_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(routing_id, MSG_ROUTING_NONE));
  RenderViewImpl* render_view = NULL;
  blink::WebRemoteFrame* web_frame = NULL;

  if (!parent) {
    // Create a top level WebRemoteFrame.
    render_view = RenderViewImpl::FromRoutingID(render_view_routing_id);
    web_frame =
        blink::WebRemoteFrame::create(replicated_state.scope, proxy.get());
    render_view->webview()->setMainFrame(web_frame);
  } else {
    // Create a frame under an existing parent. The parent is always expected
    // to be a RenderFrameProxy, because navigations initiated by local frames
    // should not wind up here.

    web_frame = parent->web_frame()->createRemoteChild(
        replicated_state.scope,
        blink::WebString::fromUTF8(replicated_state.name),
        replicated_state.sandbox_flags, proxy.get());
    render_view = parent->render_view();
  }

  blink::WebFrame* opener =
      RenderFrameImpl::ResolveOpener(opener_routing_id, nullptr);
  web_frame->setOpener(opener);

  proxy->Init(web_frame, render_view);

  // Initialize proxy's WebRemoteFrame with the security origin and other
  // replicated information.
  // TODO(dcheng): Calling this when parent_routing_id != MSG_ROUTING_NONE is
  // mostly redundant, since we already pass the name and sandbox flags in
  // createLocalChild(). We should update the Blink interface so it also takes
  // the origin. Then it will be clear that the replication call is only needed
  // for the case of setting up a main frame proxy.
  proxy->SetReplicatedState(replicated_state);

  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::FromRoutingID(int32_t routing_id) {
  RoutingIDProxyMap* proxies = g_routing_id_proxy_map.Pointer();
  RoutingIDProxyMap::iterator it = proxies->find(routing_id);
  return it == proxies->end() ? NULL : it->second;
}

// static
RenderFrameProxy* RenderFrameProxy::FromWebFrame(blink::WebFrame* web_frame) {
  FrameMap::iterator iter = g_frame_map.Get().find(web_frame);
  if (iter != g_frame_map.Get().end()) {
    RenderFrameProxy* proxy = iter->second;
    DCHECK_EQ(web_frame, proxy->web_frame());
    return proxy;
  }
  return NULL;
}

RenderFrameProxy::RenderFrameProxy(int routing_id, int frame_routing_id)
    : routing_id_(routing_id),
      frame_routing_id_(frame_routing_id),
      web_frame_(NULL),
      render_view_(NULL) {
  std::pair<RoutingIDProxyMap::iterator, bool> result =
      g_routing_id_proxy_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";
  RenderThread::Get()->AddRoute(routing_id_, this);
}

RenderFrameProxy::~RenderFrameProxy() {
  // TODO(nasko): Set the render_frame_proxy to null to avoid a double deletion
  // when detaching the main frame. This can be removed once RenderFrameImpl and
  // RenderFrameProxy have been completely decoupled. See
  // https://crbug.com/357747.
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id_);
  if (render_frame)
    render_frame->set_render_frame_proxy(nullptr);

  render_view()->UnregisterRenderFrameProxy(this);

  CHECK(!web_frame_);
  RenderThread::Get()->RemoveRoute(routing_id_);
  g_routing_id_proxy_map.Get().erase(routing_id_);
}

void RenderFrameProxy::Init(blink::WebRemoteFrame* web_frame,
                            RenderViewImpl* render_view) {
  CHECK(web_frame);
  CHECK(render_view);

  web_frame_ = web_frame;
  render_view_ = render_view;

  // TODO(nick): Should all RenderFrameProxies remain observers of their views?
  render_view_->RegisterRenderFrameProxy(this);

  std::pair<FrameMap::iterator, bool> result =
      g_frame_map.Get().insert(std::make_pair(web_frame_, this));
  CHECK(result.second) << "Inserted a duplicate item.";
}

bool RenderFrameProxy::IsMainFrameDetachedFromTree() const {
  return web_frame_->top() == web_frame_ &&
      render_view_->webview()->mainFrame()->isWebLocalFrame();
}

void RenderFrameProxy::DidCommitCompositorFrame() {
  if (compositing_helper_.get())
    compositing_helper_->DidCommitCompositorFrame();
}

void RenderFrameProxy::SetReplicatedState(const FrameReplicationState& state) {
  DCHECK(web_frame_);
  web_frame_->setReplicatedOrigin(state.origin);
  web_frame_->setReplicatedSandboxFlags(state.sandbox_flags);
  web_frame_->setReplicatedName(blink::WebString::fromUTF8(state.name));
  web_frame_->setReplicatedShouldEnforceStrictMixedContentChecking(
      state.should_enforce_strict_mixed_content_checking);
}

// Update the proxy's SecurityContext and FrameOwner with new sandbox flags
// that were set by its parent in another process.
//
// Normally, when a frame's sandbox attribute is changed dynamically, the
// frame's FrameOwner is updated with the new sandbox flags right away, while
// the frame's SecurityContext is updated when the frame is navigated and the
// new sandbox flags take effect.
//
// Currently, there is no use case for a proxy's pending FrameOwner sandbox
// flags, so there's no message sent to proxies when the sandbox attribute is
// first updated.  Instead, the update message is sent and this function is
// called when the new flags take effect, so that the proxy updates its
// SecurityContext. This is needed to ensure that sandbox flags are inherited
// properly if this proxy ever parents a local frame.  The proxy's FrameOwner
// flags are also updated here with the caveat that the FrameOwner won't learn
// about updates to its flags until they take effect.
void RenderFrameProxy::OnDidUpdateSandboxFlags(blink::WebSandboxFlags flags) {
  web_frame_->setReplicatedSandboxFlags(flags);
  web_frame_->setFrameOwnerSandboxFlags(flags);
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_HANDLER(FrameMsg_ChildFrameProcessGone, OnChildFrameProcessGone)
    IPC_MESSAGE_HANDLER_GENERIC(FrameMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(msg))
    IPC_MESSAGE_HANDLER(FrameMsg_SetChildFrameSurface, OnSetChildFrameSurface)
    IPC_MESSAGE_HANDLER(FrameMsg_UpdateOpener, OnUpdateOpener)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateSandboxFlags, OnDidUpdateSandboxFlags)
    IPC_MESSAGE_HANDLER(FrameMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateName, OnDidUpdateName)
    IPC_MESSAGE_HANDLER(FrameMsg_EnforceStrictMixedContentChecking,
                        OnEnforceStrictMixedContentChecking)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateOrigin, OnDidUpdateOrigin)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetPageFocus)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFocusedFrame, OnSetFocusedFrame)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Note: If |handled| is true, |this| may have been deleted.
  return handled;
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  return RenderThread::Get()->Send(message);
}

void RenderFrameProxy::OnDeleteProxy() {
  DCHECK(web_frame_->isWebRemoteFrame());
  web_frame_->detach();
}

void RenderFrameProxy::OnChildFrameProcessGone() {
  if (compositing_helper_.get())
    compositing_helper_->ChildFrameGone();
}

void RenderFrameProxy::OnCompositorFrameSwapped(const IPC::Message& message) {
  // If this WebFrame has already been detached, its parent will be null. This
  // can happen when swapping a WebRemoteFrame with a WebLocalFrame, where this
  // message may arrive after the frame was removed from the frame tree, but
  // before the frame has been destroyed. http://crbug.com/446575.
  if (!web_frame()->parent())
    return;

  FrameMsg_CompositorFrameSwapped::Param param;
  if (!FrameMsg_CompositorFrameSwapped::Read(&message, &param))
    return;

  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  base::get<0>(param).frame.AssignTo(frame.get());

  if (!compositing_helper_.get()) {
    compositing_helper_ =
        ChildFrameCompositingHelper::CreateForRenderFrameProxy(this);
  }
  compositing_helper_->OnCompositorFrameSwapped(
      std::move(frame), base::get<0>(param).producing_route_id,
      base::get<0>(param).output_surface_id,
      base::get<0>(param).producing_host_id,
      base::get<0>(param).shared_memory_handle);
}

void RenderFrameProxy::OnSetChildFrameSurface(
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float scale_factor,
    const cc::SurfaceSequence& sequence) {
  // If this WebFrame has already been detached, its parent will be null. This
  // can happen when swapping a WebRemoteFrame with a WebLocalFrame, where this
  // message may arrive after the frame was removed from the frame tree, but
  // before the frame has been destroyed. http://crbug.com/446575.
  if (!web_frame()->parent())
    return;

  if (!compositing_helper_.get()) {
    compositing_helper_ =
        ChildFrameCompositingHelper::CreateForRenderFrameProxy(this);
  }
  compositing_helper_->OnSetSurface(surface_id, frame_size, scale_factor,
                                    sequence);
}

void RenderFrameProxy::OnUpdateOpener(int opener_routing_id) {
  blink::WebFrame* opener =
      RenderFrameImpl::ResolveOpener(opener_routing_id, nullptr);

  // When there is a RenderFrame for this proxy, tell it to update its opener.
  // TODO(alexmos, nasko): Remove this when we only have WebRemoteFrames.
  if (!SiteIsolationPolicy::IsSwappedOutStateForbidden()) {
    RenderFrameImpl* render_frame =
        RenderFrameImpl::FromRoutingID(frame_routing_id_);
    if (render_frame) {
      render_frame->GetWebFrame()->setOpener(opener);
      return;
    }
  }

  web_frame_->setOpener(opener);
}

void RenderFrameProxy::OnDidStartLoading() {
  if (IsMainFrameDetachedFromTree())
    return;

  web_frame_->didStartLoading();
}

void RenderFrameProxy::OnDidStopLoading() {
  if (IsMainFrameDetachedFromTree())
    return;

  web_frame_->didStopLoading();
}

void RenderFrameProxy::OnDispatchLoad() {
  web_frame_->DispatchLoadEventForFrameOwner();
}

void RenderFrameProxy::OnDidUpdateName(const std::string& name) {
  web_frame_->setReplicatedName(blink::WebString::fromUTF8(name));
}

void RenderFrameProxy::OnEnforceStrictMixedContentChecking(
    bool should_enforce) {
  web_frame_->setReplicatedShouldEnforceStrictMixedContentChecking(
      should_enforce);
}

void RenderFrameProxy::OnDidUpdateOrigin(const url::Origin& origin) {
  web_frame_->setReplicatedOrigin(origin);
}

void RenderFrameProxy::OnSetPageFocus(bool is_focused) {
  render_view_->SetFocus(is_focused);
}

void RenderFrameProxy::OnSetFocusedFrame() {
  // This uses focusDocumentView rather than setFocusedFrame so that blur
  // events are properly dispatched on any currently focused elements.
  render_view_->webview()->focusDocumentView(web_frame_);
}

void RenderFrameProxy::frameDetached(DetachType type) {
  if (type == DetachType::Remove && web_frame_->parent()) {
    web_frame_->parent()->removeChild(web_frame_);

    // Let the browser process know this subframe is removed, so that it is
    // destroyed in its current process.
    Send(new FrameHostMsg_Detach(routing_id_));
  }

  web_frame_->close();

  // Remove the entry in the WebFrame->RenderFrameProxy map, as the |web_frame_|
  // is no longer valid.
  FrameMap::iterator it = g_frame_map.Get().find(web_frame_);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

  web_frame_ = nullptr;

  delete this;
}

void RenderFrameProxy::postMessageEvent(
    blink::WebLocalFrame* source_frame,
    blink::WebRemoteFrame* target_frame,
    blink::WebSecurityOrigin target_origin,
    blink::WebDOMMessageEvent event) {
  DCHECK(!web_frame_ || web_frame_ == target_frame);

  FrameMsg_PostMessage_Params params;
  params.is_data_raw_string = false;
  params.data = event.data().toString();
  params.source_origin = event.origin();
  if (!target_origin.isNull())
    params.target_origin = target_origin.toString();

  params.message_ports =
      WebMessagePortChannelImpl::ExtractMessagePortIDs(event.releaseChannels());

  // Include the routing ID for the source frame (if one exists), which the
  // browser process will translate into the routing ID for the equivalent
  // frame in the target process.
  params.source_routing_id = MSG_ROUTING_NONE;
  if (source_frame) {
    RenderFrameImpl* source_render_frame =
        RenderFrameImpl::FromWebFrame(source_frame);
    if (source_render_frame)
      params.source_routing_id = source_render_frame->GetRoutingID();
  }

  Send(new FrameHostMsg_RouteMessageEvent(routing_id_, params));
}

void RenderFrameProxy::initializeChildFrame(
    const blink::WebRect& frame_rect,
    float scale_factor) {
  Send(new FrameHostMsg_InitializeChildFrame(
      routing_id_, frame_rect, scale_factor));
}

void RenderFrameProxy::navigate(const blink::WebURLRequest& request,
                                bool should_replace_current_entry) {
  FrameHostMsg_OpenURL_Params params;
  params.url = request.url();
  params.referrer = Referrer(
      GURL(request.httpHeaderField(blink::WebString::fromUTF8("Referer"))),
      request.referrerPolicy());
  params.disposition = CURRENT_TAB;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGesture();
  blink::WebUserGestureIndicator::consumeUserGesture();
  Send(new FrameHostMsg_OpenURL(routing_id_, params));
}

void RenderFrameProxy::forwardInputEvent(const blink::WebInputEvent* event) {
  Send(new FrameHostMsg_ForwardInputEvent(routing_id_, event));
}

void RenderFrameProxy::frameRectsChanged(const blink::WebRect& frame_rect) {
  Send(new FrameHostMsg_FrameRectChanged(routing_id_, frame_rect));
}

void RenderFrameProxy::visibilityChanged(bool visible) {
  Send(new FrameHostMsg_VisibilityChanged(routing_id_, visible));
}

void RenderFrameProxy::didChangeOpener(blink::WebFrame* opener) {
  // A proxy shouldn't normally be disowning its opener.  It is possible to get
  // here when a proxy that is being detached clears its opener, in which case
  // there is no need to notify the browser process.
  if (!opener)
    return;

  // Only a LocalFrame (i.e., the caller of window.open) should be able to
  // update another frame's opener.
  DCHECK(opener->isWebLocalFrame());

  int opener_routing_id =
      RenderFrameImpl::FromWebFrame(opener->toWebLocalFrame())->GetRoutingID();
  Send(new FrameHostMsg_DidChangeOpener(routing_id_, opener_routing_id));
}

void RenderFrameProxy::advanceFocus(blink::WebFocusType type,
                                    blink::WebLocalFrame* source) {
  int source_routing_id = RenderFrameImpl::FromWebFrame(source)->GetRoutingID();
  Send(new FrameHostMsg_AdvanceFocus(routing_id_, type, source_routing_id));
}

void RenderFrameProxy::frameFocused() {
  Send(new FrameHostMsg_FrameFocused(routing_id_));
}

}  // namespace
