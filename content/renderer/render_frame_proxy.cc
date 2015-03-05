// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <map>

#include "base/lazy_instance.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
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
    int routing_id) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);

  scoped_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(routing_id, frame_to_replace->GetRoutingID()));

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame = blink::WebRemoteFrame::create(proxy.get());
  proxy->Init(web_frame, frame_to_replace->render_view());
  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    int routing_id,
    int parent_routing_id,
    int render_view_routing_id,
    const FrameReplicationState& replicated_state) {
  scoped_ptr<RenderFrameProxy> proxy(
      new RenderFrameProxy(routing_id, MSG_ROUTING_NONE));
  RenderViewImpl* render_view = NULL;
  blink::WebRemoteFrame* web_frame = NULL;
  if (parent_routing_id == MSG_ROUTING_NONE) {
    // Create a top level frame.
    render_view = RenderViewImpl::FromRoutingID(render_view_routing_id);
    web_frame = blink::WebRemoteFrame::create(proxy.get());
    render_view->webview()->setMainFrame(web_frame);
  } else {
    // Create a frame under an existing parent. The parent is always expected
    // to be a RenderFrameProxy, because navigations initiated by local frames
    // should not wind up here.
    RenderFrameProxy* parent =
        RenderFrameProxy::FromRoutingID(parent_routing_id);
    web_frame = parent->web_frame()->createRemoteChild(
        blink::WebString::fromUTF8(replicated_state.name),
        RenderFrameImpl::ContentToWebSandboxFlags(
            replicated_state.sandbox_flags),
        proxy.get());
    render_view = parent->render_view();
  }

  proxy->Init(web_frame, render_view);

  // Initialize proxy's WebRemoteFrame with the security origin and other
  // replicated information.
  proxy->SetReplicatedState(replicated_state);

  return proxy.release();
}

// static
RenderFrameProxy* RenderFrameProxy::FromRoutingID(int32 routing_id) {
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

  FrameMap::iterator it = g_frame_map.Get().find(web_frame_);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

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
  web_frame_->setReplicatedOrigin(blink::WebSecurityOrigin::createFromString(
      blink::WebString::fromUTF8(state.origin.string())));
  web_frame_->setReplicatedSandboxFlags(
      RenderFrameImpl::ContentToWebSandboxFlags(state.sandbox_flags));
  web_frame_->setReplicatedName(blink::WebString::fromUTF8(state.name));
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
void RenderFrameProxy::OnDidUpdateSandboxFlags(SandboxFlags flags) {
  web_frame_->setReplicatedSandboxFlags(
      RenderFrameImpl::ContentToWebSandboxFlags(flags));
  web_frame_->setFrameOwnerSandboxFlags(
      RenderFrameImpl::ContentToWebSandboxFlags(flags));
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_HANDLER(FrameMsg_ChildFrameProcessGone, OnChildFrameProcessGone)
    IPC_MESSAGE_HANDLER_GENERIC(FrameMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(msg))
    IPC_MESSAGE_HANDLER(FrameMsg_DisownOpener, OnDisownOpener)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateSandboxFlags, OnDidUpdateSandboxFlags)
    IPC_MESSAGE_HANDLER(FrameMsg_DispatchLoad, OnDispatchLoad)
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
  FrameMsg_CompositorFrameSwapped::Param param;
  if (!FrameMsg_CompositorFrameSwapped::Read(&message, &param))
    return;

  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  get<0>(param).frame.AssignTo(frame.get());

  if (!compositing_helper_.get()) {
    compositing_helper_ =
        ChildFrameCompositingHelper::CreateForRenderFrameProxy(this);
    compositing_helper_->EnableCompositing(true);
  }
  compositing_helper_->OnCompositorFrameSwapped(
      frame.Pass(),
      get<0>(param).producing_route_id,
      get<0>(param).output_surface_id,
      get<0>(param).producing_host_id,
      get<0>(param).shared_memory_handle);
}

void RenderFrameProxy::OnDisownOpener() {
  // TODO(creis): We should only see this for main frames for now.  To support
  // disowning the opener on subframes, we will need to move WebContentsImpl's
  // opener_ to FrameTreeNode.
  CHECK(!web_frame_->parent());

  // When there is a RenderFrame for this proxy, tell it to disown its opener.
  // TODO(creis): Remove this when we only have WebRemoteFrames and make sure
  // they know they have an opener.
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id_);
  if (render_frame) {
    if (render_frame->GetWebFrame()->opener())
      render_frame->GetWebFrame()->setOpener(NULL);
    return;
  }

  if (web_frame_->opener())
    web_frame_->setOpener(NULL);
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

void RenderFrameProxy::frameDetached() {
  if (web_frame_->parent())
    web_frame_->parent()->removeChild(web_frame_);

  web_frame_->close();
  delete this;
}

void RenderFrameProxy::postMessageEvent(
    blink::WebLocalFrame* source_frame,
    blink::WebRemoteFrame* target_frame,
    blink::WebSecurityOrigin target_origin,
    blink::WebDOMMessageEvent event) {
  DCHECK(!web_frame_ || web_frame_ == target_frame);

  ViewMsg_PostMessage_Params params;
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
    RenderViewImpl* source_view =
        RenderViewImpl::FromWebView(source_frame->view());
    if (source_view)
      params.source_routing_id = source_view->routing_id();
  }

  Send(new ViewHostMsg_RouteMessageEvent(render_view_->GetRoutingID(), params));
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

}  // namespace
