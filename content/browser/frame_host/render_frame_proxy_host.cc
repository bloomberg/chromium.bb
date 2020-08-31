// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/ipc_utils.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"

namespace content {

namespace {

RenderFrameProxyHost::CreatedCallback& GetProxyHostCreatedCallback() {
  static base::NoDestructor<RenderFrameProxyHost::CreatedCallback> s_callback;
  return *s_callback;
}

// The (process id, routing id) pair that identifies one RenderFrameProxy.
typedef std::pair<int32_t, int32_t> RenderFrameProxyHostID;
typedef std::unordered_map<RenderFrameProxyHostID,
                           RenderFrameProxyHost*,
                           base::IntPairHash<RenderFrameProxyHostID>>
    RoutingIDFrameProxyMap;
base::LazyInstance<RoutingIDFrameProxyMap>::DestructorAtExit
    g_routing_id_frame_proxy_map = LAZY_INSTANCE_INITIALIZER;

using TokenFrameMap = std::unordered_map<base::UnguessableToken,
                                         RenderFrameProxyHost*,
                                         base::UnguessableTokenHash>;
base::LazyInstance<TokenFrameMap>::Leaky g_token_frame_proxy_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void RenderFrameProxyHost::SetCreatedCallbackForTesting(
    const CreatedCallback& created_callback) {
  GetProxyHostCreatedCallback() = created_callback;
}

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromID(int process_id,
                                                   int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameProxyMap* frames = g_routing_id_frame_proxy_map.Pointer();
  auto it = frames->find(RenderFrameProxyHostID(process_id, routing_id));
  return it == frames->end() ? nullptr : it->second;
}

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromFrameToken(
    int process_id,
    const base::UnguessableToken& frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TokenFrameMap* frames = g_token_frame_proxy_map.Pointer();
  auto it = frames->find(frame_token);
  // The check against |process_id| isn't strictly necessary, but represents
  // an extra level of protection against a renderer trying to force a frame
  // token.
  return it != frames->end() && it->second->GetProcess()->GetID() == process_id
             ? it->second
             : nullptr;
}

RenderFrameProxyHost::RenderFrameProxyHost(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    FrameTreeNode* frame_tree_node)
    : routing_id_(site_instance->GetProcess()->GetNextRoutingID()),
      site_instance_(site_instance),
      process_(site_instance->GetProcess()),
      frame_tree_node_(frame_tree_node),
      render_frame_proxy_created_(false),
      render_view_host_(std::move(render_view_host)) {
  GetProcess()->AddRoute(routing_id_, this);
  CHECK(
      g_routing_id_frame_proxy_map.Get()
          .insert(std::make_pair(
              RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_), this))
          .second);
  CHECK(g_token_frame_proxy_map.Get()
            .insert(std::make_pair(frame_token_, this))
            .second);
  CHECK(render_view_host_ ||
        frame_tree_node_->render_manager()->IsMainFrameForInnerDelegate());

  bool is_proxy_to_parent = !frame_tree_node_->IsMainFrame() &&
                            frame_tree_node_->parent()
                                    ->GetSiteInstance() == site_instance;
  bool is_proxy_to_outer_delegate =
      frame_tree_node_->render_manager()->IsMainFrameForInnerDelegate();

  // If this is a proxy to parent frame or this proxy is for the inner
  // WebContents's FrameTreeNode in outer WebContents's SiteInstance, then we
  // need a CrossProcessFrameConnector.
  if (is_proxy_to_parent || is_proxy_to_outer_delegate) {
    // The RenderFrameHost navigating cross-process is destroyed and a proxy for
    // it is created in the parent's process. CrossProcessFrameConnector
    // initialization only needs to happen on an initial cross-process
    // navigation, when the RenderFrameHost leaves the same process as its
    // parent. The same CrossProcessFrameConnector is used for subsequent cross-
    // process navigations, but it will be destroyed if the frame is
    // navigated back to the same SiteInstance as its parent.
    cross_process_frame_connector_.reset(new CrossProcessFrameConnector(this));
  }

  if (!GetProxyHostCreatedCallback().is_null())
    GetProxyHostCreatedCallback().Run(this);
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (GetProcess()->IsInitializedAndNotDead()) {
    // TODO(nasko): For now, don't send this IPC for top-level frames, as
    // the top-level RenderFrame will delete the RenderFrameProxy.
    // This can be removed once we don't have a swapped out state on
    // RenderFrame. See https://crbug.com/357747
    if (!frame_tree_node_->IsMainFrame())
      Send(new UnfreezableFrameMsg_DeleteProxy(routing_id_));
  }

  // TODO(arthursonzogni): There are no known reason for removing the
  // RenderViewHostImpl here instead of automatically at the end of the
  // destructor. This line can be removed.
  render_view_host_.reset();

  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_proxy_map.Get().erase(
      RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_));
  g_token_frame_proxy_map.Get().erase(frame_token_);
}

void RenderFrameProxyHost::SetChildRWHView(
    RenderWidgetHostView* view,
    const gfx::Size* initial_frame_size) {
  cross_process_frame_connector_->SetView(
      static_cast<RenderWidgetHostViewChildFrame*>(view));
  if (initial_frame_size)
    cross_process_frame_connector_->SetLocalFrameSize(*initial_frame_size);
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  return frame_tree_node_->frame_tree()
      ->GetRenderViewHost(site_instance_.get())
      .get();
}

RenderWidgetHostView* RenderFrameProxyHost::GetRenderWidgetHostView() {
  return frame_tree_node_->parent()
      ->frame_tree_node()
      ->render_manager()
      ->GetRenderWidgetHostView();
}

bool RenderFrameProxyHost::Send(IPC::Message* msg) {
  return GetProcess()->Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  // Crash reports trigerred by IPC messages for this proxy should be associated
  // with the URL of the current RenderFrameHost that is being proxied.
  ScopedActiveURL scoped_active_url(this);

  if (cross_process_frame_connector_.get() &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxyHost, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RouteMessageEvent, OnRouteMessageEvent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeOpener, OnDidChangeOpener)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PrintCrossProcessSubframe,
                        OnPrintCrossProcessSubframe)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderFrameProxyHost::InitRenderFrameProxy() {
  DCHECK(!render_frame_proxy_created_);

  // If the current RenderFrameHost is pending deletion, no new proxies should
  // be created for it, since this frame should no longer be visible from other
  // processes. We can get here with postMessage while trying to recreate
  // proxies for the sender.
  if (!frame_tree_node_->current_frame_host()->is_active())
    return false;

  // It is possible to reach this when the process is dead (in particular, when
  // creating proxies from CreateProxiesForChildFrame).  In that case, don't
  // create the proxy.  The process shouldn't be resurrected just to create
  // RenderFrameProxies; it should be restored only if it needs to host a
  // RenderFrame.  When that happens, the process will be reinitialized, and
  // all necessary proxies, including any of the ones we skipped here, will be
  // created by CreateProxiesForSiteInstance. See https://crbug.com/476846
  if (!GetProcess()->IsInitializedAndNotDead())
    return false;

  int parent_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    // It is safe to use GetRenderFrameProxyHost to get the parent proxy, since
    // new child frames always start out as local frames, so a new proxy should
    // never have a RenderFrameHost as a parent.
    RenderFrameProxyHost* parent_proxy =
        frame_tree_node_->parent()
            ->frame_tree_node()
            ->render_manager()
            ->GetRenderFrameProxyHost(site_instance_.get());
    CHECK(parent_proxy);

    // Proxies that aren't live in the parent node should not be initialized
    // here, since there is no valid parent RenderFrameProxy on the renderer
    // side.  This can happen when adding a new child frame after an opener
    // process crashed and was reloaded.  See https://crbug.com/501152.
    if (!parent_proxy->is_render_frame_proxy_live())
      return false;

    parent_routing_id = parent_proxy->GetRoutingID();
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }

  int opener_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->opener()) {
    opener_routing_id = frame_tree_node_->render_manager()->GetOpenerRoutingID(
        site_instance_.get());
  }

  int view_routing_id = frame_tree_node_->frame_tree()
                            ->GetRenderViewHost(site_instance_.get())
                            ->GetRoutingID();
  GetProcess()->GetRendererInterface()->CreateFrameProxy(
      routing_id_, view_routing_id, opener_routing_id, parent_routing_id,
      frame_tree_node_->current_replication_state(), frame_token_,
      frame_tree_node_->devtools_frame_token());

  SetRenderFrameProxyCreated(true);

  // If this proxy was created for a frame that hasn't yet finished loading,
  // let the renderer know so it can also mark the proxy as loading. See
  // https://crbug.com/916137.
  if (frame_tree_node_->IsLoading())
    GetAssociatedRemoteFrame()->DidStartLoading();

  // For subframes, initialize the proxy's FrameOwnerProperties only if they
  // differ from default values.
  bool should_send_properties =
      !frame_tree_node_->frame_owner_properties().Equals(
          blink::mojom::FrameOwnerProperties());
  if (frame_tree_node_->parent() && should_send_properties) {
    auto frame_owner_properties =
        frame_tree_node_->frame_owner_properties().Clone();
    GetAssociatedRemoteFrame()->SetFrameOwnerProperties(
        std::move(frame_owner_properties));
  }

  return true;
}

void RenderFrameProxyHost::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == mojom::RenderFrameProxyHost::Name_) {
    frame_proxy_host_associated_receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::RenderFrameProxyHost>(
            std::move(handle)));
  } else if (interface_name == blink::mojom::RemoteFrameHost::Name_) {
    remote_frame_host_receiver_.Bind(
        mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrameHost>(
            std::move(handle)));
  }
}

blink::AssociatedInterfaceProvider*
RenderFrameProxyHost::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    mojo::AssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        remote_interfaces;
    IPC::ChannelProxy* channel = GetProcess()->GetChannel();
    if (channel) {
      RenderProcessHostImpl* process =
          static_cast<RenderProcessHostImpl*>(GetProcess());
      process->GetRemoteRouteProvider()->GetRoute(
          GetRoutingID(), remote_interfaces.BindNewEndpointAndPassReceiver());
    } else {
      // The channel may not be initialized in some tests environments. In this
      // case we set up a dummy interface provider.
      ignore_result(remote_interfaces
                        .BindNewEndpointAndPassDedicatedReceiverForTesting());
    }
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            remote_interfaces.Unbind());
  }
  return remote_associated_interfaces_.get();
}

void RenderFrameProxyHost::SetRenderFrameProxyCreated(bool created) {
  if (!created) {
    // If the renderer process has gone away, created can be false. In that
    // case, invalidate the mojo connection.
    frame_proxy_host_associated_receiver_.reset();
  }
  render_frame_proxy_created_ = created;
}

const mojo::AssociatedRemote<blink::mojom::RemoteFrame>&
RenderFrameProxyHost::GetAssociatedRemoteFrame() {
  if (!remote_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&remote_frame_);
  return remote_frame_;
}

const mojo::AssociatedRemote<content::mojom::RenderFrameProxy>&
RenderFrameProxyHost::GetAssociatedRenderFrameProxy() {
  if (!render_frame_proxy_)
    GetRemoteAssociatedInterfaces()->GetInterface(&render_frame_proxy_);
  return render_frame_proxy_;
}

void RenderFrameProxyHost::SetInheritedEffectiveTouchAction(
    cc::TouchAction touch_action) {
  cross_process_frame_connector_->OnSetInheritedEffectiveTouchAction(
      touch_action);
}

void RenderFrameProxyHost::UpdateRenderThrottlingStatus(
    bool is_throttled,
    bool subtree_throttled) {
  cross_process_frame_connector_->UpdateRenderThrottlingStatus(
      is_throttled, subtree_throttled);
}

void RenderFrameProxyHost::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  cross_process_frame_connector_->OnVisibilityChanged(visibility);
}

void RenderFrameProxyHost::UpdateOpener() {
  // Another frame in this proxy's SiteInstance may reach the new opener by
  // first reaching this proxy and then referencing its window.opener.  Ensure
  // the new opener's proxy exists in this case.
  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        GetSiteInstance(), frame_tree_node_);
  }

  int opener_routing_id =
      frame_tree_node_->render_manager()->GetOpenerRoutingID(GetSiteInstance());
  Send(new FrameMsg_UpdateOpener(GetRoutingID(), opener_routing_id));
}

void RenderFrameProxyHost::SetFocusedFrame() {
  GetAssociatedRemoteFrame()->Focus();
}

void RenderFrameProxyHost::ScrollRectToVisible(
    const gfx::Rect& rect_to_scroll,
    blink::mojom::ScrollIntoViewParamsPtr params) {
  GetAssociatedRemoteFrame()->ScrollRectToVisible(rect_to_scroll,
                                                  std::move(params));
}

void RenderFrameProxyHost::OnDetach() {
  if (frame_tree_node_->render_manager()->IsMainFrameForInnerDelegate()) {
    frame_tree_node_->render_manager()->RemoveOuterDelegateFrame();
    return;
  }

  // For a main frame with no outer delegate, no further work is needed. In this
  // case, detach can only be triggered by closing the entire RenderViewHost.
  // Instead, this cleanup relies on the destructors of RenderFrameHost and
  // RenderFrameProxyHost decrementing the refcounts of their associated
  // RenderViewHost. When the refcount hits 0, the corresponding renderer object
  // is cleaned up. Since WebContents destruction will also destroy
  // RenderFrameHost/RenderFrameProxyHost objects in FrameTree, this eventually
  // results in all the associated RenderViewHosts being closed.
  if (frame_tree_node_->IsMainFrame())
    return;

  // Otherwise, a remote child frame has been removed from the frame tree.
  // Make sure that this action is mirrored to all the other renderers, so
  // the frame tree remains consistent.
  frame_tree_node_->current_frame_host()->DetachFromProxy();
}

void RenderFrameProxyHost::OnOpenURL(
    const FrameHostMsg_OpenURL_Params& params) {
  // Verify and unpack IPC payload.
  GURL validated_url;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (!VerifyOpenURLParams(GetSiteInstance(), params, &validated_url,
                           &blob_url_loader_factory)) {
    return;
  }

  RenderFrameHostImpl* current_rfh = frame_tree_node_->current_frame_host();

  // The current_rfh may be pending deletion. In this case, ignore the
  // navigation, because the frame is going to disappear soon anyway.
  if (!current_rfh->is_active())
    return;

  // Verify that we are in the same BrowsingInstance as the current
  // RenderFrameHost.
  if (!site_instance_->IsRelatedSiteInstance(current_rfh->GetSiteInstance()))
    return;

  // Since this navigation targeted a specific RenderFrameProxy, it should stay
  // in the current tab.
  DCHECK_EQ(WindowOpenDisposition::CURRENT_TAB, params.disposition);

  // Augment |download_policy| for situations that were not covered on the
  // renderer side, e.g. status not available on remote frame, etc.
  NavigationDownloadPolicy download_policy = params.download_policy;
  GetContentClient()->browser()->AugmentNavigationDownloadPolicy(
      frame_tree_node_->navigator()->GetController()->GetWebContents(),
      current_rfh, params.user_gesture, &download_policy);

  if ((frame_tree_node_->pending_frame_policy().sandbox_flags &
       network::mojom::WebSandboxFlags::kDownloads) !=
      network::mojom::WebSandboxFlags::kNone) {
    if (download_policy.blocking_downloads_in_sandbox_enabled) {
      download_policy.SetDisallowed(content::NavigationDownloadType::kSandbox);
    } else {
      download_policy.SetAllowed(content::NavigationDownloadType::kSandbox);
    }
  }

  // TODO(lfg, lukasza): Remove |extra_headers| parameter from
  // RequestTransferURL method once both RenderFrameProxyHost and
  // RenderFrameHostImpl call RequestOpenURL from their OnOpenURL handlers.
  // See also https://crbug.com/647772.
  // TODO(clamy): The transition should probably be changed for POST navigations
  // to PAGE_TRANSITION_FORM_SUBMIT. See https://crbug.com/829827.
  frame_tree_node_->navigator()->NavigateFromFrameProxy(
      current_rfh, validated_url,
      GlobalFrameRoutingId(GetProcess()->GetID(), params.initiator_routing_id),
      params.initiator_origin, site_instance_.get(), params.referrer,
      ui::PAGE_TRANSITION_LINK, params.should_replace_current_entry,
      download_policy, params.post_body ? "POST" : "GET", params.post_body,
      params.extra_headers, std::move(blob_url_loader_factory),
      params.user_gesture, params.impression);
}

void RenderFrameProxyHost::CheckCompleted() {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();
  target_rfh->GetAssociatedLocalFrame()->CheckCompleted();
}

void RenderFrameProxyHost::EnableAutoResize(const gfx::Size& min_size,
                                            const gfx::Size& max_size) {
  GetAssociatedRenderFrameProxy()->EnableAutoResize(min_size, max_size);
}

void RenderFrameProxyHost::DisableAutoResize() {
  GetAssociatedRenderFrameProxy()->DisableAutoResize();
}

void RenderFrameProxyHost::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  GetAssociatedRenderFrameProxy()->DidUpdateVisualProperties(metadata);
}

void RenderFrameProxyHost::ChildProcessGone() {
  GetAssociatedRenderFrameProxy()->ChildProcessGone();
}

void RenderFrameProxyHost::OnRouteMessageEvent(
    const FrameMsg_PostMessage_Params& params) {
  RenderFrameHostImpl* target_rfh = frame_tree_node()->current_frame_host();
  if (!target_rfh->IsRenderFrameLive()) {
    // Check if there is an inner delegate involved; if so target its main
    // frame or otherwise return since there is no point in forwarding the
    // message.
    target_rfh = target_rfh->delegate()->GetMainFrameForInnerDelegate(
        target_rfh->frame_tree_node());
    if (!target_rfh || !target_rfh->IsRenderFrameLive())
      return;
  }

  // |targetOrigin| argument of postMessage is already checked by
  // blink::LocalDOMWindow::DispatchMessageEventWithOriginCheck (needed for
  // messages sent within the same process - e.g. same-site, cross-origin),
  // but this check needs to be duplicated below in case the recipient renderer
  // process got compromised (i.e. in case the renderer-side check may be
  // bypassed).
  if (!params.target_origin.empty()) {
    url::Origin target_origin =
        url::Origin::Create(GURL(base::UTF16ToUTF8(params.target_origin)));

    // Renderer should send either an empty string (this is how "*" is expressed
    // in the IPC) or a valid, non-opaque origin.  OTOH, there are no security
    // implications here - the message payload needs to be protected from an
    // unintended recipient, not from the sender.
    DCHECK(!target_origin.opaque());

    // While the postMessage was in flight, the target might have navigated away
    // to another origin.  In this case, the postMessage should be silently
    // dropped.
    if (target_origin != target_rfh->GetLastCommittedOrigin())
      return;
  }

  // TODO(lukasza): Move opaque-ness check into ChildProcessSecurityPolicyImpl.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (params.source_origin != base::UTF8ToUTF16("null") &&
      !policy->CanAccessDataForOrigin(GetProcess()->GetID(),
                                      GURL(params.source_origin))) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFPH_POST_MESSAGE_INVALID_SOURCE_ORIGIN);
    return;
  }

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

  base::Optional<base::UnguessableToken> translated_source_token;
  base::string16 source_origin = params.source_origin;
  base::string16 target_origin = params.target_origin;
  blink::TransferableMessage message = std::move(params.message->data);

  // If there is a source_routing_id, translate it to the frame token of the
  // equivalent RenderFrameProxyHost in the target process.
  if (params.source_routing_id != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* source_rfh = RenderFrameHostImpl::FromID(
        GetProcess()->GetID(), params.source_routing_id);
    if (source_rfh) {
      // https://crbug.com/822958: If the postMessage is going to a descendant
      // frame, ensure that any pending visual properties such as size are sent
      // to the target frame before the postMessage, as sites might implicitly
      // be relying on this ordering.
      bool target_is_descendant_of_source = false;
      for (RenderFrameHost* rfh = target_rfh; rfh; rfh = rfh->GetParent()) {
        if (rfh == source_rfh) {
          target_is_descendant_of_source = true;
          break;
        }
      }
      if (target_is_descendant_of_source) {
        target_rfh->GetRenderWidgetHost()
            ->SynchronizeVisualPropertiesIgnoringPendingAck();
      }

      // Ensure that we have a swapped-out RVH and proxy for the source frame
      // in the target SiteInstance. If it doesn't exist, create it on demand
      // and also create its opener chain, since that will also be accessible
      // to the target page.
      target_rfh->delegate()->EnsureOpenerProxiesExist(source_rfh);

      // Transfer user activation state in the frame tree in the browser and
      // the non-source and non-target renderer processes when
      // |transfer_user_activation| is true. We are making an expriment with
      // dynamic delegation of "autoplay" capability using this post message
      // approach to transfer user activation.
      // TODO(lanwei): we should transfer user activation state only when
      // |source_rfh| and |target_rfh| are in the same frame tree.
      bool should_transfer_user_activation =
          base::FeatureList::IsEnabled(
              features::kUserActivationPostMessageTransfer) &&
          message.transfer_user_activation;
      should_transfer_user_activation =
          should_transfer_user_activation || message.allow_autoplay;
      if (should_transfer_user_activation &&
          source_rfh->frame_tree_node()->HasTransientUserActivation()) {
        target_rfh->frame_tree_node()->TransferUserActivationFrom(source_rfh);
      }

      // If the message source is a cross-process subframe, its proxy will only
      // be created in --site-per-process mode.  If the proxy wasn't created,
      // set the source routing ID to MSG_ROUTING_NONE (see
      // https://crbug.com/485520 for discussion on why this is ok).
      RenderFrameProxyHost* source_proxy_in_target_site_instance =
          source_rfh->frame_tree_node()
              ->render_manager()
              ->GetRenderFrameProxyHost(target_site_instance);
      if (source_proxy_in_target_site_instance) {
        translated_source_token =
            source_proxy_in_target_site_instance->GetFrameToken();
      }
    }
  }

  target_rfh->PostMessageEvent(translated_source_token, source_origin,
                               target_origin, std::move(message));
}

void RenderFrameProxyHost::OnDidChangeOpener(int32_t opener_routing_id) {
  frame_tree_node_->render_manager()->DidChangeOpener(opener_routing_id,
                                                      GetSiteInstance());
}

void RenderFrameProxyHost::OnAdvanceFocus(blink::mojom::FocusType type,
                                          int32_t source_routing_id) {
  RenderFrameHostImpl* target_rfh =
      frame_tree_node_->render_manager()->current_frame_host();
  if (target_rfh->InsidePortal()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFPH_ADVANCE_FOCUS_INTO_PORTAL);
    return;
  }

  // Translate the source RenderFrameHost in this process to its equivalent
  // RenderFrameProxyHost in the target process.  This is needed for continuing
  // the focus traversal from correct place in a parent frame after one of its
  // child frames finishes its traversal.
  RenderFrameHostImpl* source_rfh =
      RenderFrameHostImpl::FromID(GetProcess()->GetID(), source_routing_id);
  RenderFrameProxyHost* source_proxy =
      source_rfh ? source_rfh->frame_tree_node()
                       ->render_manager()
                       ->GetRenderFrameProxyHost(target_rfh->GetSiteInstance())
                 : nullptr;

  target_rfh->AdvanceFocus(type, source_proxy);
  frame_tree_node_->current_frame_host()->delegate()->OnAdvanceFocus(
      source_rfh);
}

void RenderFrameProxyHost::DidFocusFrame() {
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node_->current_frame_host();

  // We need to handle this case due to a race, see documentation in
  // RenderFrameHostImpl::DidFocusFrame for more details.
  if (render_frame_host->InsidePortal())
    return;

  render_frame_host->delegate()->SetFocusedFrame(frame_tree_node_,
                                                 GetSiteInstance());
}

void RenderFrameProxyHost::CapturePaintPreviewOfCrossProcessSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {
  RenderFrameHostImpl* rfh = frame_tree_node_->current_frame_host();
  if (!rfh->is_active())
    return;
  rfh->delegate()->CapturePaintPreviewOfCrossProcessSubframe(clip_rect, guid,
                                                             rfh);
}

void RenderFrameProxyHost::SetIsInert(bool inert) {
  cross_process_frame_connector_->SetIsInert(inert);
}

bool RenderFrameProxyHost::IsInertForTesting() {
  return cross_process_frame_connector_->IsInert();
}

void RenderFrameProxyHost::OnPrintCrossProcessSubframe(const gfx::Rect& rect,
                                                       int document_cookie) {
  RenderFrameHostImpl* rfh = frame_tree_node_->current_frame_host();
  rfh->delegate()->PrintCrossProcessSubframe(rect, document_cookie, rfh);
}

blink::AssociatedInterfaceProvider*
RenderFrameProxyHost::GetRemoteAssociatedInterfacesTesting() {
  return GetRemoteAssociatedInterfaces();
}

}  // namespace content
