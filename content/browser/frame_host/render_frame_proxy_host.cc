// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include "base/lazy_instance.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"

namespace content {

namespace {

// The (process id, routing id) pair that identifies one RenderFrameProxy.
typedef std::pair<int32, int32> RenderFrameProxyHostID;
typedef base::hash_map<RenderFrameProxyHostID, RenderFrameProxyHost*>
    RoutingIDFrameProxyMap;
base::LazyInstance<RoutingIDFrameProxyMap> g_routing_id_frame_proxy_map =
  LAZY_INSTANCE_INITIALIZER;

}

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromID(int process_id,
                                                   int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameProxyMap* frames = g_routing_id_frame_proxy_map.Pointer();
  RoutingIDFrameProxyMap::iterator it = frames->find(
      RenderFrameProxyHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

RenderFrameProxyHost::RenderFrameProxyHost(SiteInstance* site_instance,
                                           FrameTreeNode* frame_tree_node)
    : routing_id_(site_instance->GetProcess()->GetNextRoutingID()),
      site_instance_(site_instance),
      process_(site_instance->GetProcess()),
      frame_tree_node_(frame_tree_node),
      render_frame_proxy_created_(false) {
  GetProcess()->AddRoute(routing_id_, this);
  CHECK(g_routing_id_frame_proxy_map.Get().insert(
      std::make_pair(
          RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_),
          this)).second);

  if (!frame_tree_node_->IsMainFrame() &&
      frame_tree_node_->parent()
              ->render_manager()
              ->current_frame_host()
              ->GetSiteInstance() == site_instance) {
    // The RenderFrameHost navigating cross-process is destroyed and a proxy for
    // it is created in the parent's process. CrossProcessFrameConnector
    // initialization only needs to happen on an initial cross-process
    // navigation, when the RenderFrameHost leaves the same process as its
    // parent. The same CrossProcessFrameConnector is used for subsequent cross-
    // process navigations, but it will be destroyed if the frame is
    // navigated back to the same SiteInstance as its parent.
    cross_process_frame_connector_.reset(new CrossProcessFrameConnector(this));
  }
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (GetProcess()->HasConnection()) {
    // TODO(nasko): For now, don't send this IPC for top-level frames, as
    // the top-level RenderFrame will delete the RenderFrameProxy.
    // This can be removed once we don't have a swapped out state on
    // RenderFrame. See https://crbug.com/357747
    if (!frame_tree_node_->IsMainFrame())
      Send(new FrameMsg_DeleteProxy(routing_id_));
  }

  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_proxy_map.Get().erase(
      RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_));
}

void RenderFrameProxyHost::SetChildRWHView(RenderWidgetHostView* view) {
  cross_process_frame_connector_->set_view(
      static_cast<RenderWidgetHostViewChildFrame*>(view));
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  return frame_tree_node_->frame_tree()->GetRenderViewHost(
      site_instance_.get());
}

void RenderFrameProxyHost::TakeFrameHostOwnership(
    scoped_ptr<RenderFrameHostImpl> render_frame_host) {
  CHECK(render_frame_host_ == nullptr);
  render_frame_host_ = render_frame_host.Pass();
  render_frame_host_->set_render_frame_proxy_host(this);
}

scoped_ptr<RenderFrameHostImpl> RenderFrameProxyHost::PassFrameHostOwnership() {
  render_frame_host_->set_render_frame_proxy_host(NULL);
  return render_frame_host_.Pass();
}

bool RenderFrameProxyHost::Send(IPC::Message *msg) {
  return GetProcess()->Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  if (cross_process_frame_connector_.get() &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxyHost, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RouteMessageEvent, OnRouteMessageEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderFrameProxyHost::InitRenderFrameProxy() {
  DCHECK(!render_frame_proxy_created_);
  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;

  DCHECK(GetProcess()->HasConnection());

  int parent_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    parent_routing_id = frame_tree_node_->parent()
                            ->render_manager()
                            ->GetRoutingIdForSiteInstance(site_instance_.get());
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }

  Send(new FrameMsg_NewFrameProxy(routing_id_,
                                  parent_routing_id,
                                  frame_tree_node_->frame_tree()
                                      ->GetRenderViewHost(site_instance_.get())
                                      ->GetRoutingID(),
                                  frame_tree_node_
                                      ->current_replication_state()));

  render_frame_proxy_created_ = true;
  return true;
}

void RenderFrameProxyHost::DisownOpener() {
  Send(new FrameMsg_DisownOpener(GetRoutingID()));
}

void RenderFrameProxyHost::OnDetach() {
  // This message should only be received for subframes.  Note that we can't
  // restrict it to just the current SiteInstances of the ancestors of this
  // frame, because another frame in the tree may be able to detach this frame
  // by navigating its parent.
  if (frame_tree_node_->IsMainFrame()) {
    bad_message::ReceivedBadMessage(GetProcess(), bad_message::RFPH_DETACH);
    return;
  }
  frame_tree_node_->frame_tree()->RemoveFrame(frame_tree_node_);
}

void RenderFrameProxyHost::OnOpenURL(
    const FrameHostMsg_OpenURL_Params& params) {
  // TODO(creis): Verify that we are in the same BrowsingInstance as the current
  // RenderFrameHost.  See NavigatorImpl::RequestOpenURL.
  frame_tree_node_->current_frame_host()->OpenURL(params, site_instance_.get());
}

void RenderFrameProxyHost::OnRouteMessageEvent(
    const FrameMsg_PostMessage_Params& params) {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();

  // Only deliver the message if the request came from a RenderFrameHost in the
  // same BrowsingInstance or if this WebContents is dedicated to a browser
  // plugin guest.
  //
  // TODO(alexmos, lazyboy):  The check for browser plugin guest currently
  // requires going through the delegate.  It should be refactored and
  // performed here once OOPIF support in <webview> is further along.
  SiteInstance* target_site_instance = target_rfh->GetSiteInstance();
  if (!target_site_instance->IsRelatedSiteInstance(GetSiteInstance()) &&
      !target_rfh->delegate()->ShouldRouteMessageEvent(target_rfh,
                                                       GetSiteInstance()))
    return;

  FrameMsg_PostMessage_Params new_params(params);

  // If there is a source_routing_id, translate it to the routing ID of the
  // equivalent RenderFrameProxyHost in the target process.
  if (new_params.source_routing_id != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* source_rfh = RenderFrameHostImpl::FromID(
        GetProcess()->GetID(), new_params.source_routing_id);
    if (!source_rfh) {
      new_params.source_routing_id = MSG_ROUTING_NONE;
    } else {
      // Ensure that we have a swapped-out RVH and proxy for the source frame.
      // If it doesn't exist, create it on demand and also create its opener
      // chain, since those will also be accessible to the target page.
      //
      // TODO(alexmos): This currently only works for top-level frames and
      // won't create the right proxy if the message source is a subframe on a
      // cross-process tab.  This will be cleaned up as part of moving opener
      // tracking to FrameTreeNode (https://crbug.com/225940). For now, if the
      // message is sent from a subframe on a cross-process tab, set the source
      // routing ID to the main frame of the source tab, which matches legacy
      // postMessage behavior prior to --site-per-process.
      int source_view_routing_id =
          target_rfh->delegate()->EnsureOpenerRenderViewsExist(source_rfh);

      RenderFrameProxyHost* source_proxy_in_target_site_instance =
          source_rfh->frame_tree_node()
              ->render_manager()
              ->GetRenderFrameProxyHost(target_rfh->GetSiteInstance());
      if (source_proxy_in_target_site_instance) {
        new_params.source_routing_id =
            source_proxy_in_target_site_instance->GetRoutingID();
      } else if (source_view_routing_id != MSG_ROUTING_NONE) {
        RenderViewHostImpl* source_rvh = RenderViewHostImpl::FromID(
            target_rfh->GetProcess()->GetID(), source_view_routing_id);
        CHECK(source_rvh);
        new_params.source_routing_id = source_rvh->main_frame_routing_id();
      } else {
        new_params.source_routing_id = MSG_ROUTING_NONE;
      }
    }
  }

  if (!params.message_ports.empty()) {
    // Updating the message port information has to be done in the IO thread;
    // MessagePortMessageFilter::RouteMessageEventWithMessagePorts will send
    // FrameMsg_PostMessageEvent after it's done. Note that a trivial solution
    // would've been to post a task on the IO thread to do the IO-thread-bound
    // work, and make that post a task back to WebContentsImpl in the UI
    // thread. But we cannot do that, since there's nothing to guarantee that
    // WebContentsImpl stays alive during the round trip.
    scoped_refptr<MessagePortMessageFilter> message_port_message_filter(
        static_cast<RenderProcessHostImpl*>(target_rfh->GetProcess())
            ->message_port_message_filter());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MessagePortMessageFilter::RouteMessageEventWithMessagePorts,
                   message_port_message_filter, target_rfh->GetRoutingID(),
                   new_params));
  } else {
    target_rfh->Send(
        new FrameMsg_PostMessageEvent(target_rfh->GetRoutingID(), new_params));
  }
}

}  // namespace content
