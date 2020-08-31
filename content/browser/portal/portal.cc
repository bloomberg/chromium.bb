// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal.h"

#include <unordered_map>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"

namespace content {

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(owner_render_frame_host)),
      owner_render_frame_host_(owner_render_frame_host),
      portal_token_(base::UnguessableToken::Create()) {}

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host,
               std::unique_ptr<WebContents> existing_web_contents)
    : Portal(owner_render_frame_host) {
  portal_contents_.SetOwned(std::move(existing_web_contents));
  portal_contents_->NotifyInsidePortal(true);
}

Portal::~Portal() {
  WebContentsImpl* outer_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
  devtools_instrumentation::PortalDetached(outer_contents_impl->GetMainFrame());
  Observe(nullptr);
}

// static
bool Portal::IsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kPortals) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

// static
void Portal::BindPortalHostReceiver(
    RenderFrameHostImpl* frame,
    mojo::PendingAssociatedReceiver<blink::mojom::PortalHost>
        pending_receiver) {
  if (!IsEnabled()) {
    mojo::ReportBadMessage(
        "blink.mojom.PortalHost can only be used if the Portals feature is "
        "enabled.");
    return;
  }

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));

  // This guards against the blink::mojom::PortalHost interface being used
  // outside the main frame of a Portal's guest.
  if (!web_contents || !web_contents->IsPortal() ||
      !frame->frame_tree_node()->IsMainFrame()) {
    mojo::ReportBadMessage(
        "blink.mojom.PortalHost can only be used by the the main frame of a "
        "Portal's guest.");
    return;
  }

  // This binding may already be bound to another request, and in such cases,
  // we rebind with the new request. An example scenario is a new document after
  // a portal navigation trying to create a connection, but the old document
  // hasn't been destroyed yet (and the pipe hasn't been closed).
  auto& receiver = web_contents->portal()->portal_host_receiver_;
  if (receiver.is_bound())
    receiver.reset();
  receiver.Bind(std::move(pending_receiver));
}

void Portal::Bind(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client) {
  DCHECK(!receiver_.is_bound());
  DCHECK(!client_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&Portal::Close, base::Unretained(this)));
  client_.Bind(std::move(client));
}

void Portal::DestroySelf() {
  // Deletes |this|.
  owner_render_frame_host_->DestroyPortal(this);
}

RenderFrameProxyHost* Portal::CreateProxyAndAttachPortal() {
  WebContentsImpl* outer_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));

  // Check if portal has already been attached.
  if (portal_contents_ && portal_contents_->GetOuterWebContents()) {
    mojo::ReportBadMessage(
        "Trying to attach a portal that has already been attached.");
    return nullptr;
  }

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider;
  auto interface_provider_receiver(
      interface_provider.InitWithNewPipeAndPassReceiver());

  // Create a FrameTreeNode in the outer WebContents to host the portal, in
  // response to the creation of a portal in the renderer process.
  FrameTreeNode* outer_node = outer_contents_impl->GetFrameTree()->AddFrame(
      owner_render_frame_host_, owner_render_frame_host_->GetProcess()->GetID(),
      owner_render_frame_host_->GetProcess()->GetNextRoutingID(),
      std::move(interface_provider_receiver),
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
          .InitWithNewPipeAndPassReceiver(),
      blink::mojom::TreeScopeType::kDocument, "", "", true,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
      blink::mojom::FrameOwnerElementType::kPortal);
  outer_node->AddObserver(this);

  bool web_contents_created = false;
  if (!portal_contents_) {
    // Create the Portal WebContents.
    WebContents::CreateParams params(outer_contents_impl->GetBrowserContext());
    portal_contents_.SetOwned(base::WrapUnique(
        static_cast<WebContentsImpl*>(WebContents::Create(params).release())));
    web_contents_created = true;
  }

  DCHECK(portal_contents_.OwnsContents());
  DCHECK_EQ(portal_contents_->portal(), this);
  DCHECK_EQ(portal_contents_->GetDelegate(), this);

  DCHECK(!is_closing_) << "Portal should not be shutting down when contents "
                          "ownership is yielded";
  outer_contents_impl->AttachInnerWebContents(
      portal_contents_.ReleaseOwnership(), outer_node->current_frame_host(),
      false /* is_full_page */);

  // If a cross-process navigation started while the predecessor was orphaned,
  // we need to create a view for the speculative RFH as well.
  if (RenderFrameHostImpl* speculative_rfh =
          portal_contents_->GetPendingMainFrame()) {
    if (RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
            speculative_rfh->GetView())) {
      view->Destroy();
    }
    portal_contents_->CreateRenderWidgetHostViewForRenderManager(
        speculative_rfh->render_view_host());
  }

  FrameTreeNode* frame_tree_node =
      portal_contents_->GetMainFrame()->frame_tree_node();
  RenderFrameProxyHost* proxy_host =
      frame_tree_node->render_manager()->GetProxyToOuterDelegate();
  proxy_host->SetRenderFrameProxyCreated(true);
  portal_contents_->ReattachToOuterWebContentsFrame();

  if (web_contents_created)
    PortalWebContentsCreated(portal_contents_.get());

  devtools_instrumentation::PortalAttached(outer_contents_impl->GetMainFrame());

  return proxy_host;
}

void Portal::Close() {
  if (is_closing_)
    return;
  is_closing_ = true;
  receiver_.reset();

  // If the contents is unowned, it would need to be properly detached from the
  // WebContentsTreeNode before it can be cleanly closed. Otherwise a race is
  // possible.
  if (!portal_contents_.OwnsContents()) {
    DestroySelf();  // Deletes this.
    return;
  }

  portal_contents_->ClosePage();
}

void Portal::Navigate(const GURL& url,
                      blink::mojom::ReferrerPtr referrer,
                      NavigateCallback callback) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Portal::Navigate tried to use non-HTTP protocol.");
    DestroySelf();  // Also deletes |this|.
    return;
  }

  GURL out_validated_url = url;
  owner_render_frame_host_->GetSiteInstance()->GetProcess()->FilterURL(
      false, &out_validated_url);

  FrameTreeNode* portal_root = portal_contents_->GetFrameTree()->root();
  RenderFrameHostImpl* portal_frame = portal_root->current_frame_host();

  // TODO(lfg): Figure out download policies for portals.
  // https://github.com/WICG/portals/issues/150
  NavigationDownloadPolicy download_policy;

  // Navigations in portals do not affect the host's session history. Upon
  // activation, only the portal's last committed entry is merged with the
  // host's session history. Hence, a portal maintaining multiple session
  // history entries is not useful and would introduce unnecessary complexity.
  // We therefore have portal navigations done with replacement, so that we only
  // have one entry at a time.
  // TODO(mcnee): There are still corner cases (e.g. using window.opener when
  // it's remote) that could cause a portal to navigate without replacement.
  // Fix this so that we can enforce this as an invariant.
  constexpr bool should_replace_entry = true;

  // TODO(https://crbug.com/1074422): It is possible for a portal to be
  // navigated by a frame other than the owning frame. Find a way to route the
  // correct initiator of the portal navigation to this call.
  portal_root->navigator()->NavigateFromFrameProxy(
      portal_frame, url,
      GlobalFrameRoutingId(owner_render_frame_host_->GetProcess()->GetID(),
                           owner_render_frame_host_->GetRoutingID()),
      owner_render_frame_host_->GetLastCommittedOrigin(),
      owner_render_frame_host_->GetSiteInstance(),
      mojo::ConvertTo<Referrer>(referrer), ui::PAGE_TRANSITION_LINK,
      should_replace_entry, download_policy, "GET", nullptr, "", nullptr, false,
      base::nullopt);

  std::move(callback).Run();
}

namespace {
void FlushTouchEventQueues(RenderWidgetHostImpl* host) {
  host->input_router()->FlushTouchEventQueue();
  std::unique_ptr<RenderWidgetHostIterator> child_widgets =
      host->GetEmbeddedRenderWidgetHosts();
  while (RenderWidgetHost* child_widget = child_widgets->GetNextHost())
    FlushTouchEventQueues(static_cast<RenderWidgetHostImpl*>(child_widget));
}

void CreateRenderWidgetHostViewForUnattachedPredecessor(
    WebContentsImpl* predecessor) {
  if (RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
          predecessor->GetMainFrame()->GetView())) {
    view->Destroy();
  }
  predecessor->CreateRenderWidgetHostViewForRenderManager(
      predecessor->GetRenderViewHost());

  if (RenderFrameHostImpl* speculative_rfh =
          predecessor->GetPendingMainFrame()) {
    if (RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
            speculative_rfh->GetView())) {
      view->Destroy();
    }
    predecessor->CreateRenderWidgetHostViewForRenderManager(
        speculative_rfh->render_view_host());
  }
}

// Copies |predecessor_contents|'s navigation entries to
// |activated_contents|. |activated_contents| will have its last committed entry
// combined with the entries in |predecessor_contents|. |predecessor_contents|
// will only keep its last committed entry.
// TODO(914108): This currently only covers the basic cases for history
// traversal across portal activations. The design is still being discussed.
void TakeHistoryForActivation(WebContentsImpl* activated_contents,
                              WebContentsImpl* predecessor_contents) {
  NavigationControllerImpl& activated_controller =
      activated_contents->GetController();
  NavigationControllerImpl& predecessor_controller =
      predecessor_contents->GetController();

  // Activation would have discarded any pending entry in the host contents.
  DCHECK(!predecessor_controller.GetPendingEntry());

  // TODO(mcnee): Once we enforce that a portal contents does not build up its
  // own history, make this DCHECK that we only have a single committed entry,
  // possibly with a new pending entry.
  if (activated_controller.GetPendingEntryIndex() != -1) {
    return;
  }
  DCHECK(activated_controller.GetLastCommittedEntry());
  DCHECK(activated_controller.CanPruneAllButLastCommitted());

  // TODO(mcnee): Allow for portal activations to replace history entries and to
  // traverse existing history entries.
  activated_controller.CopyStateFromAndPrune(&predecessor_controller,
                                             false /* replace_entry */);

  // The predecessor may be adopted as a portal, so it should now only have a
  // single committed entry.
  DCHECK(predecessor_controller.CanPruneAllButLastCommitted());
  predecessor_controller.PruneAllButLastCommitted();
}
}  // namespace

void Portal::Activate(blink::TransferableMessage data,
                      ActivateCallback callback) {
  WebContentsImpl* outer_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));

  if (outer_contents->portal()) {
    mojo::ReportBadMessage("Portal::Activate called on nested portal");
    DestroySelf();  // Also deletes |this|.
    return;
  }

  DCHECK(owner_render_frame_host_->IsCurrent())
      << "The binding should have been closed when the portal's outer "
         "FrameTreeNode was deleted due to swap out.";

  DCHECK(portal_contents_);
  NavigationControllerImpl& portal_controller =
      portal_contents_->GetController();
  NavigationControllerImpl& predecessor_controller =
      outer_contents->GetController();

  // If no navigation has yet committed in the portal, it cannot be activated as
  // this would lead to an empty tab contents (without even an about:blank).
  if (portal_controller.GetLastCommittedEntryIndex() < 0) {
    std::move(callback).Run(
        blink::mojom::PortalActivateResult::kRejectedDueToPortalNotReady);
    return;
  }
  DCHECK(predecessor_controller.GetLastCommittedEntry());

  // Error pages and interstitials may not host portals due to the HTTP(S)
  // restriction.
  DCHECK_EQ(PAGE_TYPE_NORMAL,
            predecessor_controller.GetLastCommittedEntry()->GetPageType());
  DCHECK(!predecessor_controller.GetTransientEntry());

  // If the portal is showing an error page, reject activation.
  if (portal_controller.GetLastCommittedEntry()->GetPageType() !=
      PAGE_TYPE_NORMAL) {
    std::move(callback).Run(
        blink::mojom::PortalActivateResult::kRejectedDueToErrorInPortal);
    return;
  }
  DCHECK(!portal_controller.GetTransientEntry());

  // If a navigation in the main frame is occurring, stop it if possible and
  // reject the activation if it's too late or if an ongoing navigation takes
  // precedence. There are a few cases here:
  // - a different RenderFrameHost has been assigned to the FrameTreeNode
  // - the same RenderFrameHost is being used, but it is committing a navigation
  // - the FrameTreeNode holds a navigation request that can't turn back but has
  //   not yet been handed off to a RenderFrameHost
  FrameTreeNode* outer_root_node = owner_render_frame_host_->frame_tree_node();
  NavigationRequest* outer_navigation = outer_root_node->navigation_request();
  const bool has_user_gesture =
      owner_render_frame_host_->HasTransientUserActivation();

  // WILL_PROCESS_RESPONSE is slightly early: it happens
  // immediately before READY_TO_COMMIT (unless it's deferred), but
  // WILL_PROCESS_RESPONSE is easier to hook for tests using a
  // NavigationThrottle.
  if (owner_render_frame_host_->HasPendingCommitNavigation() ||
      (outer_navigation &&
       outer_navigation->state() >= NavigationRequest::WILL_PROCESS_RESPONSE) ||
      Navigator::ShouldIgnoreIncomingRendererRequest(outer_navigation,
                                                     has_user_gesture)) {
    std::move(callback).Run(blink::mojom::PortalActivateResult::
                                kRejectedDueToPredecessorNavigation);
    return;
  }
  outer_root_node->navigator()->CancelNavigation(outer_root_node);

  DCHECK(!is_closing_) << "Portal should not be shutting down when contents "
                          "ownership is yielded";

  WebContentsDelegate* delegate = outer_contents->GetDelegate();
  std::unique_ptr<WebContents> successor_contents;

  if (portal_contents_->GetOuterWebContents()) {
    FrameTreeNode* outer_frame_tree_node = FrameTreeNode::GloballyFindByID(
        portal_contents_->GetOuterDelegateFrameTreeNodeId());
    outer_frame_tree_node->RemoveObserver(this);
    successor_contents = portal_contents_->DetachFromOuterWebContents();
    owner_render_frame_host_->RemoveChild(outer_frame_tree_node);
  } else {
    // Portals created for predecessor pages during activation may not be
    // attached to an outer WebContents, and may not have an outer frame tree
    // node created (i.e. CreateProxyAndAttachPortal isn't called). In this
    // case, we can skip a few of the detachment steps above.
    CreateRenderWidgetHostViewForUnattachedPredecessor(portal_contents_.get());
    successor_contents = portal_contents_.ReleaseOwnership();
  }
  DCHECK(!portal_contents_.OwnsContents());

  // This assumes that the delegate keeps the new contents alive long enough to
  // notify it of activation, at least.
  WebContentsImpl* successor_contents_raw =
      static_cast<WebContentsImpl*>(successor_contents.get());

  auto* outer_contents_main_frame_view = static_cast<RenderWidgetHostViewBase*>(
      outer_contents->GetMainFrame()->GetView());
  DCHECK(!outer_contents->GetPendingMainFrame());
  auto* portal_contents_main_frame_view =
      static_cast<RenderWidgetHostViewBase*>(
          successor_contents_raw->GetMainFrame()->GetView());

  std::vector<std::unique_ptr<ui::TouchEvent>> touch_events;

  if (outer_contents_main_frame_view) {
    // Take fallback contents from previous WebContents so that the activation
    // is smooth without flashes.
    portal_contents_main_frame_view->TakeFallbackContentFrom(
        outer_contents_main_frame_view);
    touch_events =
        outer_contents_main_frame_view->ExtractAndCancelActiveTouches();
    FlushTouchEventQueues(outer_contents_main_frame_view->host());
  }

  TakeHistoryForActivation(successor_contents_raw, outer_contents);

  devtools_instrumentation::PortalActivated(outer_contents->GetMainFrame());
  successor_contents_raw->set_portal(nullptr);

  std::unique_ptr<WebContents> predecessor_web_contents =
      delegate->ActivatePortalWebContents(outer_contents,
                                          std::move(successor_contents));
  CHECK_EQ(predecessor_web_contents.get(), outer_contents);

  if (outer_contents_main_frame_view) {
    portal_contents_main_frame_view->TransferTouches(touch_events);
    // Takes ownership of SyntheticGestureController from the predecessor's
    // RenderWidgetHost. This allows the controller to continue sending events
    // to the new RenderWidgetHostView.
    portal_contents_main_frame_view->host()->TakeSyntheticGestureController(
        outer_contents_main_frame_view->host());
    outer_contents_main_frame_view->Destroy();
  }

  // These pointers are cleared so that they don't dangle in the event this
  // object isn't immediately deleted. It isn't done sooner because
  // ActivatePortalWebContents misbehaves if the WebContents doesn't appear to
  // be a portal at that time.
  portal_contents_.Clear();

  successor_contents_raw->GetMainFrame()->OnPortalActivated(
      std::move(predecessor_web_contents), std::move(data),
      std::move(callback));
  // Notifying of activation happens later than ActivatePortalWebContents so
  // that it is observed after predecessor_web_contents has been moved into a
  // portal.
  DCHECK(outer_contents->IsPortal());
  successor_contents_raw->DidActivatePortal(outer_contents);
}

void Portal::PostMessageToGuest(
    blink::TransferableMessage message,
    const base::Optional<url::Origin>& target_origin) {
  portal_contents_->GetMainFrame()->ForwardMessageFromHost(
      std::move(message), owner_render_frame_host_->GetLastCommittedOrigin(),
      target_origin);
}

void Portal::PostMessageToHost(
    blink::TransferableMessage message,
    const base::Optional<url::Origin>& target_origin) {
  DCHECK(GetPortalContents());
  if (target_origin) {
    if (target_origin != owner_render_frame_host_->GetLastCommittedOrigin())
      return;
  }
  client().ForwardMessageFromGuest(
      std::move(message),
      GetPortalContents()->GetMainFrame()->GetLastCommittedOrigin(),
      target_origin);
}

void Portal::OnFrameTreeNodeDestroyed(FrameTreeNode* frame_tree_node) {
  // Listens for the deletion of the FrameTreeNode corresponding to this portal
  // in the outer WebContents (not the FrameTreeNode of the document containing
  // it). If that outer FrameTreeNode goes away, this Portal should stop
  // accepting new messages and go away as well.

  Close();  // May delete |this|.
}

void Portal::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  // Even though this object is owned (via unique_ptr by the RenderFrameHost),
  // explicitly observing RenderFrameDeleted is necessary because it happens
  // earlier than the destructor, notably before Mojo teardown.
  if (render_frame_host == owner_render_frame_host_)
    DestroySelf();  // Deletes |this|.
}

void Portal::WebContentsDestroyed() {
  DestroySelf();  // Deletes |this|.
}

void Portal::LoadingStateChanged(WebContents* source,
                                 bool to_different_document) {
  DCHECK_EQ(source, portal_contents_.get());
  if (!source->IsLoading())
    client_->DispatchLoadEvent();
}

void Portal::PortalWebContentsCreated(WebContents* portal_web_contents) {
  WebContentsImpl* outer_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
  DCHECK(outer_contents->GetDelegate());
  outer_contents->GetDelegate()->PortalWebContentsCreated(portal_web_contents);
}

void Portal::CloseContents(WebContents* web_contents) {
  DCHECK_EQ(web_contents, portal_contents_.get());
  DestroySelf();  // Deletes |this|.
}

WebContents* Portal::GetResponsibleWebContents(WebContents* web_contents) {
  return WebContents::FromRenderFrameHost(owner_render_frame_host_);
}

base::UnguessableToken Portal::GetDevToolsFrameToken() const {
  return portal_contents_->GetMainFrame()->GetDevToolsFrameToken();
}

WebContentsImpl* Portal::GetPortalContents() {
  return portal_contents_.get();
}

Portal::WebContentsHolder::WebContentsHolder(Portal* portal)
    : portal_(portal) {}

Portal::WebContentsHolder::~WebContentsHolder() {
  Clear();
}

bool Portal::WebContentsHolder::OwnsContents() const {
  DCHECK(!owned_contents_ || contents_ == owned_contents_.get());
  return owned_contents_ != nullptr;
}

void Portal::WebContentsHolder::SetUnowned(WebContentsImpl* web_contents) {
  Clear();
  contents_ = web_contents;
  contents_->SetDelegate(portal_);
  contents_->set_portal(portal_);
}

void Portal::WebContentsHolder::SetOwned(
    std::unique_ptr<WebContents> web_contents) {
  SetUnowned(static_cast<WebContentsImpl*>(web_contents.get()));
  owned_contents_ = std::move(web_contents);
}

void Portal::WebContentsHolder::Clear() {
  if (!contents_)
    return;

  FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
      contents_->GetOuterDelegateFrameTreeNodeId());
  if (outer_node)
    outer_node->RemoveObserver(portal_);

  if (contents_->GetDelegate() == portal_)
    contents_->SetDelegate(nullptr);
  contents_->set_portal(nullptr);

  contents_ = nullptr;
  owned_contents_ = nullptr;
}

}  // namespace content
