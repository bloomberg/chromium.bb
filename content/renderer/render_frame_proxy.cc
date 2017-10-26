// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_proxy.h"

#include <stdint.h>
#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "components/viz/common/switches.h"
#include "content/child/feature_policy/feature_policy_platform.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/frame_policy.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/frame_owner_properties.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/mash_util.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebTriggeringEventInfo.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(USE_AURA)
#include "content/renderer/mus/mus_embedded_frame.h"
#include "content/renderer/mus/renderer_window_tree_client.h"
#endif

namespace content {

namespace {

// Facilitates lookup of RenderFrameProxy by routing_id.
typedef std::map<int, RenderFrameProxy*> RoutingIDProxyMap;
static base::LazyInstance<RoutingIDProxyMap>::DestructorAtExit
    g_routing_id_proxy_map = LAZY_INSTANCE_INITIALIZER;

// Facilitates lookup of RenderFrameProxy by WebRemoteFrame.
typedef std::map<blink::WebRemoteFrame*, RenderFrameProxy*> FrameMap;
base::LazyInstance<FrameMap>::DestructorAtExit g_frame_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
RenderFrameProxy* RenderFrameProxy::CreateProxyToReplaceFrame(
    RenderFrameImpl* frame_to_replace,
    int routing_id,
    blink::WebTreeScopeType scope) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);

  std::unique_ptr<RenderFrameProxy> proxy(new RenderFrameProxy(routing_id));
  proxy->unique_name_ = frame_to_replace->unique_name();

  // When a RenderFrame is replaced by a RenderProxy, the WebRemoteFrame should
  // always come from WebRemoteFrame::create and a call to WebFrame::swap must
  // follow later.
  blink::WebRemoteFrame* web_frame =
      blink::WebRemoteFrame::Create(scope, proxy.get());

  // If frame_to_replace has a RenderFrameProxy parent, then its
  // RenderWidget will be destroyed along with it, so the new
  // RenderFrameProxy uses its parent's RenderWidget.
  RenderWidget* widget =
      (!frame_to_replace->GetWebFrame()->Parent() ||
       frame_to_replace->GetWebFrame()->Parent()->IsWebLocalFrame())
          ? frame_to_replace->GetRenderWidget()
          : RenderFrameProxy::FromWebFrame(
                frame_to_replace->GetWebFrame()->Parent()->ToWebRemoteFrame())
                ->render_widget();
  proxy->Init(web_frame, frame_to_replace->render_view(), widget);
  return proxy.release();
}

RenderFrameProxy* RenderFrameProxy::CreateFrameProxy(
    int routing_id,
    int render_view_routing_id,
    blink::WebFrame* opener,
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

  std::unique_ptr<RenderFrameProxy> proxy(new RenderFrameProxy(routing_id));
  RenderViewImpl* render_view = nullptr;
  RenderWidget* render_widget = nullptr;
  blink::WebRemoteFrame* web_frame = nullptr;

  if (!parent) {
    // Create a top level WebRemoteFrame.
    render_view = RenderViewImpl::FromRoutingID(render_view_routing_id);
    web_frame = blink::WebRemoteFrame::CreateMainFrame(
        render_view->GetWebView(), proxy.get(), opener);
    render_widget = render_view->GetWidget();

    // If the RenderView is reused by this proxy after having been used for a
    // pending RenderFrame that was discarded, its swapped out state needs to
    // be updated, as the OnSwapOut flow which normally does this won't happen
    // in that case.  See https://crbug.com/653746 and
    // https://crbug.com/651980.
    if (!render_view->is_swapped_out())
      render_view->SetSwappedOut(true);
  } else {
    // Create a frame under an existing parent. The parent is always expected
    // to be a RenderFrameProxy, because navigations initiated by local frames
    // should not wind up here.

    web_frame = parent->web_frame()->CreateRemoteChild(
        replicated_state.scope,
        blink::WebString::FromUTF8(replicated_state.name),
        replicated_state.frame_policy.sandbox_flags,
        FeaturePolicyHeaderToWeb(
            replicated_state.frame_policy.container_policy),
        proxy.get(), opener);
    proxy->unique_name_ = replicated_state.unique_name;
    render_view = parent->render_view();
    render_widget = parent->render_widget();
  }

  proxy->Init(web_frame, render_view, render_widget);

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
RenderFrameProxy* RenderFrameProxy::FromWebFrame(
    blink::WebRemoteFrame* web_frame) {
  // TODO(dcheng): Turn this into a DCHECK() if it doesn't crash on canary.
  CHECK(web_frame);
  FrameMap::iterator iter = g_frame_map.Get().find(web_frame);
  if (iter != g_frame_map.Get().end()) {
    RenderFrameProxy* proxy = iter->second;
    DCHECK_EQ(web_frame, proxy->web_frame());
    return proxy;
  }
  // Reaching this is not expected: this implies that the |web_frame| in
  // question is not managed by the content API, or the associated
  // RenderFrameProxy is already deleted--in which case, it's not safe to touch
  // |web_frame|.
  NOTREACHED();
  return NULL;
}

RenderFrameProxy::RenderFrameProxy(int routing_id)
    : routing_id_(routing_id),
      provisional_frame_routing_id_(MSG_ROUTING_NONE),
      web_frame_(nullptr),
      render_view_(nullptr),
      render_widget_(nullptr) {
  std::pair<RoutingIDProxyMap::iterator, bool> result =
      g_routing_id_proxy_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";
  RenderThread::Get()->AddRoute(routing_id_, this);
}

RenderFrameProxy::~RenderFrameProxy() {
  render_widget_->UnregisterRenderFrameProxy(this);

  CHECK(!web_frame_);
  RenderThread::Get()->RemoveRoute(routing_id_);
  g_routing_id_proxy_map.Get().erase(routing_id_);
}

void RenderFrameProxy::Init(blink::WebRemoteFrame* web_frame,
                            RenderViewImpl* render_view,
                            RenderWidget* render_widget) {
  CHECK(web_frame);
  CHECK(render_view);
  CHECK(render_widget);

  web_frame_ = web_frame;
  render_view_ = render_view;
  render_widget_ = render_widget;

  render_widget_->RegisterRenderFrameProxy(this);

  std::pair<FrameMap::iterator, bool> result =
      g_frame_map.Get().insert(std::make_pair(web_frame_, this));
  CHECK(result.second) << "Inserted a duplicate item.";

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  enable_surface_synchronization_ =
      command_line.HasSwitch(switches::kEnableSurfaceSynchronization);

  compositing_helper_.reset(
      ChildFrameCompositingHelper::CreateForRenderFrameProxy(this));

#if defined(USE_AURA)
  if (IsRunningInMash()) {
    RendererWindowTreeClient* renderer_window_tree_client =
        RendererWindowTreeClient::Get(render_widget_->routing_id());
    // It's possible a MusEmbeddedFrame has already been scheduled for creation
    // (that is, RendererWindowTreeClient::Embed() was called). Call to
    // OnRenderFrameProxyCreated() to potentially get the MusEmbeddedFrame.
    // OnRenderFrameProxyCreated() returns null if Embed() was not called.
    mus_embedded_frame_ =
        renderer_window_tree_client->OnRenderFrameProxyCreated(this);
  }
#endif
}

void RenderFrameProxy::ResendFrameRects() {
  // Reset |frame_rect_| in order to allocate a new viz::LocalSurfaceId.
  gfx::Rect rect = frame_rect_;
  frame_rect_ = gfx::Rect();
  FrameRectsChanged(rect);
}

void RenderFrameProxy::WillBeginCompositorFrame() {
  if (compositing_helper_->surface_id().is_valid()) {
    FrameHostMsg_HittestData_Params params;
    params.surface_id = compositing_helper_->surface_id();
    params.ignored_for_hittest = web_frame_->IsIgnoredForHitTest();
    render_widget_->QueueMessage(
        new FrameHostMsg_HittestData(render_widget_->routing_id(), params),
        MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
  }
}

void RenderFrameProxy::DidCommitCompositorFrame() {
}

void RenderFrameProxy::SetReplicatedState(const FrameReplicationState& state) {
  DCHECK(web_frame_);
  web_frame_->SetReplicatedOrigin(state.origin);
  web_frame_->SetReplicatedSandboxFlags(state.frame_policy.sandbox_flags);
  web_frame_->SetReplicatedName(blink::WebString::FromUTF8(state.name));
  web_frame_->SetReplicatedInsecureRequestPolicy(state.insecure_request_policy);
  web_frame_->SetReplicatedPotentiallyTrustworthyUniqueOrigin(
      state.has_potentially_trustworthy_unique_origin);
  web_frame_->SetReplicatedFeaturePolicyHeader(
      FeaturePolicyHeaderToWeb(state.feature_policy_header));
  if (state.has_received_user_gesture)
    web_frame_->SetHasReceivedUserGesture();

  web_frame_->ResetReplicatedContentSecurityPolicy();
  OnAddContentSecurityPolicies(state.accumulated_csp_headers);
}

// Update the proxy's SecurityContext and FrameOwner with new sandbox flags
// and container policy that were set by its parent in another process.
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
void RenderFrameProxy::OnDidUpdateFramePolicy(const FramePolicy& frame_policy) {
  web_frame_->SetReplicatedSandboxFlags(frame_policy.sandbox_flags);
  web_frame_->SetFrameOwnerPolicy(
      frame_policy.sandbox_flags,
      FeaturePolicyHeaderToWeb(frame_policy.container_policy));
}

void RenderFrameProxy::MaybeUpdateCompositingHelper() {
  if (!frame_sink_id_.is_valid() || !local_surface_id_.is_valid())
    return;

  float device_scale_factor = render_widget_->GetOriginalDeviceScaleFactor();
  viz::SurfaceInfo surface_info(
      viz::SurfaceId(frame_sink_id_, local_surface_id_), device_scale_factor,
      gfx::ScaleToCeiledSize(frame_rect_.size(), device_scale_factor));

  if (enable_surface_synchronization_)
    compositing_helper_->SetPrimarySurfaceInfo(surface_info);
}

void RenderFrameProxy::SetChildFrameSurface(
    const viz::SurfaceInfo& surface_info,
    const viz::SurfaceSequence& sequence) {
  // If this WebFrame has already been detached, its parent will be null. This
  // can happen when swapping a WebRemoteFrame with a WebLocalFrame, where this
  // message may arrive after the frame was removed from the frame tree, but
  // before the frame has been destroyed. http://crbug.com/446575.
  if (!web_frame()->Parent())
    return;

  if (!enable_surface_synchronization_)
    compositing_helper_->SetPrimarySurfaceInfo(surface_info);
  compositing_helper_->SetFallbackSurfaceId(surface_info.id(), sequence);
}

bool RenderFrameProxy::OnMessageReceived(const IPC::Message& msg) {
  // Forward Page IPCs to the RenderView.
  if ((IPC_MESSAGE_CLASS(msg) == PageMsgStart)) {
    if (render_view())
      return render_view()->OnMessageReceived(msg);

    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxy, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_DeleteProxy, OnDeleteProxy)
    IPC_MESSAGE_HANDLER(FrameMsg_ChildFrameProcessGone, OnChildFrameProcessGone)
    IPC_MESSAGE_HANDLER(FrameMsg_SetChildFrameSurface, OnSetChildFrameSurface)
    IPC_MESSAGE_HANDLER(FrameMsg_UpdateOpener, OnUpdateOpener)
    IPC_MESSAGE_HANDLER(FrameMsg_ViewChanged, OnViewChanged)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStartLoading, OnDidStartLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateFramePolicy, OnDidUpdateFramePolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameMsg_Collapse, OnCollapse)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateName, OnDidUpdateName)
    IPC_MESSAGE_HANDLER(FrameMsg_AddContentSecurityPolicies,
                        OnAddContentSecurityPolicies)
    IPC_MESSAGE_HANDLER(FrameMsg_ResetContentSecurityPolicy,
                        OnResetContentSecurityPolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_EnforceInsecureRequestPolicy,
                        OnEnforceInsecureRequestPolicy)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFrameOwnerProperties,
                        OnSetFrameOwnerProperties)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateOrigin, OnDidUpdateOrigin)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetPageFocus)
    IPC_MESSAGE_HANDLER(FrameMsg_SetFocusedFrame, OnSetFocusedFrame)
    IPC_MESSAGE_HANDLER(FrameMsg_WillEnterFullscreen, OnWillEnterFullscreen)
    IPC_MESSAGE_HANDLER(FrameMsg_SetHasReceivedUserGesture,
                        OnSetHasReceivedUserGesture)
    IPC_MESSAGE_HANDLER(FrameMsg_ScrollRectToVisible, OnScrollRectToVisible)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Note: If |handled| is true, |this| may have been deleted.
  return handled;
}

bool RenderFrameProxy::Send(IPC::Message* message) {
  return RenderThread::Get()->Send(message);
}

void RenderFrameProxy::OnDeleteProxy() {
  DCHECK(web_frame_);
  web_frame_->Detach();
}

void RenderFrameProxy::OnChildFrameProcessGone() {
  compositing_helper_->ChildFrameGone();
}

void RenderFrameProxy::OnSetChildFrameSurface(
    const viz::SurfaceInfo& surface_info,
    const viz::SurfaceSequence& sequence) {
  SetChildFrameSurface(surface_info, sequence);
}

void RenderFrameProxy::OnUpdateOpener(int opener_routing_id) {
  blink::WebFrame* opener = RenderFrameImpl::ResolveOpener(opener_routing_id);
  web_frame_->SetOpener(opener);
}

void RenderFrameProxy::OnDidStartLoading() {
  web_frame_->DidStartLoading();
}

void RenderFrameProxy::OnViewChanged(const viz::FrameSinkId& frame_sink_id) {
  // In mash the FrameSinkId comes from RendererWindowTreeClient.
  if (!IsRunningInMash())
    frame_sink_id_ = frame_sink_id;

  // Resend the FrameRects and allocate a new viz::LocalSurfaceId when the view
  // changes.
  ResendFrameRects();
}

void RenderFrameProxy::OnDidStopLoading() {
  web_frame_->DidStopLoading();
}

void RenderFrameProxy::OnDispatchLoad() {
  web_frame_->DispatchLoadEventOnFrameOwner();
}

void RenderFrameProxy::OnCollapse(bool collapsed) {
  web_frame_->Collapse(collapsed);
}

void RenderFrameProxy::OnDidUpdateName(const std::string& name,
                                       const std::string& unique_name) {
  web_frame_->SetReplicatedName(blink::WebString::FromUTF8(name));
  unique_name_ = unique_name;
}

void RenderFrameProxy::OnAddContentSecurityPolicies(
    const std::vector<ContentSecurityPolicyHeader>& headers) {
  for (const auto& header : headers) {
    web_frame_->AddReplicatedContentSecurityPolicyHeader(
        blink::WebString::FromUTF8(header.header_value), header.type,
        header.source);
  }
}

void RenderFrameProxy::OnResetContentSecurityPolicy() {
  web_frame_->ResetReplicatedContentSecurityPolicy();
}

void RenderFrameProxy::OnEnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  web_frame_->SetReplicatedInsecureRequestPolicy(policy);
}

void RenderFrameProxy::OnSetFrameOwnerProperties(
    const FrameOwnerProperties& properties) {
  web_frame_->SetFrameOwnerProperties(
      ConvertFrameOwnerPropertiesToWebFrameOwnerProperties(properties));
}

void RenderFrameProxy::OnDidUpdateOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  web_frame_->SetReplicatedOrigin(origin);
  web_frame_->SetReplicatedPotentiallyTrustworthyUniqueOrigin(
      is_potentially_trustworthy_unique_origin);
}

void RenderFrameProxy::OnSetPageFocus(bool is_focused) {
  render_view_->SetFocus(is_focused);
}

void RenderFrameProxy::OnSetFocusedFrame() {
  // This uses focusDocumentView rather than setFocusedFrame so that blur
  // events are properly dispatched on any currently focused elements.
  render_view_->webview()->FocusDocumentView(web_frame_);
}

void RenderFrameProxy::OnWillEnterFullscreen() {
  web_frame_->WillEnterFullscreen();
}

void RenderFrameProxy::OnSetHasReceivedUserGesture() {
  web_frame_->SetHasReceivedUserGesture();
}

void RenderFrameProxy::OnScrollRectToVisible(
    const gfx::Rect& rect_to_scroll,
    const blink::WebRemoteScrollProperties& properties) {
  web_frame_->ScrollRectToVisible(rect_to_scroll, properties);
}

#if defined(USE_AURA)
void RenderFrameProxy::SetMusEmbeddedFrame(
    std::unique_ptr<MusEmbeddedFrame> mus_embedded_frame) {
  mus_embedded_frame_ = std::move(mus_embedded_frame);
}
#endif

void RenderFrameProxy::FrameDetached(DetachType type) {
#if defined(USE_AURA)
  mus_embedded_frame_.reset();
#endif

  if (type == DetachType::kRemove && web_frame_->Parent()) {
    // Let the browser process know this subframe is removed, so that it is
    // destroyed in its current process.
    Send(new FrameHostMsg_Detach(routing_id_));
  }

  web_frame_->Close();

  // If this proxy was associated with a provisional RenderFrame, and we're not
  // in the process of swapping with it, clean it up as well.
  if (type == DetachType::kRemove &&
      provisional_frame_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameImpl* provisional_frame =
        RenderFrameImpl::FromRoutingID(provisional_frame_routing_id_);
    // |provisional_frame| should always exist.  If it was deleted via
    // FrameMsg_Delete right before this proxy was removed,
    // RenderFrameImpl::frameDetached would've cleared this proxy's
    // |provisional_frame_routing_id_| and we wouldn't get here.
    CHECK(provisional_frame);
    provisional_frame->GetWebFrame()->Detach();
  }

  // Remove the entry in the WebFrame->RenderFrameProxy map, as the |web_frame_|
  // is no longer valid.
  FrameMap::iterator it = g_frame_map.Get().find(web_frame_);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

  web_frame_ = nullptr;

  delete this;
}

void RenderFrameProxy::ForwardPostMessage(
    blink::WebLocalFrame* source_frame,
    blink::WebRemoteFrame* target_frame,
    blink::WebSecurityOrigin target_origin,
    blink::WebDOMMessageEvent event) {
  DCHECK(!web_frame_ || web_frame_ == target_frame);

  FrameMsg_PostMessage_Params params;
  params.is_data_raw_string = false;
  params.data = event.Data().ToString().Utf16();
  params.source_origin = event.Origin().Utf16();
  if (!target_origin.IsNull())
    params.target_origin = target_origin.ToString().Utf16();

  params.message_ports = event.ReleaseChannels().ReleaseVector();

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

void RenderFrameProxy::Navigate(const blink::WebURLRequest& request,
                                bool should_replace_current_entry) {
  FrameHostMsg_OpenURL_Params params;
  params.url = request.Url();
  params.uses_post = request.HttpMethod().Utf8() == "POST";
  params.resource_request_body = GetRequestBodyForWebURLRequest(request);
  params.extra_headers = GetWebURLRequestHeadersAsString(request);
  params.referrer = Referrer(blink::WebStringToGURL(request.HttpHeaderField(
                                 blink::WebString::FromUTF8("Referer"))),
                             request.GetReferrerPolicy());
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = request.HasUserGesture();
  params.triggering_event_info = blink::WebTriggeringEventInfo::kUnknown;
  Send(new FrameHostMsg_OpenURL(routing_id_, params));
}

void RenderFrameProxy::FrameRectsChanged(const blink::WebRect& frame_rect) {
  gfx::Rect rect = frame_rect;
  const bool did_size_change = frame_rect_.size() != rect.size();
#if defined(USE_AURA)
  const bool did_rect_change = did_size_change || frame_rect_ != rect;
#endif

  frame_rect_ = rect;

  if (did_size_change || !local_surface_id_.is_valid()) {
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
    MaybeUpdateCompositingHelper();
  }

#if defined(USE_AURA)
  if (did_rect_change && mus_embedded_frame_)
    mus_embedded_frame_->SetWindowBounds(local_surface_id_, rect);
#endif

  if (IsUseZoomForDSFEnabled()) {
    rect = gfx::ScaleToEnclosingRect(
        rect, 1.f / render_widget_->GetOriginalDeviceScaleFactor());
  }

  Send(new FrameHostMsg_FrameRectChanged(routing_id_, rect, local_surface_id_));
}

void RenderFrameProxy::UpdateRemoteViewportIntersection(
    const blink::WebRect& viewportIntersection) {
  Send(new FrameHostMsg_UpdateViewportIntersection(
      routing_id_, gfx::Rect(viewportIntersection)));
}

void RenderFrameProxy::VisibilityChanged(bool visible) {
  Send(new FrameHostMsg_VisibilityChanged(routing_id_, visible));
}

void RenderFrameProxy::SetIsInert(bool inert) {
  Send(new FrameHostMsg_SetIsInert(routing_id_, inert));
}

void RenderFrameProxy::DidChangeOpener(blink::WebFrame* opener) {
  // A proxy shouldn't normally be disowning its opener.  It is possible to get
  // here when a proxy that is being detached clears its opener, in which case
  // there is no need to notify the browser process.
  if (!opener)
    return;

  // Only a LocalFrame (i.e., the caller of window.open) should be able to
  // update another frame's opener.
  DCHECK(opener->IsWebLocalFrame());

  int opener_routing_id =
      RenderFrameImpl::FromWebFrame(opener->ToWebLocalFrame())->GetRoutingID();
  Send(new FrameHostMsg_DidChangeOpener(routing_id_, opener_routing_id));
}

void RenderFrameProxy::AdvanceFocus(blink::WebFocusType type,
                                    blink::WebLocalFrame* source) {
  int source_routing_id = RenderFrameImpl::FromWebFrame(source)->GetRoutingID();
  Send(new FrameHostMsg_AdvanceFocus(routing_id_, type, source_routing_id));
}

void RenderFrameProxy::FrameFocused() {
  Send(new FrameHostMsg_FrameFocused(routing_id_));
}

#if defined(USE_AURA)
void RenderFrameProxy::OnMusEmbeddedFrameSurfaceChanged(
    const viz::SurfaceInfo& surface_info) {
  SetChildFrameSurface(surface_info, viz::SurfaceSequence());
}

void RenderFrameProxy::OnMusEmbeddedFrameSinkIdAllocated(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  MaybeUpdateCompositingHelper();
  // Resend the FrameRects and allocate a new viz::LocalSurfaceId when the view
  // changes.
  ResendFrameRects();
}
#endif

}  // namespace
