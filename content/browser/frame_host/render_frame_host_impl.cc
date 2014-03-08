// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/metrics/user_metrics_action.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace content {

// The (process id, routing id) pair that identifies one RenderFrame.
typedef std::pair<int32, int32> RenderFrameHostID;
typedef base::hash_map<RenderFrameHostID, RenderFrameHostImpl*>
    RoutingIDFrameMap;
static base::LazyInstance<RoutingIDFrameMap> g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(
    int process_id, int routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  RoutingIDFrameMap::iterator it = frames->find(
      RenderFrameHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

RenderFrameHostImpl::RenderFrameHostImpl(
    RenderViewHostImpl* render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int routing_id,
    bool is_swapped_out)
    : render_view_host_(render_view_host),
      delegate_(delegate),
      cross_process_frame_connector_(NULL),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      routing_id_(routing_id),
      is_swapped_out_(is_swapped_out) {
  frame_tree_->RegisterRenderFrameHost(this);
  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().insert(std::make_pair(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_),
      this));
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_map.Get().erase(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_));
  if (delegate_)
    delegate_->RenderFrameDeleted(this);

  // Notify the FrameTree that this RFH is going away, allowing it to shut down
  // the corresponding RenderViewHost if it is no longer needed.
  frame_tree_->UnregisterRenderFrameHost(this);
}

int RenderFrameHostImpl::GetRoutingID() {
  return routing_id_;
}

SiteInstance* RenderFrameHostImpl::GetSiteInstance() {
  return render_view_host_->GetSiteInstance();
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() {
  // TODO(nasko): This should return its own process, once we have working
  // cross-process navigation for subframes.
  return render_view_host_->GetProcess();
}

RenderFrameHost* RenderFrameHostImpl::GetParent() {
  FrameTreeNode* parent_node = frame_tree_node_->parent();
  if (!parent_node)
    return NULL;
  return parent_node->current_frame_host();
}

bool RenderFrameHostImpl::IsCrossProcessSubframe() {
  FrameTreeNode* parent_node = frame_tree_node_->parent();
  if (!parent_node)
    return false;
  return GetSiteInstance() !=
      parent_node->current_frame_host()->GetSiteInstance();
}

GURL RenderFrameHostImpl::GetLastCommittedURL() {
  return frame_tree_node_->current_url();
}

gfx::NativeView RenderFrameHostImpl::GetNativeView() {
  RenderWidgetHostView* view = render_view_host_->GetView();
  if (!view)
    return NULL;
  return view->GetNativeView();
}

void RenderFrameHostImpl::NotifyContextMenuClosed(
    const CustomContextMenuContext& context) {
  Send(new FrameMsg_ContextMenuClosed(routing_id_, context));
}

void RenderFrameHostImpl::ExecuteCustomContextMenuCommand(
    int action, const CustomContextMenuContext& context) {
  Send(new FrameMsg_CustomContextMenuAction(routing_id_, context, action));
}

void RenderFrameHostImpl::Cut() {
  Send(new InputMsg_Cut(routing_id_));
  RecordAction(base::UserMetricsAction("Cut"));
}

void RenderFrameHostImpl::Copy() {
  Send(new InputMsg_Copy(routing_id_));
  RecordAction(base::UserMetricsAction("Copy"));
}

void RenderFrameHostImpl::Paste() {
  Send(new InputMsg_Paste(routing_id_));
  RecordAction(base::UserMetricsAction("Paste"));
}

void RenderFrameHostImpl::InsertCSS(const std::string& css) {
  Send(new FrameMsg_CSSInsertRequest(routing_id_, css));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() {
  return render_view_host_;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  if (delegate_->OnMessageReceived(this, msg))
    return true;

  if (cross_process_frame_connector_ &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderFrameHostImpl, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameFocused, OnFrameFocused)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailProvisionalLoadWithError,
                        OnDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidRedirectProvisionalLoad,
                        OnDidRedirectProvisionalLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailLoadWithError,
                        OnDidFailLoadWithError)
    IPC_MESSAGE_HANDLER_GENERIC(FrameHostMsg_DidCommitProvisionalLoad,
                                OnNavigate(msg))
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message had a handler, but its de-serialization failed.
    // Kill the renderer.
    RecordAction(base::UserMetricsAction("BadMessageTerminate_RFH"));
    GetProcess()->ReceivedBadMessage();
  }

  return handled;
}

void RenderFrameHostImpl::Init() {
  GetProcess()->ResumeRequestsForView(routing_id_);
}

void RenderFrameHostImpl::OnCreateChildFrame(int new_routing_id,
                                             const std::string& frame_name) {
  RenderFrameHostImpl* new_frame = frame_tree_->AddFrame(
      frame_tree_node_, new_routing_id, frame_name);
  if (delegate_)
    delegate_->RenderFrameCreated(new_frame);
}

void RenderFrameHostImpl::OnDetach() {
  frame_tree_->RemoveFrame(frame_tree_node_);
}

void RenderFrameHostImpl::OnFrameFocused() {
  frame_tree_->SetFocusedFrame(frame_tree_node_);
}

void RenderFrameHostImpl::OnOpenURL(
    const FrameHostMsg_OpenURL_Params& params) {
  GURL validated_url(params.url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator()->RequestOpenURL(
      this, validated_url, params.referrer, params.disposition,
      params.should_replace_current_entry, params.user_gesture);
}

void RenderFrameHostImpl::OnDidStartProvisionalLoadForFrame(
    int parent_routing_id,
    bool is_main_frame,
    const GURL& url) {
  frame_tree_node_->navigator()->DidStartProvisionalLoad(
      this, parent_routing_id, is_main_frame, url);
}

void RenderFrameHostImpl::OnDidFailProvisionalLoadWithError(
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  frame_tree_node_->navigator()->DidFailProvisionalLoadWithError(this, params);
}

void RenderFrameHostImpl::OnDidFailLoadWithError(
    const GURL& url,
    bool is_main_frame,
    int error_code,
    const base::string16& error_description) {
  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator()->DidFailLoadWithError(
      this, validated_url, is_main_frame, error_code,
      error_description);
}

void RenderFrameHostImpl::OnDidRedirectProvisionalLoad(
    int32 page_id,
    const GURL& source_url,
    const GURL& target_url) {
  frame_tree_node_->navigator()->DidRedirectProvisionalLoad(
      this, page_id, source_url, target_url);
}

// Called when the renderer navigates.  For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
//
// Subframes are identified by the page transition type.  For subframes loaded
// as part of a wider page load, the page_id will be the same as for the top
// level frame.  If the user explicitly requests a subframe navigation, we will
// get a new page_id because we need to create a new navigation entry for that
// action.
void RenderFrameHostImpl::OnNavigate(const IPC::Message& msg) {
  // Read the parameters out of the IPC message directly to avoid making another
  // copy when we filter the URLs.
  PickleIterator iter(msg);
  FrameHostMsg_DidCommitProvisionalLoad_Params validated_params;
  if (!IPC::ParamTraits<FrameHostMsg_DidCommitProvisionalLoad_Params>::
      Read(&msg, &iter, &validated_params))
    return;

  // If we're waiting for a cross-site beforeunload ack from this renderer and
  // we receive a Navigate message from the main frame, then the renderer was
  // navigating already and sent it before hearing the ViewMsg_Stop message.
  // We do not want to cancel the pending navigation in this case, since the
  // old page will soon be stopped.  Instead, treat this as a beforeunload ack
  // to allow the pending navigation to continue.
  if (render_view_host_->is_waiting_for_beforeunload_ack_ &&
      render_view_host_->unload_ack_is_for_cross_site_transition_ &&
      PageTransitionIsMainFrame(validated_params.transition)) {
    render_view_host_->OnShouldCloseACK(
        true, render_view_host_->send_should_close_start_time_,
        base::TimeTicks::Now());
    return;
  }

  // If we're waiting for an unload ack from this renderer and we receive a
  // Navigate message, then the renderer was navigating before it received the
  // unload request.  It will either respond to the unload request soon or our
  // timer will expire.  Either way, we should ignore this message, because we
  // have already committed to closing this renderer.
  if (render_view_host_->IsWaitingForUnloadACK())
    return;

  RenderProcessHost* process = GetProcess();

  // Attempts to commit certain off-limits URL should be caught more strictly
  // than our FilterURL checks below.  If a renderer violates this policy, it
  // should be killed.
  if (!CanCommitURL(validated_params.url)) {
    VLOG(1) << "Blocked URL " << validated_params.url.spec();
    validated_params.url = GURL(kAboutBlankURL);
    RecordAction(base::UserMetricsAction("CanCommitURL_BlockedAndKilled"));
    // Kills the process.
    process->ReceivedBadMessage();
  }

  // Now that something has committed, we don't need to track whether the
  // initial page has been accessed.
  render_view_host_->has_accessed_initial_document_ = false;

  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  process->FilterURL(false, &validated_params.url);
  process->FilterURL(true, &validated_params.referrer.url);
  for (std::vector<GURL>::iterator it(validated_params.redirects.begin());
      it != validated_params.redirects.end(); ++it) {
    process->FilterURL(false, &(*it));
  }
  process->FilterURL(true, &validated_params.searchable_form_url);

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!render_view_host_->CanAccessFilesOfPageState(
          validated_params.page_state)) {
    GetProcess()->ReceivedBadMessage();
    return;
  }

  frame_tree_node()->navigator()->DidNavigate(this, validated_params);
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return render_view_host_->GetEnabledBindings();
}

void RenderFrameHostImpl::OnCrossSiteResponse(
    const GlobalRequestID& global_request_id,
    scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    PageTransition page_transition,
    bool should_replace_current_entry) {
  frame_tree_node_->render_manager()->OnCrossSiteResponse(
      this, global_request_id, cross_site_transferring_request.Pass(),
      transfer_url_chain, referrer, page_transition,
      should_replace_current_entry);
}

void RenderFrameHostImpl::SwapOut() {
  if (render_view_host_->IsRenderViewLive()) {
    Send(new FrameMsg_SwapOut(routing_id_));
  } else {
    // Our RenderViewHost doesn't have a live renderer, so just skip the unload
    // event.
    OnSwappedOut(true);
  }
}

void RenderFrameHostImpl::OnDidStartLoading() {
  delegate_->DidStartLoading(this);
}

void RenderFrameHostImpl::OnDidStopLoading() {
  delegate_->DidStopLoading(this);
}

void RenderFrameHostImpl::OnSwapOutACK() {
  OnSwappedOut(false);
}

void RenderFrameHostImpl::OnSwappedOut(bool timed_out) {
  frame_tree_node_->render_manager()->SwappedOutFrame(this);
}

void RenderFrameHostImpl::OnContextMenu(const ContextMenuParams& params) {
  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  RenderProcessHost* process = GetProcess();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  process->FilterURL(true, &validated_params.link_url);
  process->FilterURL(true, &validated_params.src_url);
  process->FilterURL(false, &validated_params.page_url);
  process->FilterURL(true, &validated_params.frame_url);

  delegate_->ShowContextMenu(this, validated_params);
}

void RenderFrameHostImpl::SetPendingShutdown(const base::Closure& on_swap_out) {
  render_view_host_->SetPendingShutdown(on_swap_out);
}

bool RenderFrameHostImpl::CanCommitURL(const GURL& url) {
  // TODO(creis): We should also check for WebUI pages here.  Also, when the
  // out-of-process iframes implementation is ready, we should check for
  // cross-site URLs that are not allowed to commit in this process.

  // Give the client a chance to disallow URLs from committing.
  return GetContentClient()->browser()->CanCommitURL(GetProcess(), url);
}

void RenderFrameHostImpl::Navigate(const FrameMsg_Navigate_Params& params) {
  TRACE_EVENT0("frame_host", "RenderFrameHostImpl::Navigate");
  // Browser plugin guests are not allowed to navigate outside web-safe schemes,
  // so do not grant them the ability to request additional URLs.
  if (!GetProcess()->IsGuest()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
        GetProcess()->GetID(), params.url);
    if (params.url.SchemeIs(kDataScheme) &&
        params.base_url_for_data_url.SchemeIs(kFileScheme)) {
      // If 'data:' is used, and we have a 'file:' base url, grant access to
      // local files.
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
          GetProcess()->GetID(), params.base_url_for_data_url);
    }
  }

  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (render_view_host_->navigations_suspended_) {
    // Shouldn't be possible to have a second navigation while suspended, since
    // navigations will only be suspended during a cross-site request.  If a
    // second navigation occurs, RenderFrameHostManager will cancel this pending
    // RFH and create a new pending RFH.
    DCHECK(!render_view_host_->suspended_nav_params_.get());
    render_view_host_->suspended_nav_params_.reset(
        new FrameMsg_Navigate_Params(params));
  } else {
    // Get back to a clean state, in case we start a new navigation without
    // completing a RVH swap or unload handler.
    render_view_host_->SetState(RenderViewHostImpl::STATE_DEFAULT);

    Send(new FrameMsg_Navigate(routing_id_, params));
  }

  // Force the throbber to start. We do this because Blink's "started
  // loading" message will be received asynchronously from the UI of the
  // browser. But we want to keep the throbber in sync with what's happening
  // in the UI. For example, we want to start throbbing immediately when the
  // user naivgates even if the renderer is delayed. There is also an issue
  // with the throbber starting because the WebUI (which controls whether the
  // favicon is displayed) happens synchronously. If the start loading
  // messages was asynchronous, then the default favicon would flash in.
  //
  // Blink doesn't send throb notifications for JavaScript URLs, so we
  // don't want to either.
  if (!params.url.SchemeIs(kJavaScriptScheme))
    delegate_->DidStartLoading(this);
}

void RenderFrameHostImpl::NavigateToURL(const GURL& url) {
  FrameMsg_Navigate_Params params;
  params.page_id = -1;
  params.pending_history_list_offset = -1;
  params.current_history_list_offset = -1;
  params.current_history_list_length = 0;
  params.url = url;
  params.transition = PAGE_TRANSITION_LINK;
  params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  Navigate(params);
}

}  // namespace content
