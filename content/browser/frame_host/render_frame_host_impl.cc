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
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/swapped_out_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

using base::TimeDelta;

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

const std::string& RenderFrameHostImpl::GetFrameName() {
  return frame_tree_node_->frame_name();
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

void RenderFrameHostImpl::DispatchBeforeUnload(bool for_cross_site_transition) {
  // TODO(creis): Support subframes.
  DCHECK(!GetParent());

  if (!render_view_host_->IsRenderViewLive()) {
    // We don't have a live renderer, so just skip running beforeunload.
    render_view_host_->is_waiting_for_beforeunload_ack_ = true;
    render_view_host_->unload_ack_is_for_cross_site_transition_ =
        for_cross_site_transition;
    base::TimeTicks now = base::TimeTicks::Now();
    OnBeforeUnloadACK(true, now, now);
    return;
  }

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if she clicks the tab close button then the browser close
  // button), and we only send the message once.
  if (render_view_host_->is_waiting_for_beforeunload_ack_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    render_view_host_->unload_ack_is_for_cross_site_transition_ =
        render_view_host_->unload_ack_is_for_cross_site_transition_ &&
        for_cross_site_transition;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    render_view_host_->is_waiting_for_beforeunload_ack_ = true;
    render_view_host_->unload_ack_is_for_cross_site_transition_ =
        for_cross_site_transition;
    // Increment the in-flight event count, to ensure that input events won't
    // cancel the timeout timer.
    render_view_host_->increment_in_flight_event_count();
    render_view_host_->StartHangMonitorTimeout(
        TimeDelta::FromMilliseconds(RenderViewHostImpl::kUnloadTimeoutMS));
    send_before_unload_start_time_ = base::TimeTicks::Now();
    Send(new FrameMsg_BeforeUnload(routing_id_));
  }
}

void RenderFrameHostImpl::NotifyContextMenuClosed(
    const CustomContextMenuContext& context) {
  Send(new FrameMsg_ContextMenuClosed(routing_id_, context));
}

void RenderFrameHostImpl::ExecuteCustomContextMenuCommand(
    int action, const CustomContextMenuContext& context) {
  Send(new FrameMsg_CustomContextMenuAction(routing_id_, context, action));
}

void RenderFrameHostImpl::Undo() {
  Send(new InputMsg_Undo(routing_id_));
  RecordAction(base::UserMetricsAction("Undo"));
}

void RenderFrameHostImpl::Redo() {
  Send(new InputMsg_Redo(routing_id_));
  RecordAction(base::UserMetricsAction("Redo"));
}

void RenderFrameHostImpl::Cut() {
  Send(new InputMsg_Cut(routing_id_));
  RecordAction(base::UserMetricsAction("Cut"));
}

void RenderFrameHostImpl::Copy() {
  Send(new InputMsg_Copy(routing_id_));
  RecordAction(base::UserMetricsAction("Copy"));
}

void RenderFrameHostImpl::CopyToFindPboard() {
#if defined(OS_MACOSX)
  // Windows/Linux don't have the concept of a find pasteboard.
  Send(new InputMsg_CopyToFindPboard(routing_id_));
  RecordAction(base::UserMetricsAction("CopyToFindPboard"));
#endif
}

void RenderFrameHostImpl::Paste() {
  Send(new InputMsg_Paste(routing_id_));
  RecordAction(base::UserMetricsAction("Paste"));
}

void RenderFrameHostImpl::PasteAndMatchStyle() {
  Send(new InputMsg_PasteAndMatchStyle(routing_id_));
  RecordAction(base::UserMetricsAction("PasteAndMatchStyle"));
}

void RenderFrameHostImpl::Delete() {
  Send(new InputMsg_Delete(routing_id_));
  RecordAction(base::UserMetricsAction("DeleteSelection"));
}

void RenderFrameHostImpl::SelectAll() {
  Send(new InputMsg_SelectAll(routing_id_));
  RecordAction(base::UserMetricsAction("SelectAll"));
}

void RenderFrameHostImpl::Unselect() {
  Send(new InputMsg_Unselect(routing_id_));
  RecordAction(base::UserMetricsAction("Unselect"));
}

void RenderFrameHostImpl::InsertCSS(const std::string& css) {
  Send(new FrameMsg_CSSInsertRequest(routing_id_, css));
}

void RenderFrameHostImpl::ExecuteJavaScript(
    const base::string16& javascript) {
  Send(new FrameMsg_JavaScriptExecuteRequest(routing_id_,
                                             javascript,
                                             0, false));
}

void RenderFrameHostImpl::ExecuteJavaScript(
     const base::string16& javascript,
     const JavaScriptResultCallback& callback) {
  static int next_id = 1;
  int key = next_id++;
  Send(new FrameMsg_JavaScriptExecuteRequest(routing_id_,
                                             javascript,
                                             key, true));
  javascript_callbacks_.insert(std::make_pair(key, callback));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() {
  return render_view_host_;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  if (IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart) {
    return render_view_host_->input_router()->SendInput(
        make_scoped_ptr(message));
  }

  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  // Filter out most IPC messages if this renderer is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  // TODO(nasko): Only check RenderViewHost state, as this object's own state
  // isn't yet properly updated. Transition this check once the swapped out
  // state is correct in RenderFrameHost itself.
  if (render_view_host_->IsSwappedOut()) {
    if (!SwappedOutMessages::CanHandleWhileSwappedOut(msg)) {
      // If this is a synchronous message and we decided not to handle it,
      // we must send an error reply, or else the renderer will be stuck
      // and won't respond to future requests.
      if (msg.is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
        reply->set_reply_error();
        Send(reply);
      }
      // Don't continue looking for someone to handle it.
      return true;
    }
  }

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  if (cross_process_frame_connector_ &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderFrameHostImpl, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AddMessageToConsole, OnAddMessageToConsole)
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
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeforeUnload_ACK, OnBeforeUnloadACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
    IPC_MESSAGE_HANDLER(FrameHostMsg_JavaScriptExecuteResponse,
                        OnJavaScriptExecuteResponse)
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

void RenderFrameHostImpl::OnAddMessageToConsole(
    int32 level,
    const base::string16& message,
    int32 line_no,
    const base::string16& source_id) {
  if (delegate_->AddMessageToConsole(level, message, line_no, source_id))
    return;

  // Pass through log level only on WebUI pages to limit console spew.
  int32 resolved_level =
      HasWebUIScheme(delegate_->GetMainFrameLastCommittedURL()) ? level : 0;

  if (resolved_level >= ::logging::GetMinLogLevel()) {
    logging::LogMessage("CONSOLE", line_no, resolved_level).stream() << "\"" <<
        message << "\", source: " << source_id << " (" << line_no << ")";
  }
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
    const GURL& url) {
  frame_tree_node_->navigator()->DidStartProvisionalLoad(
      this, parent_routing_id, url);
}

void RenderFrameHostImpl::OnDidFailProvisionalLoadWithError(
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  frame_tree_node_->navigator()->DidFailProvisionalLoadWithError(this, params);
}

void RenderFrameHostImpl::OnDidFailLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator()->DidFailLoadWithError(
      this, validated_url, error_code, error_description);
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
    OnBeforeUnloadACK(true, send_before_unload_start_time_,
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
  // TODO(creis): Move swapped out state to RFH.  Until then, only update it
  // when swapping out the main frame.
  if (!GetParent()) {
    // If this RenderViewHost is not in the default state, it must have already
    // gone through this, therefore just return.
    if (render_view_host_->rvh_state_ != RenderViewHostImpl::STATE_DEFAULT)
      return;

    render_view_host_->SetState(
        RenderViewHostImpl::STATE_WAITING_FOR_UNLOAD_ACK);
    render_view_host_->unload_event_monitor_timeout_->Start(
        base::TimeDelta::FromMilliseconds(
            RenderViewHostImpl::kUnloadTimeoutMS));
  }

  if (render_view_host_->IsRenderViewLive())
    Send(new FrameMsg_SwapOut(routing_id_));

  if (!GetParent())
    delegate_->SwappedOut(this);

  // Allow the navigation to proceed.
  frame_tree_node_->render_manager()->SwappedOut(this);
}

void RenderFrameHostImpl::OnDidStartLoading(bool to_different_document) {
  delegate_->DidStartLoading(this, to_different_document);
}

void RenderFrameHostImpl::OnDidStopLoading() {
  delegate_->DidStopLoading(this);
}

void RenderFrameHostImpl::OnBeforeUnloadACK(
    bool proceed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  // TODO(creis): Support beforeunload on subframes.
  if (GetParent()) {
    NOTREACHED() << "Should only receive BeforeUnload_ACK from the main frame.";
    return;
  }

  render_view_host_->decrement_in_flight_event_count();
  render_view_host_->StopHangMonitorTimeout();
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in OnNavigate, in which case we can ignore
  // this message.
  if (!render_view_host_->is_waiting_for_beforeunload_ack_ ||
      render_view_host_->rvh_state_ != RenderViewHostImpl::STATE_DEFAULT) {
    return;
  }

  render_view_host_->is_waiting_for_beforeunload_ack_ = false;

  base::TimeTicks before_unload_end_time;
  if (!send_before_unload_start_time_.is_null() &&
      !renderer_before_unload_start_time.is_null() &&
      !renderer_before_unload_end_time.is_null()) {
    // When passing TimeTicks across process boundaries, we need to compensate
    // for any skew between the processes. Here we are converting the
    // renderer's notion of before_unload_end_time to TimeTicks in the browser
    // process. See comments in inter_process_time_ticks_converter.h for more.
    InterProcessTimeTicksConverter converter(
        LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
        LocalTimeTicks::FromTimeTicks(base::TimeTicks::Now()),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    LocalTimeTicks browser_before_unload_end_time =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();
  }
  frame_tree_node_->render_manager()->OnBeforeUnloadACK(
      render_view_host_->unload_ack_is_for_cross_site_transition_, proceed,
      before_unload_end_time);

  // If canceled, notify the delegate to cancel its pending navigation entry.
  if (!proceed)
    render_view_host_->GetDelegate()->DidCancelLoading();
}

void RenderFrameHostImpl::OnSwapOutACK() {
  OnSwappedOut(false);
}

void RenderFrameHostImpl::OnSwappedOut(bool timed_out) {
  // For now, we only need to update the RVH state machine for top-level swaps.
  // Subframe swaps (in --site-per-process) can just continue via RFHM.
  if (!GetParent())
    render_view_host_->OnSwappedOut(timed_out);
  else
    frame_tree_node_->render_manager()->SwappedOut(this);
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

void RenderFrameHostImpl::OnJavaScriptExecuteResponse(
    int id, const base::ListValue& result) {
  const base::Value* result_value;
  if (!result.Get(0, &result_value)) {
    // Programming error or rogue renderer.
    NOTREACHED() << "Got bad arguments for OnJavaScriptExecuteResponse";
    return;
  }

  std::map<int, JavaScriptResultCallback>::iterator it =
      javascript_callbacks_.find(id);
  if (it != javascript_callbacks_.end()) {
    it->second.Run(result_value);
    javascript_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received script response for unknown request";
  }
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
    delegate_->DidStartLoading(this, true);
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

void RenderFrameHostImpl::SelectRange(const gfx::Point& start,
                                      const gfx::Point& end) {
  Send(new InputMsg_SelectRange(routing_id_, start, end));
}

void RenderFrameHostImpl::ExtendSelectionAndDelete(size_t before,
                                                   size_t after) {
  Send(new FrameMsg_ExtendSelectionAndDelete(routing_id_, before, after));
}

}  // namespace content
