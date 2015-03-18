// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/frame_accessibility.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/geolocation/geolocation_service_context.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/transition_request_manager.h"
#include "content/common/accessibility_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/render_frame_setup.mojom.h"
#include "content/common/swapped_out_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "ui/accessibility/ax_tree.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#endif

#if defined(ENABLE_MEDIA_MOJO_RENDERER)
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif

using base::TimeDelta;

namespace content {

namespace {

// The next value to use for the accessibility reset token.
int g_next_accessibility_reset_token = 1;

// The (process id, routing id) pair that identifies one RenderFrame.
typedef std::pair<int32, int32> RenderFrameHostID;
typedef base::hash_map<RenderFrameHostID, RenderFrameHostImpl*>
    RoutingIDFrameMap;
base::LazyInstance<RoutingIDFrameMap> g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// Translate a WebKit text direction into a base::i18n one.
base::i18n::TextDirection WebTextDirectionToChromeTextDirection(
    blink::WebTextDirection dir) {
  switch (dir) {
    case blink::WebTextDirectionLeftToRight:
      return base::i18n::LEFT_TO_RIGHT;
    case blink::WebTextDirectionRightToLeft:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
      return base::i18n::UNKNOWN_DIRECTION;
  }
}

}  // namespace

const double RenderFrameHostImpl::kLoadingProgressNotStarted = 0.0;
const double RenderFrameHostImpl::kLoadingProgressMinimum = 0.1;
const double RenderFrameHostImpl::kLoadingProgressDone = 1.0;

// static
bool RenderFrameHostImpl::IsRFHStateActive(RenderFrameHostImplState rfh_state) {
  return rfh_state == STATE_DEFAULT;
}

// static
RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(int process_id,
                                                 int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  RoutingIDFrameMap::iterator it = frames->find(
      RenderFrameHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

RenderFrameHostImpl::RenderFrameHostImpl(SiteInstance* site_instance,
                                         RenderViewHostImpl* render_view_host,
                                         RenderFrameHostDelegate* delegate,
                                         RenderWidgetHostDelegate* rwh_delegate,
                                         FrameTree* frame_tree,
                                         FrameTreeNode* frame_tree_node,
                                         int routing_id,
                                         int flags)
    : render_view_host_(render_view_host),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      process_(site_instance->GetProcess()),
      cross_process_frame_connector_(NULL),
      render_frame_proxy_host_(NULL),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      routing_id_(routing_id),
      render_frame_created_(false),
      navigations_suspended_(false),
      has_beforeunload_handlers_(false),
      has_unload_handlers_(false),
      override_sudden_termination_status_(false),
      is_waiting_for_beforeunload_ack_(false),
      unload_ack_is_for_navigation_(false),
      is_loading_(false),
      loading_progress_(kLoadingProgressNotStarted),
      accessibility_reset_token_(0),
      accessibility_reset_count_(0),
      no_create_browser_accessibility_manager_for_testing_(false),
      weak_ptr_factory_(this) {
  bool is_swapped_out = !!(flags & CREATE_RF_SWAPPED_OUT);
  bool hidden = !!(flags & CREATE_RF_HIDDEN);
  frame_tree_->RegisterRenderFrameHost(this);
  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().insert(std::make_pair(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_),
      this));

  if (is_swapped_out) {
    rfh_state_ = STATE_SWAPPED_OUT;
  } else {
    rfh_state_ = STATE_DEFAULT;
    GetSiteInstance()->increment_active_frame_count();
  }

  SetUpMojoIfNeeded();
  swapout_event_monitor_timeout_.reset(new TimeoutMonitor(base::Bind(
      &RenderFrameHostImpl::OnSwappedOut, weak_ptr_factory_.GetWeakPtr())));

  if (flags & CREATE_RF_NEEDS_RENDER_WIDGET_HOST) {
    render_widget_host_.reset(new RenderWidgetHostImpl(
        rwh_delegate, GetProcess(), MSG_ROUTING_NONE, hidden));
    render_widget_host_->set_owned_by_render_frame_host(true);
  }
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_map.Get().erase(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_));

  if (delegate_ && render_frame_created_)
    delegate_->RenderFrameDeleted(this);

  FrameAccessibility::GetInstance()->OnRenderFrameHostDestroyed(this);

  // If this was swapped out, it already decremented the active frame count of
  // the SiteInstance it belongs to.
  if (IsRFHStateActive(rfh_state_))
    GetSiteInstance()->decrement_active_frame_count();

  // Notify the FrameTree that this RFH is going away, allowing it to shut down
  // the corresponding RenderViewHost if it is no longer needed.
  frame_tree_->UnregisterRenderFrameHost(this);

  // NULL out the swapout timer; in crash dumps this member will be null only if
  // the dtor has run.
  swapout_event_monitor_timeout_.reset();

  for (const auto& iter: visual_state_callbacks_) {
    iter.second.Run(false);
  }

  if (render_widget_host_)
    render_widget_host_->Cleanup();
}

int RenderFrameHostImpl::GetRoutingID() {
  return routing_id_;
}

SiteInstanceImpl* RenderFrameHostImpl::GetSiteInstance() {
  return site_instance_.get();
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() {
  return process_;
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

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const base::string16& javascript) {
  Send(new FrameMsg_JavaScriptExecuteRequestForTests(routing_id_,
                                                     javascript,
                                                     0, false));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() {
  return render_view_host_;
}

ServiceRegistry* RenderFrameHostImpl::GetServiceRegistry() {
  return service_registry_.get();
}

blink::WebPageVisibilityState RenderFrameHostImpl::GetVisibilityState() {
  // TODO(mlamouri,kenrb): call GetRenderWidgetHost() directly when it stops
  // returning nullptr in some cases. See https://crbug.com/455245.
  blink::WebPageVisibilityState visibility_state =
      RenderWidgetHostImpl::From(GetView()->GetRenderWidgetHost())->is_hidden()
          ? blink::WebPageVisibilityStateHidden
          : blink::WebPageVisibilityStateVisible;
  GetContentClient()->browser()->OverridePageVisibilityState(this,
                                                             &visibility_state);
  return visibility_state;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  if (IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart) {
    return render_view_host_->input_router()->SendInput(
        make_scoped_ptr(message));
  }

  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  // Filter out most IPC messages if this frame is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  if (is_swapped_out()) {
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

  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (proxy && proxy->cross_process_frame_connector() &&
      proxy->cross_process_frame_connector()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameFocused, OnFrameFocused)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailProvisionalLoadWithError,
                        OnDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailLoadWithError,
                        OnDidFailLoadWithError)
    IPC_MESSAGE_HANDLER_GENERIC(FrameHostMsg_DidCommitProvisionalLoad,
                                OnDidCommitProvisionalLoad(msg))
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidDropNavigation, OnDidDropNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DocumentOnLoadCompleted,
                        OnDocumentOnLoadCompleted)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeforeUnload_ACK, OnBeforeUnloadACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeforeUnloadHandlersPresent,
                        OnBeforeUnloadHandlersPresent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UnloadHandlersPresent,
                        OnUnloadHandlersPresent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
    IPC_MESSAGE_HANDLER(FrameHostMsg_JavaScriptExecuteResponse,
                        OnJavaScriptExecuteResponse)
    IPC_MESSAGE_HANDLER(FrameHostMsg_VisualStateResponse,
                        OnVisualStateResponse)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_RunJavaScriptMessage,
                                    OnRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_RunBeforeUnloadConfirm,
                                    OnRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAccessInitialDocument,
                        OnDidAccessInitialDocument)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidDisownOpener, OnDidDisownOpener)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeName, OnDidChangeName)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAssignPageId, OnDidAssignPageId)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeSandboxFlags,
                        OnDidChangeSandboxFlags)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateTitle, OnUpdateTitle)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateEncoding, OnUpdateEncoding)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeginNavigation,
                        OnBeginNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_TextSurroundingSelectionResponse,
                        OnTextSurroundingSelectionResponse)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_Events, OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_LocationChanges,
                        OnAccessibilityLocationChanges)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_FindInPageResult,
                        OnAccessibilityFindInPageResult)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ToggleFullscreen, OnToggleFullscreen)
    // The following message is synthetic and doesn't come from RenderFrame, but
    // from RenderProcessHost.
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderProcessGone, OnRenderProcessGone)
#if defined(OS_MACOSX) || defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ShowPopup, OnShowPopup)
    IPC_MESSAGE_HANDLER(FrameHostMsg_HidePopup, OnHidePopup)
#endif
  IPC_END_MESSAGE_MAP()

  // No further actions here, since we may have been deleted.
  return handled;
}

void RenderFrameHostImpl::AccessibilitySetFocus(int object_id) {
  Send(new AccessibilityMsg_SetFocus(routing_id_, object_id));
}

void RenderFrameHostImpl::AccessibilityDoDefaultAction(int object_id) {
  Send(new AccessibilityMsg_DoDefaultAction(routing_id_, object_id));
}

void RenderFrameHostImpl::AccessibilityShowMenu(
    const gfx::Point& global_point) {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view)
    view->AccessibilityShowMenu(global_point);
}

void RenderFrameHostImpl::AccessibilityScrollToMakeVisible(
    int acc_obj_id, const gfx::Rect& subfocus) {
  Send(new AccessibilityMsg_ScrollToMakeVisible(
      routing_id_, acc_obj_id, subfocus));
}

void RenderFrameHostImpl::AccessibilityScrollToPoint(
    int acc_obj_id, const gfx::Point& point) {
  Send(new AccessibilityMsg_ScrollToPoint(
      routing_id_, acc_obj_id, point));
}

void RenderFrameHostImpl::AccessibilitySetTextSelection(
    int object_id, int start_offset, int end_offset) {
  Send(new AccessibilityMsg_SetTextSelection(
      routing_id_, object_id, start_offset, end_offset));
}

void RenderFrameHostImpl::AccessibilitySetValue(
    int object_id, const base::string16& value) {
  Send(new AccessibilityMsg_SetValue(routing_id_, object_id, value));
}

bool RenderFrameHostImpl::AccessibilityViewHasFocus() const {
  RenderWidgetHostView* view = render_view_host_->GetView();
  if (view)
    return view->HasFocus();
  return false;
}

gfx::Rect RenderFrameHostImpl::AccessibilityGetViewBounds() const {
  RenderWidgetHostView* view = render_view_host_->GetView();
  if (view)
    return view->GetViewBounds();
  return gfx::Rect();
}

gfx::Point RenderFrameHostImpl::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) const {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view)
    return view->AccessibilityOriginInScreen(bounds);
  return gfx::Point();
}

void RenderFrameHostImpl::AccessibilityHitTest(const gfx::Point& point) {
  Send(new AccessibilityMsg_HitTest(routing_id_, point));
}

void RenderFrameHostImpl::AccessibilitySetAccessibilityFocus(int acc_obj_id) {
  Send(new AccessibilityMsg_SetAccessibilityFocus(routing_id_, acc_obj_id));
}

void RenderFrameHostImpl::AccessibilityFatalError() {
  browser_accessibility_manager_.reset(NULL);
  if (accessibility_reset_token_)
    return;

  accessibility_reset_count_++;
  if (accessibility_reset_count_ >= kMaxAccessibilityResets) {
    Send(new AccessibilityMsg_FatalError(routing_id_));
  } else {
    accessibility_reset_token_ = g_next_accessibility_reset_token++;
    UMA_HISTOGRAM_COUNTS("Accessibility.FrameResetCount", 1);
    Send(new AccessibilityMsg_Reset(routing_id_, accessibility_reset_token_));
  }
}

gfx::AcceleratedWidget
    RenderFrameHostImpl::AccessibilityGetAcceleratedWidget() {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view)
    return view->AccessibilityGetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
    RenderFrameHostImpl::AccessibilityGetNativeViewAccessible() {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessible();
  return NULL;
}

BrowserAccessibilityManager* RenderFrameHostImpl::AccessibilityGetChildFrame(
    int accessibility_node_id) {
  RenderFrameHostImpl* child_frame =
      FrameAccessibility::GetInstance()->GetChild(this, accessibility_node_id);
  if (!child_frame || IsSameSiteInstance(child_frame))
    return nullptr;

  return child_frame->GetOrCreateBrowserAccessibilityManager();
}

void RenderFrameHostImpl::AccessibilityGetAllChildFrames(
    std::vector<BrowserAccessibilityManager*>* child_frames) {
  std::vector<RenderFrameHostImpl*> child_frame_hosts;
  FrameAccessibility::GetInstance()->GetAllChildFrames(
      this, &child_frame_hosts);
  for (size_t i = 0; i < child_frame_hosts.size(); ++i) {
    RenderFrameHostImpl* child_frame_host = child_frame_hosts[i];
    if (!child_frame_host || IsSameSiteInstance(child_frame_host))
      continue;

    BrowserAccessibilityManager* manager =
        child_frame_host->GetOrCreateBrowserAccessibilityManager();
    if (manager)
      child_frames->push_back(manager);
  }
}

BrowserAccessibility* RenderFrameHostImpl::AccessibilityGetParentFrame() {
  RenderFrameHostImpl* parent_frame = NULL;
  int parent_node_id = 0;
  if (!FrameAccessibility::GetInstance()->GetParent(
      this, &parent_frame, &parent_node_id)) {
    return NULL;
  }

  // As a sanity check, make sure the frame we're going to return belongs
  // to the same BrowserContext.
  if (GetSiteInstance()->GetBrowserContext() !=
      parent_frame->GetSiteInstance()->GetBrowserContext()) {
    NOTREACHED();
    return NULL;
  }

  BrowserAccessibilityManager* manager =
      parent_frame->browser_accessibility_manager();
  if (!manager)
    return NULL;

  return manager->GetFromID(parent_node_id);
}

bool RenderFrameHostImpl::CreateRenderFrame(int parent_routing_id,
                                            int proxy_routing_id) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::CreateRenderFrame");
  DCHECK(!IsRenderFrameLive()) << "Creating frame twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;

  DCHECK(GetProcess()->HasConnection());

  FrameMsg_NewFrame_WidgetParams widget_params;
  if (render_widget_host_) {
    widget_params.routing_id = render_widget_host_->GetRoutingID();
    widget_params.surface_id = render_widget_host_->surface_id();
    widget_params.hidden = render_widget_host_->is_hidden();
  } else {
    // MSG_ROUTING_NONE will prevent a new RenderWidget from being created in
    // the renderer process.
    widget_params.routing_id = MSG_ROUTING_NONE;
    widget_params.surface_id = 0;
    widget_params.hidden = true;
  }

  Send(new FrameMsg_NewFrame(routing_id_, parent_routing_id, proxy_routing_id,
                             frame_tree_node()->current_replication_state(),
                             widget_params));

  // The RenderWidgetHost takes ownership of its view. It is tied to the
  // lifetime of the current RenderProcessHost for this RenderFrameHost.
  if (render_widget_host_) {
    RenderWidgetHostView* rwhv =
        new RenderWidgetHostViewChildFrame(render_widget_host_.get());
    rwhv->Hide();
  }

  if (proxy_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromID(
        GetProcess()->GetID(), proxy_routing_id);
    // We have also created a RenderFrameProxy in FrameMsg_NewFrame above, so
    // remember that.
    proxy->set_render_frame_proxy_created(true);
  }

  // The renderer now has a RenderFrame for this RenderFrameHost.  Note that
  // this path is only used for out-of-process iframes.  Main frame RenderFrames
  // are created with their RenderView, and same-site iframes are created at the
  // time of OnCreateChildFrame.
  SetRenderFrameCreated(true);

  return true;
}

bool RenderFrameHostImpl::IsRenderFrameLive() {
  // RenderFrames are created for main frames at the same time as RenderViews,
  // so we rely on IsRenderViewLive.  For subframes, we keep track of each
  // RenderFrame individually with render_frame_created_.
  bool is_live = !GetParent() ?
      render_view_host_->IsRenderViewLive() :
      GetProcess()->HasConnection() && render_frame_created_;

  // Sanity check: the RenderView should always be live if the RenderFrame is.
  DCHECK(!is_live || render_view_host_->IsRenderViewLive());

  return is_live;
}

void RenderFrameHostImpl::SetRenderFrameCreated(bool created) {
  // If the current status is different than the new status, the delegate
  // needs to be notified.
  if (delegate_ && (created != render_frame_created_)) {
    if (created)
      delegate_->RenderFrameCreated(this);
    else
      delegate_->RenderFrameDeleted(this);
  }

  render_frame_created_ = created;
  if (created && render_widget_host_)
    render_widget_host_->InitForFrame();
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
  const bool is_web_ui =
      HasWebUIScheme(delegate_->GetMainFrameLastCommittedURL());
  const int32 resolved_level = is_web_ui ? level : ::logging::LOG_INFO;

  // LogMessages can be persisted so this shouldn't be logged in incognito mode.
  // This rule is not applied to WebUI pages, because source code of WebUI is a
  // part of Chrome source code, and we want to treat messages from WebUI the
  // same way as we treat log messages from native code.
  if (::logging::GetMinLogLevel() <= resolved_level &&
      (is_web_ui ||
       !GetSiteInstance()->GetBrowserContext()->IsOffTheRecord())) {
    logging::LogMessage("CONSOLE", line_no, resolved_level).stream()
        << "\"" << message << "\", source: " << source_id << " (" << line_no
        << ")";
  }
}

void RenderFrameHostImpl::OnCreateChildFrame(int new_routing_id,
                                             const std::string& frame_name,
                                             SandboxFlags sandbox_flags) {
  // It is possible that while a new RenderFrameHost was committed, the
  // RenderFrame corresponding to this host sent an IPC message to create a
  // frame and it is delivered after this host is swapped out.
  // Ignore such messages, as we know this RenderFrameHost is going away.
  if (rfh_state_ != RenderFrameHostImpl::STATE_DEFAULT)
    return;

  RenderFrameHostImpl* new_frame = frame_tree_->AddFrame(
      frame_tree_node_, GetProcess()->GetID(), new_routing_id, frame_name);
  if (!new_frame)
    return;

  // Set sandbox flags for the new frame.  The flags are committed immediately,
  // since they should apply to the initial empty document in the frame.
  new_frame->frame_tree_node()->set_sandbox_flags(sandbox_flags);
  new_frame->frame_tree_node()->CommitPendingSandboxFlags();

  // We know that the RenderFrame has been created in this case, immediately
  // after the CreateChildFrame IPC was sent.
  new_frame->SetRenderFrameCreated(true);
}

void RenderFrameHostImpl::OnDetach() {
  frame_tree_->RemoveFrame(frame_tree_node_);
}

void RenderFrameHostImpl::OnFrameFocused() {
  frame_tree_->SetFocusedFrame(frame_tree_node_);
}

void RenderFrameHostImpl::OnOpenURL(const FrameHostMsg_OpenURL_Params& params) {
  OpenURL(params, GetSiteInstance());
}

void RenderFrameHostImpl::OnDocumentOnLoadCompleted(
    FrameMsg_UILoadMetricsReportType::Value report_type,
    base::TimeTicks ui_timestamp) {
  if (report_type == FrameMsg_UILoadMetricsReportType::REPORT_LINK) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.UI_OnLoadComplete.Link",
                               base::TimeTicks::Now() - ui_timestamp,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(10), 100);
  } else if (report_type == FrameMsg_UILoadMetricsReportType::REPORT_INTENT) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.UI_OnLoadComplete.Intent",
                               base::TimeTicks::Now() - ui_timestamp,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(10), 100);
  }
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->DocumentOnLoadCompleted(this);
}

void RenderFrameHostImpl::OnDidStartProvisionalLoadForFrame(
    const GURL& url,
    bool is_transition_navigation) {
  frame_tree_node_->navigator()->DidStartProvisionalLoad(
      this, url, is_transition_navigation);
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

// Called when the renderer navigates.  For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
//
// Subframes are identified by the page transition type.  For subframes loaded
// as part of a wider page load, the page_id will be the same as for the top
// level frame.  If the user explicitly requests a subframe navigation, we will
// get a new page_id because we need to create a new navigation entry for that
// action.
void RenderFrameHostImpl::OnDidCommitProvisionalLoad(const IPC::Message& msg) {
  // Read the parameters out of the IPC message directly to avoid making another
  // copy when we filter the URLs.
  PickleIterator iter(msg);
  FrameHostMsg_DidCommitProvisionalLoad_Params validated_params;
  if (!IPC::ParamTraits<FrameHostMsg_DidCommitProvisionalLoad_Params>::
      Read(&msg, &iter, &validated_params))
    return;
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnDidCommitProvisionalLoad",
               "url", validated_params.url.possibly_invalid_spec());

  // If we're waiting for a cross-site beforeunload ack from this renderer and
  // we receive a Navigate message from the main frame, then the renderer was
  // navigating already and sent it before hearing the FrameMsg_Stop message.
  // We do not want to cancel the pending navigation in this case, since the
  // old page will soon be stopped.  Instead, treat this as a beforeunload ack
  // to allow the pending navigation to continue.
  if (is_waiting_for_beforeunload_ack_ &&
      unload_ack_is_for_navigation_ &&
      ui::PageTransitionIsMainFrame(validated_params.transition)) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    OnBeforeUnloadACK(true, approx_renderer_start_time, base::TimeTicks::Now());
    return;
  }

  // If we're waiting for an unload ack from this renderer and we receive a
  // Navigate message, then the renderer was navigating before it received the
  // unload request.  It will either respond to the unload request soon or our
  // timer will expire.  Either way, we should ignore this message, because we
  // have already committed to closing this renderer.
  if (IsWaitingForUnloadACK())
    return;

  if (validated_params.report_type ==
      FrameMsg_UILoadMetricsReportType::REPORT_LINK) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Navigation.UI_OnCommitProvisionalLoad.Link",
        base::TimeTicks::Now() - validated_params.ui_timestamp,
        base::TimeDelta::FromMilliseconds(10), base::TimeDelta::FromMinutes(10),
        100);
  } else if (validated_params.report_type ==
             FrameMsg_UILoadMetricsReportType::REPORT_INTENT) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Navigation.UI_OnCommitProvisionalLoad.Intent",
        base::TimeTicks::Now() - validated_params.ui_timestamp,
        base::TimeDelta::FromMilliseconds(10), base::TimeDelta::FromMinutes(10),
        100);
  }

  RenderProcessHost* process = GetProcess();

  // Attempts to commit certain off-limits URL should be caught more strictly
  // than our FilterURL checks below.  If a renderer violates this policy, it
  // should be killed.
  if (!CanCommitURL(validated_params.url)) {
    VLOG(1) << "Blocked URL " << validated_params.url.spec();
    validated_params.url = GURL(url::kAboutBlankURL);
    RecordAction(base::UserMetricsAction("CanCommitURL_BlockedAndKilled"));
    // Kills the process.
    process->ReceivedBadMessage();
  }

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

  accessibility_reset_count_ = 0;
  frame_tree_node()->navigator()->DidNavigate(this, validated_params);
}

void RenderFrameHostImpl::OnDidDropNavigation() {
  // At the end of Navigate(), the delegate's DidStartLoading is called to force
  // the spinner to start, even if the renderer didn't yet begin the load. If it
  // turns out that the renderer dropped the navigation, we need to turn off the
  // spinner.
  delegate_->DidStopLoading(this);
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetRenderWidgetHost() {
  if (render_widget_host_)
    return render_widget_host_.get();

  // TODO(kenrb): When RenderViewHost no longer inherits RenderWidgetHost,
  // we can remove this fallback. Currently it is only used for the main
  // frame.
  if (!GetParent())
    return static_cast<RenderWidgetHostImpl*>(render_view_host_);

  return nullptr;
}

RenderWidgetHostView* RenderFrameHostImpl::GetView() {
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->render_widget_host_)
      return frame->render_widget_host_->GetView();
    frame = static_cast<RenderFrameHostImpl*>(frame->GetParent());
  }

  return render_view_host_->GetView();
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return render_view_host_->GetEnabledBindings();
}

void RenderFrameHostImpl::OnCrossSiteResponse(
    const GlobalRequestID& global_request_id,
    scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry) {
  frame_tree_node_->render_manager()->OnCrossSiteResponse(
      this, global_request_id, cross_site_transferring_request.Pass(),
      transfer_url_chain, referrer, page_transition,
      should_replace_current_entry);
}

void RenderFrameHostImpl::OnDeferredAfterResponseStarted(
    const GlobalRequestID& global_request_id,
    const TransitionLayerData& transition_data) {
  frame_tree_node_->render_manager()->OnDeferredAfterResponseStarted(
      global_request_id, this);

  if (GetParent() || !delegate_->WillHandleDeferAfterResponseStarted())
    frame_tree_node_->render_manager()->ResumeResponseDeferredAtStart();
  else
    delegate_->DidDeferAfterResponseStarted(transition_data);
}

void RenderFrameHostImpl::SwapOut(
    RenderFrameProxyHost* proxy,
    bool is_loading) {
  // The end of this event is in OnSwapOutACK when the RenderFrame has completed
  // the operation and sends back an IPC message.
  // The trace event may not end properly if the ACK times out.  We expect this
  // to be fixed when RenderViewHostImpl::OnSwapOut moves to RenderFrameHost.
  TRACE_EVENT_ASYNC_BEGIN0("navigation", "RenderFrameHostImpl::SwapOut", this);

  // If this RenderFrameHost is not in the default state, it must have already
  // gone through this, therefore just return.
  if (rfh_state_ != RenderFrameHostImpl::STATE_DEFAULT) {
    NOTREACHED() << "RFH should be in default state when calling SwapOut.";
    return;
  }

  SetState(RenderFrameHostImpl::STATE_PENDING_SWAP_OUT);
  swapout_event_monitor_timeout_->Start(
      base::TimeDelta::FromMilliseconds(RenderViewHostImpl::kUnloadTimeoutMS));

  // There may be no proxy if there are no active views in the process.
  int proxy_routing_id = MSG_ROUTING_NONE;
  FrameReplicationState replication_state;
  if (proxy) {
    set_render_frame_proxy_host(proxy);
    proxy_routing_id = proxy->GetRoutingID();
    replication_state = proxy->frame_tree_node()->current_replication_state();
  }

  if (IsRenderFrameLive()) {
    Send(new FrameMsg_SwapOut(routing_id_, proxy_routing_id, is_loading,
                              replication_state));
  }

  if (!GetParent())
    delegate_->SwappedOut(this);
}

void RenderFrameHostImpl::OnBeforeUnloadACK(
    bool proceed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  TRACE_EVENT_ASYNC_END0(
      "navigation", "RenderFrameHostImpl::BeforeUnload", this);
  DCHECK(!GetParent());
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in OnDidCommitProvisionalLoad, in which case we
  // can ignore this message.
  // However renderer might also be swapped out but we still want to proceed
  // with navigation, otherwise it would block future navigations. This can
  // happen when pending cross-site navigation is canceled by a second one just
  // before OnDidCommitProvisionalLoad while current RVH is waiting for commit
  // but second navigation is started from the beginning.
  if (!is_waiting_for_beforeunload_ack_) {
    return;
  }
  DCHECK(!send_before_unload_start_time_.is_null());

  // Sets a default value for before_unload_end_time so that the browser
  // survives a hacked renderer.
  base::TimeTicks before_unload_end_time = renderer_before_unload_end_time;
  if (!renderer_before_unload_start_time.is_null() &&
      !renderer_before_unload_end_time.is_null()) {
    // When passing TimeTicks across process boundaries, we need to compensate
    // for any skew between the processes. Here we are converting the
    // renderer's notion of before_unload_end_time to TimeTicks in the browser
    // process. See comments in inter_process_time_ticks_converter.h for more.
    base::TimeTicks receive_before_unload_ack_time = base::TimeTicks::Now();
    InterProcessTimeTicksConverter converter(
        LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
        LocalTimeTicks::FromTimeTicks(receive_before_unload_ack_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
        RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    LocalTimeTicks browser_before_unload_end_time =
        converter.ToLocalTimeTicks(
            RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
    before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();

    // Collect UMA on the inter-process skew.
    bool is_skew_additive = false;
    if (converter.IsSkewAdditiveForMetrics()) {
      is_skew_additive = true;
      base::TimeDelta skew = converter.GetSkewForMetrics();
      if (skew >= base::TimeDelta()) {
        UMA_HISTOGRAM_TIMES(
            "InterProcessTimeTicks.BrowserBehind_RendererToBrowser", skew);
      } else {
        UMA_HISTOGRAM_TIMES(
            "InterProcessTimeTicks.BrowserAhead_RendererToBrowser", -skew);
      }
    }
    UMA_HISTOGRAM_BOOLEAN(
        "InterProcessTimeTicks.IsSkewAdditive_RendererToBrowser",
        is_skew_additive);

    base::TimeDelta on_before_unload_overhead_time =
        (receive_before_unload_ack_time - send_before_unload_start_time_) -
        (renderer_before_unload_end_time - renderer_before_unload_start_time);
    UMA_HISTOGRAM_TIMES("Navigation.OnBeforeUnloadOverheadTime",
                        on_before_unload_overhead_time);

    frame_tree_node_->navigator()->LogBeforeUnloadTime(
        renderer_before_unload_start_time, renderer_before_unload_end_time);
  }
  // Resets beforeunload waiting state.
  is_waiting_for_beforeunload_ack_ = false;
  render_view_host_->decrement_in_flight_event_count();
  render_view_host_->StopHangMonitorTimeout();
  send_before_unload_start_time_ = base::TimeTicks();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    // TODO(clamy): see if before_unload_end_time should be transmitted to the
    // Navigator.
    frame_tree_node_->navigator()->OnBeforeUnloadACK(
        frame_tree_node_, proceed);
  } else {
    frame_tree_node_->render_manager()->OnBeforeUnloadACK(
        unload_ack_is_for_navigation_, proceed,
        before_unload_end_time);
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  if (!proceed)
    render_view_host_->GetDelegate()->DidCancelLoading();
}

bool RenderFrameHostImpl::IsWaitingForBeforeUnloadACK() const {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation)) {
    return is_waiting_for_beforeunload_ack_;
  }
  return frame_tree_node_->navigator()->IsWaitingForBeforeUnloadACK(
      frame_tree_node_);
}

bool RenderFrameHostImpl::IsWaitingForUnloadACK() const {
  return render_view_host_->is_waiting_for_close_ack_ ||
      rfh_state_ == STATE_PENDING_SWAP_OUT;
}

bool RenderFrameHostImpl::SuddenTerminationAllowed() const {
  return override_sudden_termination_status_ ||
      (!has_beforeunload_handlers_ && !has_unload_handlers_);
}

void RenderFrameHostImpl::OnSwapOutACK() {
  OnSwappedOut();
}

void RenderFrameHostImpl::OnRenderProcessGone(int status, int exit_code) {
  if (frame_tree_node_->IsMainFrame()) {
    // Keep the termination status so we can get at it later when we
    // need to know why it died.
    render_view_host_->render_view_termination_status_ =
        static_cast<base::TerminationStatus>(status);
  }

  // Reset frame tree state associated with this process.  This must happen
  // before RenderViewTerminated because observers expect the subframes of any
  // affected frames to be cleared first.
  // Note: When a RenderFrameHost is swapped out there is a different one
  // which is the current host. In this case, the FrameTreeNode state must
  // not be reset.
  if (!is_swapped_out())
    frame_tree_node_->ResetForNewProcess();

  // Reset state for the current RenderFrameHost once the FrameTreeNode has been
  // reset.
  SetRenderFrameCreated(false);
  InvalidateMojoConnection();

  if (frame_tree_node_->IsMainFrame()) {
    // RenderViewHost/RenderWidgetHost needs to reset some stuff.
    render_view_host_->RendererExited(
        render_view_host_->render_view_termination_status_, exit_code);

    render_view_host_->delegate_->RenderViewTerminated(
        render_view_host_, static_cast<base::TerminationStatus>(status),
        exit_code);
  }
}

void RenderFrameHostImpl::OnSwappedOut() {
  // Ignore spurious swap out ack.
  if (rfh_state_ != STATE_PENDING_SWAP_OUT)
    return;

  TRACE_EVENT_ASYNC_END0("navigation", "RenderFrameHostImpl::SwapOut", this);
  swapout_event_monitor_timeout_->Stop();

  if (frame_tree_node_->render_manager()->DeleteFromPendingList(this)) {
    // We are now deleted.
    return;
  }

  // If this RFH wasn't pending deletion, then it is now swapped out.
  SetState(RenderFrameHostImpl::STATE_SWAPPED_OUT);
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

void RenderFrameHostImpl::OnVisualStateResponse(uint64 id) {
  auto it = visual_state_callbacks_.find(id);
  if (it != visual_state_callbacks_.end()) {
    it->second.Run(true);
    visual_state_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received script response for unknown request";
  }
}

void RenderFrameHostImpl::OnRunJavaScriptMessage(
    const base::string16& message,
    const base::string16& default_prompt,
    const GURL& frame_url,
    JavaScriptMessageType type,
    IPC::Message* reply_msg) {
  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  render_view_host_->StopHangMonitorTimeout();
  delegate_->RunJavaScriptMessage(this, message, default_prompt,
                                  frame_url, type, reply_msg);
}

void RenderFrameHostImpl::OnRunBeforeUnloadConfirm(
    const GURL& frame_url,
    const base::string16& message,
    bool is_reload,
    IPC::Message* reply_msg) {
  // While a JS beforeunload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  render_view_host_->StopHangMonitorTimeout();
  delegate_->RunBeforeUnloadConfirm(this, message, is_reload, reply_msg);
}

void RenderFrameHostImpl::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    size_t start_offset,
    size_t end_offset) {
  render_view_host_->OnTextSurroundingSelectionResponse(
      content, start_offset, end_offset);
}

void RenderFrameHostImpl::OnDidAccessInitialDocument() {
  delegate_->DidAccessInitialDocument();
}

void RenderFrameHostImpl::OnDidDisownOpener() {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->DidDisownOpener(this);
}

void RenderFrameHostImpl::OnDidChangeName(const std::string& name) {
  frame_tree_node()->SetFrameName(name);
  delegate_->DidChangeName(this, name);
}

void RenderFrameHostImpl::OnDidAssignPageId(int32 page_id) {
  // Update the RVH's current page ID so that future IPCs from the renderer
  // correspond to the new page.
  render_view_host_->page_id_ = page_id;
}

void RenderFrameHostImpl::OnDidChangeSandboxFlags(int32 frame_routing_id,
                                                  SandboxFlags flags) {
  FrameTree* frame_tree = frame_tree_node()->frame_tree();
  FrameTreeNode* child =
      frame_tree->FindByRoutingID(GetProcess()->GetID(), frame_routing_id);
  if (!child)
    return;

  // Ensure that a frame can only update sandbox flags for its immediate
  // children.  If this is not the case, the renderer is considered malicious
  // and is killed.
  if (child->parent() != frame_tree_node()) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_RFH"));
    GetProcess()->ReceivedBadMessage();
    return;
  }

  child->set_sandbox_flags(flags);

  // Notify the RenderFrame if it lives in a different process from its
  // parent. The frame's proxies in other processes also need to learn about
  // the updated sandbox flags, but these notifications are sent later in
  // RenderFrameHostManager::CommitPendingSandboxFlags(), when the frame
  // navigates and the new sandbox flags take effect.
  RenderFrameHost* child_rfh = child->current_frame_host();
  if (child_rfh->GetSiteInstance() != GetSiteInstance()) {
    child_rfh->Send(
        new FrameMsg_DidUpdateSandboxFlags(child_rfh->GetRoutingID(), flags));
  }
}

void RenderFrameHostImpl::OnUpdateTitle(
    const base::string16& title,
    blink::WebTextDirection title_direction) {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  if (title.length() > kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }

  delegate_->UpdateTitle(this, render_view_host_->page_id_, title,
                         WebTextDirectionToChromeTextDirection(
                             title_direction));
}

void RenderFrameHostImpl::OnUpdateEncoding(const std::string& encoding_name) {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->UpdateEncoding(this, encoding_name);
}

void RenderFrameHostImpl::OnBeginNavigation(
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    scoped_refptr<ResourceRequestBody> body) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  frame_tree_node()->navigator()->OnBeginNavigation(
      frame_tree_node(), common_params, begin_params, body);
}

void RenderFrameHostImpl::OnDispatchLoad() {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess));
  // Only frames with an out-of-process parent frame should be sending this
  // message.
  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    GetProcess()->ReceivedBadMessage();
    return;
  }

  proxy->Send(new FrameMsg_DispatchLoad(proxy->GetRoutingID()));
}

void RenderFrameHostImpl::OnAccessibilityEvents(
    const std::vector<AccessibilityHostMsg_EventParams>& params,
    int reset_token) {
  // Don't process this IPC if either we're waiting on a reset and this
  // IPC doesn't have the matching token ID, or if we're not waiting on a
  // reset but this message includes a reset token.
  if (accessibility_reset_token_ != reset_token) {
    Send(new AccessibilityMsg_Events_ACK(routing_id_));
    return;
  }
  accessibility_reset_token_ = 0;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());

  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if ((accessibility_mode != AccessibilityModeOff) && view &&
      RenderFrameHostImpl::IsRFHStateActive(rfh_state())) {
    if (accessibility_mode & AccessibilityModeFlagPlatform) {
      GetOrCreateBrowserAccessibilityManager();
      if (browser_accessibility_manager_)
        browser_accessibility_manager_->OnAccessibilityEvents(params);
    }

    if (browser_accessibility_manager_) {
      // Get the frame routing ids from out-of-process iframes and
      // browser plugin instance ids from guests and update the mappings in
      // FrameAccessibility.
      for (size_t i = 0; i < params.size(); ++i) {
        const AccessibilityHostMsg_EventParams& param = params[i];
        UpdateCrossProcessIframeAccessibility(
            param.node_to_frame_routing_id_map);
        UpdateGuestFrameAccessibility(
            param.node_to_browser_plugin_instance_id_map);
      }
    }

    // Send the updates to the automation extension API.
    std::vector<AXEventNotificationDetails> details;
    details.reserve(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
      const AccessibilityHostMsg_EventParams& param = params[i];
      AXEventNotificationDetails detail(param.update.node_id_to_clear,
                                        param.update.nodes,
                                        param.event_type,
                                        param.id,
                                        GetProcess()->GetID(),
                                        routing_id_);
      details.push_back(detail);
    }

    delegate_->AccessibilityEventReceived(details);
  }

  // Always send an ACK or the renderer can be in a bad state.
  Send(new AccessibilityMsg_Events_ACK(routing_id_));

  // The rest of this code is just for testing; bail out if we're not
  // in that mode.
  if (accessibility_testing_callback_.is_null())
    return;

  for (size_t i = 0; i < params.size(); i++) {
    const AccessibilityHostMsg_EventParams& param = params[i];
    if (static_cast<int>(param.event_type) < 0)
      continue;

    if (!ax_tree_for_testing_) {
      if (browser_accessibility_manager_) {
        ax_tree_for_testing_.reset(new ui::AXTree(
            browser_accessibility_manager_->SnapshotAXTreeForTesting()));
      } else {
        ax_tree_for_testing_.reset(new ui::AXTree());
        CHECK(ax_tree_for_testing_->Unserialize(param.update))
            << ax_tree_for_testing_->error();
      }
    } else {
      CHECK(ax_tree_for_testing_->Unserialize(param.update))
          << ax_tree_for_testing_->error();
    }
    accessibility_testing_callback_.Run(param.event_type, param.id);
  }
}

void RenderFrameHostImpl::OnAccessibilityLocationChanges(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  if (accessibility_reset_token_)
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view && RenderFrameHostImpl::IsRFHStateActive(rfh_state())) {
    AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
    if (accessibility_mode & AccessibilityModeFlagPlatform) {
      BrowserAccessibilityManager* manager =
          GetOrCreateBrowserAccessibilityManager();
      if (manager)
        manager->OnLocationChanges(params);
    }
    // TODO(aboxhall): send location change events to web contents observers too
  }
}

void RenderFrameHostImpl::OnAccessibilityFindInPageResult(
    const AccessibilityHostMsg_FindInPageResultParams& params) {
  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode & AccessibilityModeFlagPlatform) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager) {
      manager->OnFindInPageResult(
          params.request_id, params.match_index, params.start_id,
          params.start_offset, params.end_id, params.end_offset);
    }
  }
}

void RenderFrameHostImpl::OnToggleFullscreen(bool enter_fullscreen) {
  if (enter_fullscreen)
    delegate_->EnterFullscreenMode(GetLastCommittedURL().GetOrigin());
  else
    delegate_->ExitFullscreenMode();

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  render_view_host_->WasResized();
}

void RenderFrameHostImpl::OnBeforeUnloadHandlersPresent(bool present) {
  has_beforeunload_handlers_ = present;
}

void RenderFrameHostImpl::OnUnloadHandlersPresent(bool present) {
  has_unload_handlers_ = present;
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void RenderFrameHostImpl::OnShowPopup(
    const FrameHostMsg_ShowPopup_Params& params) {
  RenderViewHostDelegateView* view =
      render_view_host_->delegate_->GetDelegateView();
  if (view) {
    view->ShowPopupMenu(this,
                        params.bounds,
                        params.item_height,
                        params.item_font_size,
                        params.selected_item,
                        params.popup_items,
                        params.right_aligned,
                        params.allow_multiple_selection);
  }
}

void RenderFrameHostImpl::OnHidePopup() {
  RenderViewHostDelegateView* view =
      render_view_host_->delegate_->GetDelegateView();
  if (view)
    view->HidePopupMenu();
}
#endif

#if defined(ENABLE_MEDIA_MOJO_RENDERER)
static void CreateMediaRendererService(
    mojo::InterfaceRequest<mojo::MediaRenderer> request) {
  media::MojoRendererService* service = new media::MojoRendererService();
  mojo::BindToRequest(service, &request);
}
#endif

void RenderFrameHostImpl::RegisterMojoServices() {
  GeolocationServiceContext* geolocation_service_context =
      delegate_ ? delegate_->GetGeolocationServiceContext() : NULL;
  if (geolocation_service_context) {
    // TODO(creis): Bind process ID here so that GeolocationServiceImpl
    // can perform permissions checks once site isolation is complete.
    // crbug.com/426384
    GetServiceRegistry()->AddService<GeolocationService>(
        base::Bind(&GeolocationServiceContext::CreateService,
                   base::Unretained(geolocation_service_context),
                   base::Bind(&RenderFrameHostImpl::DidUseGeolocationPermission,
                              base::Unretained(this))));
  }

  if (!permission_service_context_)
    permission_service_context_.reset(new PermissionServiceContext(this));

  GetServiceRegistry()->AddService<PermissionService>(
      base::Bind(&PermissionServiceContext::CreateService,
                 base::Unretained(permission_service_context_.get())));

  GetServiceRegistry()->AddService<presentation::PresentationService>(
      base::Bind(&PresentationServiceImpl::CreateMojoService,
                 base::Unretained(this)));

#if defined(ENABLE_MEDIA_MOJO_RENDERER)
  GetServiceRegistry()->AddService<mojo::MediaRenderer>(
      base::Bind(&CreateMediaRendererService));
#endif
}

void RenderFrameHostImpl::SetState(RenderFrameHostImplState rfh_state) {
  // Only main frames should be swapped out and retained inside a proxy host.
  if (rfh_state == STATE_SWAPPED_OUT)
    CHECK(!GetParent());

  // We update the number of RenderFrameHosts in a SiteInstance when the swapped
  // out status of a RenderFrameHost gets flipped to/from active.
  if (!IsRFHStateActive(rfh_state_) && IsRFHStateActive(rfh_state))
    GetSiteInstance()->increment_active_frame_count();
  else if (IsRFHStateActive(rfh_state_) && !IsRFHStateActive(rfh_state))
    GetSiteInstance()->decrement_active_frame_count();

  // The active and swapped out state of the RVH is determined by its main
  // frame, since subframes should have their own widgets.
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host_->set_is_active(IsRFHStateActive(rfh_state));
    render_view_host_->set_is_swapped_out(rfh_state == STATE_SWAPPED_OUT);
  }

  // Whenever we change the RFH state to and from active or swapped out state,
  // we should not be waiting for beforeunload or close acks.  We clear them
  // here to be safe, since they can cause navigations to be ignored in
  // OnDidCommitProvisionalLoad.
  // TODO(creis): Move is_waiting_for_beforeunload_ack_ into the state machine.
  if (rfh_state == STATE_DEFAULT ||
      rfh_state == STATE_SWAPPED_OUT ||
      rfh_state_ == STATE_DEFAULT ||
      rfh_state_ == STATE_SWAPPED_OUT) {
    if (is_waiting_for_beforeunload_ack_) {
      is_waiting_for_beforeunload_ack_ = false;
      render_view_host_->decrement_in_flight_event_count();
      render_view_host_->StopHangMonitorTimeout();
    }
    send_before_unload_start_time_ = base::TimeTicks();
    render_view_host_->is_waiting_for_close_ack_ = false;
  }
  rfh_state_ = rfh_state;
}

bool RenderFrameHostImpl::CanCommitURL(const GURL& url) {
  // TODO(creis): We should also check for WebUI pages here.  Also, when the
  // out-of-process iframes implementation is ready, we should check for
  // cross-site URLs that are not allowed to commit in this process.

  // Give the client a chance to disallow URLs from committing.
  return GetContentClient()->browser()->CanCommitURL(GetProcess(), url);
}

void RenderFrameHostImpl::Navigate(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const CommitNavigationParams& commit_params,
    const HistoryNavigationParams& history_params) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::Navigate");
  // Browser plugin guests are not allowed to navigate outside web-safe schemes,
  // so do not grant them the ability to request additional URLs.
  if (!GetProcess()->IsIsolatedGuest()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
        GetProcess()->GetID(), common_params.url);
    if (common_params.url.SchemeIs(url::kDataScheme) &&
        common_params.base_url_for_data_url.SchemeIs(url::kFileScheme)) {
      // If 'data:' is used, and we have a 'file:' base url, grant access to
      // local files.
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
          GetProcess()->GetID(), common_params.base_url_for_data_url);
    }
  }

  // We may be returning to an existing NavigationEntry that had been granted
  // file access.  If this is a different process, we will need to grant the
  // access again.  The files listed in the page state are validated when they
  // are received from the renderer to prevent abuse.
  if (history_params.page_state.IsValid()) {
    render_view_host_->GrantFileAccessFromPageState(history_params.page_state);
  }

  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (navigations_suspended_) {
    // Shouldn't be possible to have a second navigation while suspended, since
    // navigations will only be suspended during a cross-site request.  If a
    // second navigation occurs, RenderFrameHostManager will cancel this pending
    // RFH and create a new pending RFH.
    DCHECK(!suspended_nav_params_.get());
    suspended_nav_params_.reset(new NavigationParams(
        common_params, start_params, commit_params, history_params));
  } else {
    // Get back to a clean state, in case we start a new navigation without
    // completing a RFH swap or unload handler.
    SetState(RenderFrameHostImpl::STATE_DEFAULT);

    Send(new FrameMsg_Navigate(routing_id_, common_params, start_params,
                               commit_params, history_params));
  }

  // Force the throbber to start. We do this because Blink's "started
  // loading" message will be received asynchronously from the UI of the
  // browser. But we want to keep the throbber in sync with what's happening
  // in the UI. For example, we want to start throbbing immediately when the
  // user navigates even if the renderer is delayed. There is also an issue
  // with the throbber starting because the WebUI (which controls whether the
  // favicon is displayed) happens synchronously. If the start loading
  // messages was asynchronous, then the default favicon would flash in.
  //
  // Blink doesn't send throb notifications for JavaScript URLs, so we
  // don't want to either.
  if (!common_params.url.SchemeIs(url::kJavaScriptScheme))
    delegate_->DidStartLoading(this, true);
}

void RenderFrameHostImpl::NavigateToURL(const GURL& url) {
  CommonNavigationParams common_params(
      url, Referrer(), ui::PAGE_TRANSITION_LINK, FrameMsg_Navigate_Type::NORMAL,
      true, base::TimeTicks::Now(), FrameMsg_UILoadMetricsReportType::NO_REPORT,
      GURL(), GURL());
  Navigate(common_params, StartNavigationParams(), CommitNavigationParams(),
           HistoryNavigationParams());
}

void RenderFrameHostImpl::OpenURL(const FrameHostMsg_OpenURL_Params& params,
                                  SiteInstance* source_site_instance) {
  GURL validated_url(params.url);
  GetProcess()->FilterURL(false, &validated_url);

  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OpenURL", "url",
               validated_url.possibly_invalid_spec());
  frame_tree_node_->navigator()->RequestOpenURL(
      this, validated_url, source_site_instance, params.referrer,
      params.disposition, params.should_replace_current_entry,
      params.user_gesture);
}

void RenderFrameHostImpl::Stop() {
  Send(new FrameMsg_Stop(routing_id_));
}

void RenderFrameHostImpl::DispatchBeforeUnload(bool for_navigation) {
  // TODO(creis): Support beforeunload on subframes.  For now just pretend that
  // the handler ran and allowed the navigation to proceed.
  if (GetParent() || !IsRenderFrameLive()) {
    // We don't have a live renderer, so just skip running beforeunload.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation)) {
      frame_tree_node_->navigator()->OnBeforeUnloadACK(
          frame_tree_node_, true);
    } else {
      frame_tree_node_->render_manager()->OnBeforeUnloadACK(
          for_navigation, true, base::TimeTicks::Now());
    }
    return;
  }
  TRACE_EVENT_ASYNC_BEGIN0(
      "navigation", "RenderFrameHostImpl::BeforeUnload", this);

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if she clicks the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_ack_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_navigation_ =
        unload_ack_is_for_navigation_ && for_navigation;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_ack_ = true;
    unload_ack_is_for_navigation_ = for_navigation;
    // Increment the in-flight event count, to ensure that input events won't
    // cancel the timeout timer.
    render_view_host_->increment_in_flight_event_count();
    render_view_host_->StartHangMonitorTimeout(
        TimeDelta::FromMilliseconds(RenderViewHostImpl::kUnloadTimeoutMS));
    send_before_unload_start_time_ = base::TimeTicks::Now();
    Send(new FrameMsg_BeforeUnload(routing_id_));
  }
}

void RenderFrameHostImpl::DisownOpener() {
  Send(new FrameMsg_DisownOpener(GetRoutingID()));
}

void RenderFrameHostImpl::ExtendSelectionAndDelete(size_t before,
                                                   size_t after) {
  Send(new InputMsg_ExtendSelectionAndDelete(routing_id_, before, after));
}

void RenderFrameHostImpl::JavaScriptDialogClosed(
    IPC::Message* reply_msg,
    bool success,
    const base::string16& user_input,
    bool dialog_was_suppressed) {
  GetProcess()->SetIgnoreInputEvents(false);
  bool is_waiting = is_waiting_for_beforeunload_ack_ || IsWaitingForUnloadACK();

  // If we are executing as part of (before)unload event handling, we don't
  // want to use the regular hung_renderer_delay_ms_ if the user has agreed to
  // leave the current page. In this case, use the regular timeout value used
  // during the (before)unload handling.
  if (is_waiting) {
    render_view_host_->StartHangMonitorTimeout(
        success
            ? TimeDelta::FromMilliseconds(RenderViewHostImpl::kUnloadTimeoutMS)
            : render_view_host_->hung_renderer_delay_);
  }

  FrameHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg,
                                                      success, user_input);
  Send(reply_msg);

  // If we are waiting for an unload or beforeunload ack and the user has
  // suppressed messages, kill the tab immediately; a page that's spamming
  // alerts in onbeforeunload is presumably malicious, so there's no point in
  // continuing to run its script and dragging out the process.
  // This must be done after sending the reply since RenderView can't close
  // correctly while waiting for a response.
  if (is_waiting && dialog_was_suppressed)
    render_view_host_->delegate_->RendererUnresponsive(render_view_host_);
}

// PlzNavigate
void RenderFrameHostImpl::CommitNavigation(
    ResourceResponse* response,
    scoped_ptr<StreamHandle> body,
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    const HistoryNavigationParams& history_params) {
  DCHECK((response && body.get()) ||
          !NavigationRequest::ShouldMakeNetworkRequest(common_params.url));
  // TODO(clamy): Check if we have to add security checks for the browser plugin
  // guests.

  // Get back to a clean state, in case we start a new navigation without
  // completing a RFH swap or unload handler.
  SetState(RenderFrameHostImpl::STATE_DEFAULT);

  const GURL body_url = body.get() ? body->GetURL() : GURL();
  const ResourceResponseHead head = response ?
      response->head : ResourceResponseHead();
  Send(new FrameMsg_CommitNavigation(routing_id_, head, body_url, common_params,
                                     commit_params, history_params));
  // TODO(clamy): Check if we should start the throbber for non javascript urls
  // here.

  // TODO(clamy): Release the stream handle once the renderer has finished
  // reading it.
  stream_handle_ = body.Pass();
}

void RenderFrameHostImpl::SetUpMojoIfNeeded() {
  if (service_registry_.get())
    return;

  service_registry_.reset(new ServiceRegistryImpl());
  if (!GetProcess()->GetServiceRegistry())
    return;

  RegisterMojoServices();
  RenderFrameSetupPtr setup;
  GetProcess()->GetServiceRegistry()->ConnectToRemoteService(&setup);

  mojo::ServiceProviderPtr exposed_services;
  service_registry_->Bind(GetProxy(&exposed_services));

  mojo::ServiceProviderPtr services;
  setup->ExchangeServiceProviders(routing_id_, GetProxy(&services),
                                  exposed_services.Pass());
  service_registry_->BindRemoteServiceProvider(services.Pass());

#if defined(OS_ANDROID)
  service_registry_android_.reset(
      new ServiceRegistryAndroid(service_registry_.get()));
#endif
}

void RenderFrameHostImpl::InvalidateMojoConnection() {
#if defined(OS_ANDROID)
  // The Android-specific service registry has a reference to
  // |service_registry_| and thus must be torn down first.
  service_registry_android_.reset();
#endif

  service_registry_.reset();
}

bool RenderFrameHostImpl::IsFocused() {
  // TODO(mlamouri,kenrb): call GetRenderWidgetHost() directly when it stops
  // returning nullptr in some cases. See https://crbug.com/455245.
  return RenderWidgetHostImpl::From(
            GetView()->GetRenderWidgetHost())->is_focused() &&
         frame_tree_->GetFocusedFrame() &&
         (frame_tree_->GetFocusedFrame() == frame_tree_node() ||
          frame_tree_->GetFocusedFrame()->IsDescendantOf(frame_tree_node()));
}

void RenderFrameHostImpl::UpdateCrossProcessIframeAccessibility(
    const std::map<int32, int>& node_to_frame_routing_id_map) {
  for (const auto& iter : node_to_frame_routing_id_map) {
    // This is the id of the accessibility node that has a child frame.
    int32 node_id = iter.first;
    // The routing id from either a RenderFrame or a RenderFrameProxy.
    int frame_routing_id = iter.second;

    FrameTree* frame_tree = frame_tree_node()->frame_tree();
    FrameTreeNode* child_frame_tree_node = frame_tree->FindByRoutingID(
        GetProcess()->GetID(), frame_routing_id);

    if (child_frame_tree_node) {
      FrameAccessibility::GetInstance()->AddChildFrame(
          this, node_id, child_frame_tree_node->frame_tree_node_id());
    }
  }
}

void RenderFrameHostImpl::UpdateGuestFrameAccessibility(
    const std::map<int32, int>& node_to_browser_plugin_instance_id_map) {
  for (const auto& iter : node_to_browser_plugin_instance_id_map) {
    // This is the id of the accessibility node that hosts a plugin.
    int32 node_id = iter.first;
    // The id of the browser plugin.
    int browser_plugin_instance_id = iter.second;
    FrameAccessibility::GetInstance()->AddGuestWebContents(
        this, node_id, browser_plugin_instance_id);
  }
}

bool RenderFrameHostImpl::IsSameSiteInstance(
    RenderFrameHostImpl* other_render_frame_host) {
  // As a sanity check, make sure the frame belongs to the same BrowserContext.
  CHECK_EQ(GetSiteInstance()->GetBrowserContext(),
           other_render_frame_host->GetSiteInstance()->GetBrowserContext());
  return GetSiteInstance() == other_render_frame_host->GetSiteInstance();
}

void RenderFrameHostImpl::SetAccessibilityMode(AccessibilityMode mode) {
  Send(new FrameMsg_SetAccessibilityMode(routing_id_, mode));
}

void RenderFrameHostImpl::SetAccessibilityCallbackForTesting(
    const base::Callback<void(ui::AXEvent, int)>& callback) {
  accessibility_testing_callback_ = callback;
}

const ui::AXTree* RenderFrameHostImpl::GetAXTreeForTesting() {
  return ax_tree_for_testing_.get();
}

BrowserAccessibilityManager*
    RenderFrameHostImpl::GetOrCreateBrowserAccessibilityManager() {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view &&
      !browser_accessibility_manager_ &&
      !no_create_browser_accessibility_manager_for_testing_) {
    browser_accessibility_manager_.reset(
        view->CreateBrowserAccessibilityManager(this));
    if (browser_accessibility_manager_)
      UMA_HISTOGRAM_COUNTS("Accessibility.FrameEnabledCount", 1);
    else
      UMA_HISTOGRAM_COUNTS("Accessibility.FrameDidNotEnableCount", 1);
  }
  return browser_accessibility_manager_.get();
}

void RenderFrameHostImpl::ActivateFindInPageResultForAccessibility(
    int request_id) {
  AccessibilityMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode & AccessibilityModeFlagPlatform) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->ActivateFindInPageResult(request_id);
  }
}

void RenderFrameHostImpl::InsertVisualStateCallback(
    const VisualStateCallback& callback) {
  static uint64 next_id = 1;
  uint64 key = next_id++;
  Send(new FrameMsg_VisualStateRequest(routing_id_, key));
  visual_state_callbacks_.insert(std::make_pair(key, callback));
}

#if defined(OS_WIN)

void RenderFrameHostImpl::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetView());
  if (view)
    view->SetParentNativeViewAccessible(accessible_parent);
}

gfx::NativeViewAccessible
RenderFrameHostImpl::GetParentNativeViewAccessible() const {
  return delegate_->GetParentNativeViewAccessible();
}

#elif defined(OS_MACOSX)

void RenderFrameHostImpl::DidSelectPopupMenuItem(int selected_index) {
  Send(new FrameMsg_SelectPopupMenuItem(routing_id_, selected_index));
}

void RenderFrameHostImpl::DidCancelPopupMenu() {
  Send(new FrameMsg_SelectPopupMenuItem(routing_id_, -1));
}

#elif defined(OS_ANDROID)

void RenderFrameHostImpl::DidSelectPopupMenuItems(
    const std::vector<int>& selected_indices) {
  Send(new FrameMsg_SelectPopupMenuItems(routing_id_, false, selected_indices));
}

void RenderFrameHostImpl::DidCancelPopupMenu() {
  Send(new FrameMsg_SelectPopupMenuItems(
      routing_id_, true, std::vector<int>()));
}

#endif

void RenderFrameHostImpl::ClearPendingTransitionRequestData() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &TransitionRequestManager::ClearPendingTransitionRequestData,
          base::Unretained(TransitionRequestManager::GetInstance()),
          GetProcess()->GetID(),
          routing_id_));
}

void RenderFrameHostImpl::SetNavigationsSuspended(
    bool suspend,
    const base::TimeTicks& proceed_time) {
  // This should only be called to toggle the state.
  DCHECK(navigations_suspended_ != suspend);

  navigations_suspended_ = suspend;
  if (navigations_suspended_) {
    TRACE_EVENT_ASYNC_BEGIN0("navigation",
                             "RenderFrameHostImpl navigation suspended", this);
  } else {
    TRACE_EVENT_ASYNC_END0("navigation",
                           "RenderFrameHostImpl navigation suspended", this);
  }

  if (!suspend && suspended_nav_params_) {
    // There's navigation message params waiting to be sent. Now that we're not
    // suspended anymore, resume navigation by sending them. If we were swapped
    // out, we should also stop filtering out the IPC messages now.
    SetState(RenderFrameHostImpl::STATE_DEFAULT);

    DCHECK(!proceed_time.is_null());
    suspended_nav_params_->commit_params.browser_navigation_start =
        proceed_time;
    Send(new FrameMsg_Navigate(routing_id_,
                               suspended_nav_params_->common_params,
                               suspended_nav_params_->start_params,
                               suspended_nav_params_->commit_params,
                               suspended_nav_params_->history_params));
    suspended_nav_params_.reset();
  }
}

void RenderFrameHostImpl::CancelSuspendedNavigations() {
  // Clear any state if a pending navigation is canceled or preempted.
  if (suspended_nav_params_)
    suspended_nav_params_.reset();

  TRACE_EVENT_ASYNC_END0("navigation",
                         "RenderFrameHostImpl navigation suspended", this);
  navigations_suspended_ = false;
}

void RenderFrameHostImpl::DidUseGeolocationPermission() {
  RenderFrameHost* top_frame = frame_tree_node()->frame_tree()->GetMainFrame();
  GetContentClient()->browser()->RegisterPermissionUsage(
      PERMISSION_GEOLOCATION,
      delegate_->GetAsWebContents(),
      GetLastCommittedURL().GetOrigin(),
      top_frame->GetLastCommittedURL().GetOrigin());
}

}  // namespace content
