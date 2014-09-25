// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <map>

#include "base/lazy_instance.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
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
    int render_view_routing_id) {
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
    web_frame = parent->web_frame()->createRemoteChild("", proxy.get());
    render_view = parent->render_view();
  }

  proxy->Init(web_frame, render_view);

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
  render_view()->UnregisterRenderFrameProxy(this);

  FrameMap::iterator it = g_frame_map.Get().find(web_frame_);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

  RenderThread::Get()->RemoveRoute(routing_id_);
  g_routing_id_proxy_map.Get().erase(routing_id_);

  // TODO(nick): Call close unconditionally when web_frame() is always remote.
  if (web_frame()->isWebRemoteFrame())
    web_frame()->close();
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

void RenderFrameProxy::DidCommitCompositorFrame() {
  if (compositing_helper_.get())
    compositing_helper_->DidCommitCompositorFrame();
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_HANDLER(FrameMsg_ChildFrameProcessGone, OnChildFrameProcessGone)
    IPC_MESSAGE_HANDLER_GENERIC(FrameMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(msg))
    IPC_MESSAGE_HANDLER(FrameMsg_DisownOpener, OnDisownOpener)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // If |handled| is true, |this| may have been deleted.
  if (handled)
    return true;

  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id_);
  return render_frame && render_frame->OnMessageReceived(msg);
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  if (!SwappedOutMessages::CanSendWhileSwappedOut(message)) {
    delete message;
    return false;
  }
  message->set_routing_id(routing_id_);
  return RenderThread::Get()->Send(message);
}

void RenderFrameProxy::OnDeleteProxy() {
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id_);

  if (render_frame)
    render_frame->set_render_frame_proxy(NULL);

  delete this;
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
  param.a.frame.AssignTo(frame.get());

  if (!compositing_helper_.get()) {
    compositing_helper_ =
        ChildFrameCompositingHelper::CreateForRenderFrameProxy(this);
    compositing_helper_->EnableCompositing(true);
  }
  compositing_helper_->OnCompositorFrameSwapped(frame.Pass(),
                                                param.a.producing_route_id,
                                                param.a.output_surface_id,
                                                param.a.producing_host_id,
                                                param.a.shared_memory_handle);
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

  blink::WebMessagePortChannelArray channels = event.releaseChannels();
  if (!channels.isEmpty()) {
    std::vector<int> message_port_ids(channels.size());
     // Extract the port IDs from the channel array.
     for (size_t i = 0; i < channels.size(); ++i) {
       WebMessagePortChannelImpl* webchannel =
           static_cast<WebMessagePortChannelImpl*>(channels[i]);
       message_port_ids[i] = webchannel->message_port_id();
       webchannel->QueueMessages();
       DCHECK_NE(message_port_ids[i], MSG_ROUTING_NONE);
     }
     params.message_port_ids = message_port_ids;
  }

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

}  // namespace
