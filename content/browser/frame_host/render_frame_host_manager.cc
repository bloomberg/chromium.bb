// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/cross_site_transferring_request.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"

namespace content {

// static
bool RenderFrameHostManager::ClearRFHsPendingShutdown(FrameTreeNode* node) {
  node->render_manager()->pending_delete_hosts_.clear();
  return true;
}

RenderFrameHostManager::RenderFrameHostManager(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderViewHostDelegate* render_view_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    Delegate* delegate)
    : frame_tree_node_(frame_tree_node),
      delegate_(delegate),
      cross_navigation_pending_(false),
      render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      interstitial_page_(nullptr),
      should_reuse_web_ui_(false),
      weak_factory_(this) {
  DCHECK(frame_tree_node_);
}

RenderFrameHostManager::~RenderFrameHostManager() {
  if (pending_render_frame_host_)
    UnsetPendingRenderFrameHost();

  if (speculative_render_frame_host_)
    UnsetSpeculativeRenderFrameHost();

  if (render_frame_host_ &&
      render_frame_host_->GetSiteInstance()->active_frame_count() <= 1U) {
    ShutdownRenderFrameProxyHostsInSiteInstance(
        render_frame_host_->GetSiteInstance()->GetId());
  }

  // Delete any RenderFrameProxyHosts and swapped out RenderFrameHosts.
  // It is important to delete those prior to deleting the current
  // RenderFrameHost, since the CrossProcessFrameConnector (owned by
  // RenderFrameProxyHost) points to the RenderWidgetHostView associated with
  // the current RenderFrameHost and uses it during its destructor.
  STLDeleteValues(&proxy_hosts_);

  // Release the WebUI prior to resetting the current RenderFrameHost, as the
  // WebUI accesses the RenderFrameHost during cleanup.
  web_ui_.reset();

  // We should always have a current RenderFrameHost except in some tests.
  SetRenderFrameHost(scoped_ptr<RenderFrameHostImpl>());
}

void RenderFrameHostManager::Init(BrowserContext* browser_context,
                                  SiteInstance* site_instance,
                                  int view_routing_id,
                                  int frame_routing_id) {
  // Create a RenderViewHost and RenderFrameHost, once we have an instance.  It
  // is important to immediately give this SiteInstance to a RenderViewHost so
  // that the SiteInstance is ref counted.
  if (!site_instance)
    site_instance = SiteInstance::Create(browser_context);

  int flags = delegate_->IsHidden() ? CREATE_RF_HIDDEN : 0;
  SetRenderFrameHost(CreateRenderFrameHost(site_instance, view_routing_id,
                                           frame_routing_id, flags));

  // Notify the delegate of the creation of the current RenderFrameHost.
  // Do this only for subframes, as the main frame case is taken care of by
  // WebContentsImpl::Init.
  if (!frame_tree_node_->IsMainFrame()) {
    delegate_->NotifySwappedFromRenderManager(
        nullptr, render_frame_host_.get(), false);
  }

  // Keep track of renderer processes as they start to shut down or are
  // crashed/killed.
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSING,
                 NotificationService::AllSources());
}

RenderViewHostImpl* RenderFrameHostManager::current_host() const {
  if (!render_frame_host_)
    return NULL;
  return render_frame_host_->render_view_host();
}

RenderViewHostImpl* RenderFrameHostManager::pending_render_view_host() const {
  if (!pending_render_frame_host_)
    return NULL;
  return pending_render_frame_host_->render_view_host();
}

RenderWidgetHostView* RenderFrameHostManager::GetRenderWidgetHostView() const {
  if (interstitial_page_)
    return interstitial_page_->GetView();
  if (render_frame_host_)
    return render_frame_host_->GetView();
  return nullptr;
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToParent() {
  if (frame_tree_node_->IsMainFrame())
    return NULL;

  RenderFrameProxyHostMap::iterator iter =
      proxy_hosts_.find(frame_tree_node_->parent()
                            ->render_manager()
                            ->current_frame_host()
                            ->GetSiteInstance()
                            ->GetId());
  if (iter == proxy_hosts_.end())
    return NULL;

  return iter->second;
}

void RenderFrameHostManager::SetPendingWebUI(const GURL& url, int bindings) {
  pending_web_ui_ = CreateWebUI(url, bindings);
  pending_and_current_web_ui_.reset();
}

scoped_ptr<WebUIImpl> RenderFrameHostManager::CreateWebUI(const GURL& url,
                                                          int bindings) {
  scoped_ptr<WebUIImpl> new_web_ui(delegate_->CreateWebUIForRenderManager(url));

  // If we have assigned (zero or more) bindings to this NavigationEntry in the
  // past, make sure we're not granting it different bindings than it had
  // before.  If so, note it and don't give it any bindings, to avoid a
  // potential privilege escalation.
  if (new_web_ui && bindings != NavigationEntryImpl::kInvalidBindings &&
      new_web_ui->GetBindings() != bindings) {
    RecordAction(base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
    return nullptr;
  }
  return new_web_ui.Pass();
}

RenderFrameHostImpl* RenderFrameHostManager::Navigate(
    const NavigationEntryImpl& entry) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager:Navigate",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  // Create a pending RenderFrameHost to use for the navigation.
  RenderFrameHostImpl* dest_render_frame_host = UpdateStateForNavigate(
      entry.GetURL(), entry.source_site_instance(), entry.site_instance(),
      entry.GetTransitionType(),
      entry.restore_type() != NavigationEntryImpl::RESTORE_NONE,
      entry.IsViewSourceMode(), entry.transferred_global_request_id(),
      entry.bindings());
  if (!dest_render_frame_host)
    return NULL;  // We weren't able to create a pending render frame host.

  // If the current render_frame_host_ isn't live, we should create it so
  // that we don't show a sad tab while the dest_render_frame_host fetches
  // its first page.  (Bug 1145340)
  if (dest_render_frame_host != render_frame_host_ &&
      !render_frame_host_->IsRenderFrameLive()) {
    // Note: we don't call InitRenderView here because we are navigating away
    // soon anyway, and we don't have the NavigationEntry for this host.
    delegate_->CreateRenderViewForRenderManager(
        render_frame_host_->render_view_host(), MSG_ROUTING_NONE,
        MSG_ROUTING_NONE, frame_tree_node_->IsMainFrame());
  }

  // If the renderer crashed, then try to create a new one to satisfy this
  // navigation request.
  if (!dest_render_frame_host->IsRenderFrameLive()) {
    // Instruct the destination render frame host to set up a Mojo connection
    // with the new render frame if necessary.  Note that this call needs to
    // occur before initializing the RenderView; the flow of creating the
    // RenderView can cause browser-side code to execute that expects the this
    // RFH's ServiceRegistry to be initialized (e.g., if the site is a WebUI
    // site that is handled via Mojo, then Mojo WebUI code in //chrome will
    // add a service to this RFH's ServiceRegistry).
    dest_render_frame_host->SetUpMojoIfNeeded();

    // Recreate the opener chain.
    int opener_route_id = delegate_->CreateOpenerRenderViewsForRenderManager(
        dest_render_frame_host->GetSiteInstance());
    if (!InitRenderView(dest_render_frame_host->render_view_host(),
                        opener_route_id,
                        MSG_ROUTING_NONE,
                        frame_tree_node_->IsMainFrame()))
      return NULL;

    // Now that we've created a new renderer, be sure to hide it if it isn't
    // our primary one.  Otherwise, we might crash if we try to call Show()
    // on it later.
    if (dest_render_frame_host != render_frame_host_) {
      if (dest_render_frame_host->GetView())
        dest_render_frame_host->GetView()->Hide();
    } else {
      // TODO(nasko): This is a very ugly hack. The Chrome extensions process
      // manager still uses NotificationService and expects to see a
      // RenderViewHost changed notification after WebContents and
      // RenderFrameHostManager are completely initialized. This should be
      // removed once the process manager moves away from NotificationService.
      // See https://crbug.com/462682.
      delegate_->NotifyMainFrameSwappedFromRenderManager(
          nullptr, render_frame_host_->render_view_host());
    }
  }

  // If entry includes the request ID of a request that is being transferred,
  // the destination render frame will take ownership, so release ownership of
  // the request.
  if (cross_site_transferring_request_.get() &&
      cross_site_transferring_request_->request_id() ==
          entry.transferred_global_request_id()) {
    cross_site_transferring_request_->ReleaseRequest();
  }

  return dest_render_frame_host;
}

void RenderFrameHostManager::Stop() {
  render_frame_host_->Stop();

  // If we are cross-navigating, we should stop the pending renderers.  This
  // will lead to a DidFailProvisionalLoad, which will properly destroy them.
  if (cross_navigation_pending_) {
    pending_render_frame_host_->Send(new FrameMsg_Stop(
        pending_render_frame_host_->GetRoutingID()));
  }
}

void RenderFrameHostManager::SetIsLoading(bool is_loading) {
  render_frame_host_->render_view_host()->SetIsLoading(is_loading);
  if (pending_render_frame_host_)
    pending_render_frame_host_->render_view_host()->SetIsLoading(is_loading);
}

bool RenderFrameHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  // If we're waiting for a close ACK, then the tab should close whether there's
  // a navigation in progress or not.  Unfortunately, we also need to check for
  // cases that we arrive here with no navigation in progress, since there are
  // some tab closure paths that don't set is_waiting_for_close_ack to true.
  // TODO(creis): Clean this up in http://crbug.com/418266.
  if (!cross_navigation_pending_ ||
      render_frame_host_->render_view_host()->is_waiting_for_close_ack())
    return true;

  // We should always have a pending RFH when there's a cross-process navigation
  // in progress.  Sanity check this for http://crbug.com/276333.
  CHECK(pending_render_frame_host_);

  // Unload handlers run in the background, so we should never get an
  // unresponsiveness warning for them.
  CHECK(!render_frame_host_->IsWaitingForUnloadACK());

  // If the tab becomes unresponsive during beforeunload while doing a
  // cross-site navigation, proceed with the navigation.  (This assumes that
  // the pending RenderFrameHost is still responsive.)
  if (render_frame_host_->IsWaitingForBeforeUnloadACK()) {
    // Haven't gotten around to starting the request, because we're still
    // waiting for the beforeunload handler to finish.  We'll pretend that it
    // did finish, to let the navigation proceed.  Note that there's a danger
    // that the beforeunload handler will later finish and possibly return
    // false (meaning the navigation should not proceed), but we'll ignore it
    // in this case because it took too long.
    if (pending_render_frame_host_->are_navigations_suspended()) {
      pending_render_frame_host_->SetNavigationsSuspended(
          false, base::TimeTicks::Now());
    }
  }
  return false;
}

void RenderFrameHostManager::OnBeforeUnloadACK(
    bool for_cross_site_transition,
    bool proceed,
    const base::TimeTicks& proceed_time) {
  if (for_cross_site_transition) {
    DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserSideNavigation));
    // Ignore if we're not in a cross-site navigation.
    if (!cross_navigation_pending_)
      return;

    if (proceed) {
      // Ok to unload the current page, so proceed with the cross-site
      // navigation.  Note that if navigations are not currently suspended, it
      // might be because the renderer was deemed unresponsive and this call was
      // already made by ShouldCloseTabOnUnresponsiveRenderer.  In that case, it
      // is ok to do nothing here.
      if (pending_render_frame_host_ &&
          pending_render_frame_host_->are_navigations_suspended()) {
        pending_render_frame_host_->SetNavigationsSuspended(false,
                                                            proceed_time);
      }
    } else {
      // Current page says to cancel.
      CancelPending();
      cross_navigation_pending_ = false;
    }
  } else {
    // Non-cross site transition means closing the entire tab.
    bool proceed_to_fire_unload;
    delegate_->BeforeUnloadFiredFromRenderManager(proceed, proceed_time,
                                                  &proceed_to_fire_unload);

    if (proceed_to_fire_unload) {
      // If we're about to close the tab and there's a pending RFH, cancel it.
      // Otherwise, if the navigation in the pending RFH completes before the
      // close in the current RFH, we'll lose the tab close.
      if (pending_render_frame_host_) {
        CancelPending();
        cross_navigation_pending_ = false;
      }

      // This is not a cross-site navigation, the tab is being closed.
      render_frame_host_->render_view_host()->ClosePage();
    }
  }
}

void RenderFrameHostManager::OnCrossSiteResponse(
    RenderFrameHostImpl* pending_render_frame_host,
    const GlobalRequestID& global_request_id,
    scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry) {
  // We should only get here for transfer navigations.  Most cross-process
  // navigations can just continue and wait to run the unload handler (by
  // swapping out) when the new navigation commits.
  CHECK(cross_site_transferring_request.get());

  // A transfer should only have come from our pending or current RFH.
  // TODO(creis): We need to handle the case that the pending RFH has changed
  // in the mean time, while this was being posted from the IO thread.  We
  // should probably cancel the request in that case.
  DCHECK(pending_render_frame_host == pending_render_frame_host_ ||
         pending_render_frame_host == render_frame_host_);

  // Store the transferring request so that we can release it if the transfer
  // navigation matches.
  cross_site_transferring_request_ = cross_site_transferring_request.Pass();

  // Sanity check that the params are for the correct frame and process.
  // These should match the RenderFrameHost that made the request.
  // If it started as a cross-process navigation via OpenURL, this is the
  // pending one.  If it wasn't cross-process until the transfer, this is the
  // current one.
  int render_frame_id = pending_render_frame_host_ ?
      pending_render_frame_host_->GetRoutingID() :
      render_frame_host_->GetRoutingID();
  DCHECK_EQ(render_frame_id, pending_render_frame_host->GetRoutingID());
  int process_id = pending_render_frame_host_ ?
      pending_render_frame_host_->GetProcess()->GetID() :
      render_frame_host_->GetProcess()->GetID();
  DCHECK_EQ(process_id, global_request_id.child_id);

  // Treat the last URL in the chain as the destination and the remainder as
  // the redirect chain.
  CHECK(transfer_url_chain.size());
  GURL transfer_url = transfer_url_chain.back();
  std::vector<GURL> rest_of_chain = transfer_url_chain;
  rest_of_chain.pop_back();

  // We don't know whether the original request had |user_action| set to true.
  // However, since we force the navigation to be in the current tab, it
  // doesn't matter.
  pending_render_frame_host->frame_tree_node()->navigator()->RequestTransferURL(
      pending_render_frame_host, transfer_url, nullptr, rest_of_chain, referrer,
      page_transition, CURRENT_TAB, global_request_id,
      should_replace_current_entry, true);

  // The transferring request was only needed during the RequestTransferURL
  // call, so it is safe to clear at this point.
  cross_site_transferring_request_.reset();
}

void RenderFrameHostManager::OnDeferredAfterResponseStarted(
    const GlobalRequestID& global_request_id,
    RenderFrameHostImpl* pending_render_frame_host) {
  DCHECK(!response_started_id_.get());

  response_started_id_.reset(new GlobalRequestID(global_request_id));
}

void RenderFrameHostManager::ResumeResponseDeferredAtStart() {
  DCHECK(response_started_id_.get());

  RenderProcessHostImpl* process =
      static_cast<RenderProcessHostImpl*>(render_frame_host_->GetProcess());
  process->ResumeResponseDeferredAtStart(*response_started_id_);

  render_frame_host_->ClearPendingTransitionRequestData();

  response_started_id_.reset();
}

void RenderFrameHostManager::ClearNavigationTransitionData() {
  render_frame_host_->ClearPendingTransitionRequestData();
}

void RenderFrameHostManager::DidNavigateFrame(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture) {
  CommitPendingIfNecessary(render_frame_host, was_caused_by_user_gesture);

  // Make sure any dynamic changes to this frame's sandbox flags that were made
  // prior to navigation take effect.
  CommitPendingSandboxFlags();
}

void RenderFrameHostManager::CommitPendingIfNecessary(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    if (render_frame_host == speculative_render_frame_host_.get()) {
      CommitPending();
    } else if (render_frame_host == render_frame_host_.get()) {
      // TODO(carlosk): this code doesn't properly handle in-page navigation or
      // interwoven navigation requests.
      DCHECK(!speculative_render_frame_host_);
    } else {
      // No one else should be sending us a DidNavigate in this state.
      DCHECK(false);
    }
    DCHECK(!speculative_render_frame_host_);
    return;
  }

  if (!cross_navigation_pending_) {
    DCHECK(!pending_render_frame_host_);

    // We should only hear this from our current renderer.
    DCHECK_EQ(render_frame_host_, render_frame_host);

    // Even when there is no pending RVH, there may be a pending Web UI.
    if (pending_web_ui())
      CommitPending();
    return;
  }

  if (render_frame_host == pending_render_frame_host_) {
    // The pending cross-site navigation completed, so show the renderer.
    CommitPending();
  } else if (render_frame_host == render_frame_host_) {
    if (was_caused_by_user_gesture) {
      // A navigation in the original page has taken place.  Cancel the pending
      // one. Only do it for user gesture originated navigations to prevent
      // page doing any shenanigans to prevent user from navigating.
      // See https://code.google.com/p/chromium/issues/detail?id=75195
      CancelPending();
      cross_navigation_pending_ = false;
    }
  } else {
    // No one else should be sending us DidNavigate in this state.
    DCHECK(false);
  }
}

void RenderFrameHostManager::DidDisownOpener(
    RenderFrameHost* render_frame_host) {
  // Notify all RenderFrameHosts but the one that notified us. This is necessary
  // in case a process swap has occurred while the message was in flight.
  for (RenderFrameProxyHostMap::iterator iter = proxy_hosts_.begin();
       iter != proxy_hosts_.end();
       ++iter) {
    DCHECK_NE(iter->second->GetSiteInstance(),
              current_frame_host()->GetSiteInstance());
    iter->second->DisownOpener();
  }

  if (render_frame_host_.get() != render_frame_host)
    render_frame_host_->DisownOpener();

  if (pending_render_frame_host_ &&
      pending_render_frame_host_.get() != render_frame_host) {
    pending_render_frame_host_->DisownOpener();
  }
}

void RenderFrameHostManager::CommitPendingSandboxFlags() {
  // Return early if there were no pending sandbox flags updates.
  if (!frame_tree_node_->CommitPendingSandboxFlags())
    return;

  // Sandbox flags updates can only happen when the frame has a parent.
  CHECK(frame_tree_node_->parent());

  // Notify all of the frame's proxies about updated sandbox flags, excluding
  // the parent process since it already knows the latest flags.
  SiteInstance* parent_site_instance =
      frame_tree_node_->parent()->current_frame_host()->GetSiteInstance();
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() != parent_site_instance) {
      pair.second->Send(new FrameMsg_DidUpdateSandboxFlags(
          pair.second->GetRoutingID(),
          frame_tree_node_->current_replication_state().sandbox_flags));
    }
  }
}

void RenderFrameHostManager::RendererProcessClosing(
    RenderProcessHost* render_process_host) {
  // Remove any swapped out RVHs from this process, so that we don't try to
  // swap them back in while the process is exiting.  Start by finding them,
  // since there could be more than one.
  std::list<int> ids_to_remove;
  // Do not remove proxies in the dead process that still have active frame
  // count though, we just reset them to be uninitialized.
  std::list<int> ids_to_keep;
  for (RenderFrameProxyHostMap::iterator iter = proxy_hosts_.begin();
       iter != proxy_hosts_.end();
       ++iter) {
    RenderFrameProxyHost* proxy = iter->second;
    if (proxy->GetProcess() != render_process_host)
      continue;

    if (static_cast<SiteInstanceImpl*>(proxy->GetSiteInstance())
            ->active_frame_count() >= 1U) {
      ids_to_keep.push_back(iter->first);
    } else {
      ids_to_remove.push_back(iter->first);
    }
  }

  // Now delete them.
  while (!ids_to_remove.empty()) {
    delete proxy_hosts_[ids_to_remove.back()];
    proxy_hosts_.erase(ids_to_remove.back());
    ids_to_remove.pop_back();
  }

  while (!ids_to_keep.empty()) {
    frame_tree_node_->frame_tree()->ForEach(
        base::Bind(
            &RenderFrameHostManager::ResetProxiesInSiteInstance,
            ids_to_keep.back()));
    ids_to_keep.pop_back();
  }
}

void RenderFrameHostManager::SwapOutOldFrame(
    scoped_ptr<RenderFrameHostImpl> old_render_frame_host) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::SwapOutOldFrame",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());

  // Tell the renderer to suppress any further modal dialogs so that we can swap
  // it out.  This must be done before canceling any current dialog, in case
  // there is a loop creating additional dialogs.
  // TODO(creis): Handle modal dialogs in subframe processes.
  old_render_frame_host->render_view_host()->SuppressDialogsUntilSwapOut();

  // Now close any modal dialogs that would prevent us from swapping out.  This
  // must be done separately from SwapOut, so that the PageGroupLoadDeferrer is
  // no longer on the stack when we send the SwapOut message.
  delegate_->CancelModalDialogsForRenderManager();

  // If the old RFH is not live, just return as there is no further work to do.
  // It will be deleted and there will be no proxy created.
  int32 old_site_instance_id =
      old_render_frame_host->GetSiteInstance()->GetId();
  if (!old_render_frame_host->IsRenderFrameLive()) {
    ShutdownRenderFrameProxyHostsInSiteInstance(old_site_instance_id);
    return;
  }

  // If there are no active frames besides this one, we can delete the old
  // RenderFrameHost once it runs its unload handler, without replacing it with
  // a proxy.
  size_t active_frame_count =
      old_render_frame_host->GetSiteInstance()->active_frame_count();
  if (active_frame_count <= 1) {
    // Tell the old RenderFrameHost to swap out, with no proxy to replace it.
    old_render_frame_host->SwapOut(NULL, true);
    MoveToPendingDeleteHosts(old_render_frame_host.Pass());

    // Also clear out any proxies from this SiteInstance, in case this was the
    // last one keeping other proxies alive.
    ShutdownRenderFrameProxyHostsInSiteInstance(old_site_instance_id);

    return;
  }

  // Otherwise there are active views and we need a proxy for the old RFH.
  // (There should not be one yet.)
  CHECK(!GetRenderFrameProxyHost(old_render_frame_host->GetSiteInstance()));
  RenderFrameProxyHost* proxy = new RenderFrameProxyHost(
      old_render_frame_host->GetSiteInstance(), frame_tree_node_);
  CHECK(proxy_hosts_.insert(std::make_pair(old_site_instance_id, proxy)).second)
      << "Inserting a duplicate item.";

  // Tell the old RenderFrameHost to swap out and be replaced by the proxy.
  old_render_frame_host->SwapOut(proxy, true);

  // SwapOut creates a RenderFrameProxy, so set the proxy to be initialized.
  proxy->set_render_frame_proxy_created(true);

  bool is_main_frame = frame_tree_node_->IsMainFrame();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess) &&
      !is_main_frame) {
    // In --site-per-process, subframes delete their RFH rather than storing it
    // in the proxy.  Schedule it for deletion once the SwapOutACK comes in.
    // TODO(creis): This will be the default when we remove swappedout://.
    MoveToPendingDeleteHosts(old_render_frame_host.Pass());
  } else {
    // We shouldn't get here for subframes, since we only swap subframes when
    // --site-per-process is used.
    DCHECK(is_main_frame);

    // The old RenderFrameHost will stay alive inside the proxy so that existing
    // JavaScript window references to it stay valid.
    proxy->TakeFrameHostOwnership(old_render_frame_host.Pass());
  }
}

void RenderFrameHostManager::DiscardUnusedFrame(
    scoped_ptr<RenderFrameHostImpl> render_frame_host) {
  // TODO(carlosk): this code is very similar to what can be found in
  // SwapOutOldFrame and we should see that these are unified at some point.

  // If the SiteInstance for the pending RFH is being used by others don't
  // delete the RFH. Just swap it out and it can be reused at a later point.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  if (site_instance->HasSite() && site_instance->active_frame_count() > 1) {
    // Any currently suspended navigations are no longer needed.
    render_frame_host->CancelSuspendedNavigations();

    RenderFrameProxyHost* proxy =
        new RenderFrameProxyHost(site_instance, frame_tree_node_);
    proxy_hosts_[site_instance->GetId()] = proxy;

    // Check if the RenderFrameHost is already swapped out, to avoid swapping it
    // out again.
    if (!render_frame_host->is_swapped_out())
      render_frame_host->SwapOut(proxy, false);

    if (frame_tree_node_->IsMainFrame())
      proxy->TakeFrameHostOwnership(render_frame_host.Pass());
  } else {
    // We won't be coming back, so delete this one.
    render_frame_host.reset();
  }
}

void RenderFrameHostManager::MoveToPendingDeleteHosts(
    scoped_ptr<RenderFrameHostImpl> render_frame_host) {
  // |render_frame_host| will be deleted when its SwapOut ACK is received, or
  // when the timer times out, or when the RFHM itself is deleted (whichever
  // comes first).
  pending_delete_hosts_.push_back(
      linked_ptr<RenderFrameHostImpl>(render_frame_host.release()));
}

bool RenderFrameHostManager::IsPendingDeletion(
    RenderFrameHostImpl* render_frame_host) {
  for (const auto& rfh : pending_delete_hosts_) {
    if (rfh == render_frame_host)
      return true;
  }
  return false;
}

bool RenderFrameHostManager::DeleteFromPendingList(
    RenderFrameHostImpl* render_frame_host) {
  for (RFHPendingDeleteList::iterator iter = pending_delete_hosts_.begin();
       iter != pending_delete_hosts_.end();
       iter++) {
    if (*iter == render_frame_host) {
      pending_delete_hosts_.erase(iter);
      return true;
    }
  }
  return false;
}

void RenderFrameHostManager::ResetProxyHosts() {
  STLDeleteValues(&proxy_hosts_);
}

// PlzNavigate
void RenderFrameHostManager::BeginNavigation(const NavigationRequest& request) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  // Clean up any state in case there's an ongoing navigation.
  // TODO(carlosk): remove this cleanup here once we properly cancel ongoing
  // navigations.
  CleanUpNavigation();

  RenderFrameHostImpl* dest_rfh = GetFrameHostForNavigation(request);
  DCHECK(dest_rfh);
}

// PlzNavigate
RenderFrameHostImpl* RenderFrameHostManager::GetFrameHostForNavigation(
    const NavigationRequest& request) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));

  SiteInstance* current_site_instance = render_frame_host_->GetSiteInstance();

  scoped_refptr<SiteInstance> dest_site_instance = GetSiteInstanceForNavigation(
      request.common_params().url, request.source_site_instance(),
      request.dest_site_instance(), request.common_params().transition,
      request.restore_type() != NavigationEntryImpl::RESTORE_NONE,
      request.is_view_source());
  // The appropriate RenderFrameHost to commit the navigation.
  RenderFrameHostImpl* navigation_rfh = nullptr;

  // TODO(carlosk): do not swap processes for renderer initiated navigations
  // (see crbug.com/440266).
  if (current_site_instance == dest_site_instance.get() ||
      (!frame_tree_node_->IsMainFrame() &&
       !base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kSitePerProcess))) {
    // Reuse the current RFH if its SiteInstance matches the the navigation's
    // or if this is a subframe navigation. We only swap RFHs for subframes when
    // --site-per-process is enabled.
    CleanUpNavigation();
    navigation_rfh = render_frame_host_.get();

    // As SiteInstances are the same, check if the WebUI should be reused.
    const NavigationEntry* current_navigation_entry =
        delegate_->GetLastCommittedNavigationEntryForRenderManager();
    bool should_reuse_web_ui_ = ShouldReuseWebUI(current_navigation_entry,
                                                 request.common_params().url);
    if (!should_reuse_web_ui_) {
      speculative_web_ui_ = CreateWebUI(request.common_params().url,
                                        request.bindings());
      // Make sure the current RenderViewHost has the right bindings.
      if (speculative_web_ui() &&
          !render_frame_host_->GetProcess()->IsIsolatedGuest()) {
        render_frame_host_->render_view_host()->AllowBindings(
            speculative_web_ui()->GetBindings());
      }
    }
  } else {
    // If the SiteInstance for the final URL doesn't match the one from the
    // speculatively created RenderFrameHost, create a new RenderFrameHost using
    // this new SiteInstance.
    if (!speculative_render_frame_host_ ||
        speculative_render_frame_host_->GetSiteInstance() !=
            dest_site_instance.get()) {
      CleanUpNavigation();
      bool success = CreateSpeculativeRenderFrameHost(
          request.common_params().url, current_site_instance,
          dest_site_instance.get(), request.bindings());
      DCHECK(success);
    }
    DCHECK(speculative_render_frame_host_);
    navigation_rfh = speculative_render_frame_host_.get();

    // Check if our current RFH is live.
    if (!render_frame_host_->IsRenderFrameLive()) {
      // The current RFH is not live.  There's no reason to sit around with a
      // sad tab or a newly created RFH while we wait for the navigation to
      // complete. Just switch to the speculative RFH now and go back to non
      // cross-navigating (Note that we don't care about on{before}unload
      // handlers if the current RFH isn't live.)
      CommitPending();
    }
  }
  DCHECK(navigation_rfh &&
         (navigation_rfh == render_frame_host_.get() ||
          navigation_rfh == speculative_render_frame_host_.get()));

  // If the RenderFrame that needs to navigate is not live (its process was just
  // created or has crashed), initialize it.
  if (!navigation_rfh->IsRenderFrameLive()) {
    // Recreate the opener chain.
    int opener_route_id = delegate_->CreateOpenerRenderViewsForRenderManager(
        navigation_rfh->GetSiteInstance());
    if (!InitRenderView(navigation_rfh->render_view_host(), opener_route_id,
                        MSG_ROUTING_NONE, frame_tree_node_->IsMainFrame())) {
      return nullptr;
    }
  }

  cross_navigation_pending_ = navigation_rfh != render_frame_host_.get();
  return navigation_rfh;
}

// PlzNavigate
void RenderFrameHostManager::CleanUpNavigation() {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  speculative_web_ui_.reset();
  should_reuse_web_ui_ = false;
  if (speculative_render_frame_host_)
    DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost());
}

// PlzNavigate
scoped_ptr<RenderFrameHostImpl>
RenderFrameHostManager::UnsetSpeculativeRenderFrameHost() {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  speculative_render_frame_host_->GetProcess()->RemovePendingView();
  return speculative_render_frame_host_.Pass();
}

void RenderFrameHostManager::OnDidStartLoading() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_DidStartLoading(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnDidStopLoading() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(new FrameMsg_DidStopLoading(pair.second->GetRoutingID()));
  }
}

void RenderFrameHostManager::OnDidUpdateName(const std::string& name) {
  // The window.name message may be sent outside of --site-per-process when
  // report_frame_name_changes renderer preference is set (used by
  // WebView).  Don't send the update to proxies in those cases.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return;

  for (const auto& pair : proxy_hosts_) {
    pair.second->Send(
        new FrameMsg_DidUpdateName(pair.second->GetRoutingID(), name));
  }
}

void RenderFrameHostManager::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case NOTIFICATION_RENDERER_PROCESS_CLOSING:
      RendererProcessClosing(
          Source<RenderProcessHost>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}

// static
bool RenderFrameHostManager::ClearProxiesInSiteInstance(
    int32 site_instance_id,
    FrameTreeNode* node) {
  RenderFrameProxyHostMap::iterator iter =
      node->render_manager()->proxy_hosts_.find(site_instance_id);
  if (iter != node->render_manager()->proxy_hosts_.end()) {
    RenderFrameProxyHost* proxy = iter->second;
    // Delete the proxy.  If it is for a main frame (and thus the RFH is stored
    // in the proxy) and it was still pending swap out, move the RFH to the
    // pending deletion list first.
    if (node->IsMainFrame() &&
        proxy->render_frame_host()->rfh_state() ==
        RenderFrameHostImpl::STATE_PENDING_SWAP_OUT) {
      scoped_ptr<RenderFrameHostImpl> swapped_out_rfh =
          proxy->PassFrameHostOwnership();
      node->render_manager()->MoveToPendingDeleteHosts(swapped_out_rfh.Pass());
    }
    delete proxy;
    node->render_manager()->proxy_hosts_.erase(site_instance_id);
  }

  return true;
}

// static.
bool RenderFrameHostManager::ResetProxiesInSiteInstance(int32 site_instance_id,
                                                        FrameTreeNode* node) {
  RenderFrameProxyHostMap::iterator iter =
      node->render_manager()->proxy_hosts_.find(site_instance_id);
  if (iter != node->render_manager()->proxy_hosts_.end())
    iter->second->set_render_frame_proxy_created(false);

  return true;
}

bool RenderFrameHostManager::ShouldTransitionCrossSite() {
  // True for --site-per-process, which overrides both kSingleProcess and
  // kProcessPerTab.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return true;

  // False in the single-process mode, as it makes RVHs to accumulate
  // in swapped_out_hosts_.
  // True if we are using process-per-site-instance (default) or
  // process-per-site (kProcessPerSite).
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kProcessPerTab);
}

bool RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation(
    const GURL& current_effective_url,
    bool current_is_view_source_mode,
    SiteInstance* new_site_instance,
    const GURL& new_effective_url,
    bool new_is_view_source_mode) const {
  // If new_entry already has a SiteInstance, assume it is correct.  We only
  // need to force a swap if it is in a different BrowsingInstance.
  if (new_site_instance) {
    return !new_site_instance->IsRelatedSiteInstance(
        render_frame_host_->GetSiteInstance());
  }

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).  Any time we return true,
  // the new_entry will be rendered in a new SiteInstance AND BrowsingInstance.
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();

  // Don't force a new BrowsingInstance for debug URLs that are handled in the
  // renderer process, like javascript: or chrome://crash.
  if (IsRendererDebugURL(new_effective_url))
    return false;

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          render_frame_host_->GetProcess()->GetID()) ||
      WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, current_effective_url)) {
    // If so, force a swap if destination is not an acceptable URL for Web UI.
    // Here, data URLs are never allowed.
    if (!WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            browser_context, new_effective_url)) {
      return true;
    }
  } else {
    // Force a swap if it's a Web UI URL.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, new_effective_url)) {
      return true;
    }
  }

  // Check with the content client as well.  Important to pass
  // current_effective_url here, which uses the SiteInstance's site if there is
  // no current_entry.
  if (GetContentClient()->browser()->ShouldSwapBrowsingInstancesForNavigation(
          render_frame_host_->GetSiteInstance(),
          current_effective_url, new_effective_url)) {
    return true;
  }

  // We can't switch a RenderView between view source and non-view source mode
  // without screwing up the session history sometimes (when navigating between
  // "view-source:http://foo.com/" and "http://foo.com/", Blink doesn't treat
  // it as a new navigation). So require a BrowsingInstance switch.
  if (current_is_view_source_mode != new_is_view_source_mode)
    return true;

  return false;
}

bool RenderFrameHostManager::ShouldReuseWebUI(
    const NavigationEntry* current_entry,
    const GURL& new_url) const {
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  return current_entry && web_ui_.get() &&
      (WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          controller.GetBrowserContext(), current_entry->GetURL()) ==
       WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          controller.GetBrowserContext(), new_url));
}

SiteInstance* RenderFrameHostManager::GetSiteInstanceForNavigation(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance,
    ui::PageTransition transition,
    bool dest_is_restore,
    bool dest_is_view_source_mode) {
  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();
  SiteInstance* new_instance = current_instance;

  // We do not currently swap processes for navigations in webview tag guests.
  if (current_instance->GetSiteURL().SchemeIs(kGuestScheme))
    return current_instance;

  // Determine if we need a new BrowsingInstance for this entry.  If true, this
  // implies that it will get a new SiteInstance (and likely process), and that
  // other tabs in the current BrowsingInstance will be unable to script it.
  // This is used for cases that require a process swap even in the
  // process-per-tab model, such as WebUI pages.
  // TODO(clamy): Remove the dependency on the current entry.
  const NavigationEntry* current_entry =
      delegate_->GetLastCommittedNavigationEntryForRenderManager();
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();
  const GURL& current_effective_url = current_entry ?
      SiteInstanceImpl::GetEffectiveURL(browser_context,
                                        current_entry->GetURL()) :
      render_frame_host_->GetSiteInstance()->GetSiteURL();
  bool current_is_view_source_mode = current_entry ?
      current_entry->IsViewSourceMode() : dest_is_view_source_mode;
  bool force_swap = ShouldSwapBrowsingInstancesForNavigation(
      current_effective_url,
      current_is_view_source_mode,
      dest_instance,
      SiteInstanceImpl::GetEffectiveURL(browser_context, dest_url),
      dest_is_view_source_mode);
  if (ShouldTransitionCrossSite() || force_swap) {
    new_instance = GetSiteInstanceForURL(
        dest_url, source_instance, current_instance, dest_instance,
        transition, dest_is_restore, dest_is_view_source_mode, force_swap);
  }

  // If force_swap is true, we must use a different SiteInstance.  If we didn't,
  // we would have two RenderFrameHosts in the same SiteInstance and the same
  // frame, resulting in page_id conflicts for their NavigationEntries.
  if (force_swap)
    CHECK_NE(new_instance, current_instance);
  return new_instance;
}

SiteInstance* RenderFrameHostManager::GetSiteInstanceForURL(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* current_instance,
    SiteInstance* dest_instance,
    ui::PageTransition transition,
    bool dest_is_restore,
    bool dest_is_view_source_mode,
    bool force_browsing_instance_swap) {
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  BrowserContext* browser_context = controller.GetBrowserContext();

  // If the entry has an instance already we should use it.
  if (dest_instance) {
    // If we are forcing a swap, this should be in a different BrowsingInstance.
    if (force_browsing_instance_swap) {
      CHECK(!dest_instance->IsRelatedSiteInstance(
                render_frame_host_->GetSiteInstance()));
    }
    return dest_instance;
  }

  // If a swap is required, we need to force the SiteInstance AND
  // BrowsingInstance to be different ones, using CreateForURL.
  if (force_browsing_instance_swap)
    return SiteInstance::CreateForURL(browser_context, dest_url);

  // (UGLY) HEURISTIC, process-per-site only:
  //
  // If this navigation is generated, then it probably corresponds to a search
  // query.  Given that search results typically lead to users navigating to
  // other sites, we don't really want to use the search engine hostname to
  // determine the site instance for this navigation.
  //
  // NOTE: This can be removed once we have a way to transition between
  //       RenderViews in response to a link click.
  //
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessPerSite) &&
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED)) {
    return current_instance;
  }

  SiteInstanceImpl* current_site_instance =
      static_cast<SiteInstanceImpl*>(current_instance);

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored or it's a Web UI as described below.
  if (!current_site_instance->HasSite()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the current_instance has a site, because for now, we
    // want to compare against the current URL and not the SiteInstance's site.
    // In this case, there is no current URL, so comparing against the site is
    // ok.  See additional comments below.)
    //
    // Also, if the URL should use process-per-site mode and there is an
    // existing process for the site, we should use it.  We can call
    // GetRelatedSiteInstance() for this, which will eagerly set the site and
    // thus use the correct process.
    bool use_process_per_site =
        RenderProcessHost::ShouldUseProcessPerSite(browser_context, dest_url) &&
        RenderProcessHostImpl::GetProcessHostForSite(browser_context, dest_url);
    if (current_site_instance->HasRelatedSiteInstance(dest_url) ||
        use_process_per_site) {
      return current_site_instance->GetRelatedSiteInstance(dest_url);
    }

    // For extensions, Web UI URLs (such as the new tab page), and apps we do
    // not want to use the current_instance if it has no site, since it will
    // have a RenderProcessHost of PRIV_NORMAL.  Create a new SiteInstance for
    // this URL instead (with the correct process type).
    if (current_site_instance->HasWrongProcessForURL(dest_url))
      return current_site_instance->GetRelatedSiteInstance(dest_url);

    // View-source URLs must use a new SiteInstance and BrowsingInstance.
    // TODO(nasko): This is the same condition as later in the function. This
    // should be taken into account when refactoring this method as part of
    // http://crbug.com/123007.
    if (dest_is_view_source_mode)
      return SiteInstance::CreateForURL(browser_context, dest_url);

    // If we are navigating from a blank SiteInstance to a WebUI, make sure we
    // create a new SiteInstance.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, dest_url)) {
        return SiteInstance::CreateForURL(browser_context, dest_url);
    }

    // Normally the "site" on the SiteInstance is set lazily when the load
    // actually commits. This is to support better process sharing in case
    // the site redirects to some other site: we want to use the destination
    // site in the site instance.
    //
    // In the case of session restore, as it loads all the pages immediately
    // we need to set the site first, otherwise after a restore none of the
    // pages would share renderers in process-per-site.
    //
    // The embedder can request some urls never to be assigned to SiteInstance
    // through the ShouldAssignSiteForURL() content client method, so that
    // renderers created for particular chrome urls (e.g. the chrome-native://
    // scheme) can be reused for subsequent navigations in the same WebContents.
    // See http://crbug.com/386542.
    if (dest_is_restore &&
        GetContentClient()->browser()->ShouldAssignSiteForURL(dest_url)) {
      current_site_instance->SetSite(dest_url);
    }

    return current_site_instance;
  }

  // Otherwise, only create a new SiteInstance for a cross-site navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = current_instance->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationEntry* current_entry = controller.GetLastCommittedEntry();
  if (interstitial_page_) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    current_entry = controller.GetEntryAtOffset(-1);
  }

  // View-source URLs must use a new SiteInstance and BrowsingInstance.
  // We don't need a swap when going from view-source to a debug URL like
  // chrome://crash, however.
  // TODO(creis): Refactor this method so this duplicated code isn't needed.
  // See http://crbug.com/123007.
  if (current_entry &&
      current_entry->IsViewSourceMode() != dest_is_view_source_mode &&
      !IsRendererDebugURL(dest_url)) {
    return SiteInstance::CreateForURL(browser_context, dest_url);
  }

  // Use the source SiteInstance in case of data URLs or about:blank pages,
  // because the content is then controlled and/or scriptable by the source
  // SiteInstance.
  GURL about_blank(url::kAboutBlankURL);
  if (source_instance &&
      (dest_url == about_blank || dest_url.scheme() == url::kDataScheme))
    return source_instance;

  // Use the current SiteInstance for same site navigations, as long as the
  // process type is correct.  (The URL may have been installed as an app since
  // the last time we visited it.)
  const GURL& current_url =
      GetCurrentURLForSiteInstance(current_instance, current_entry);
  if (SiteInstance::IsSameWebSite(browser_context, current_url, dest_url) &&
      !current_site_instance->HasWrongProcessForURL(dest_url)) {
    return current_instance;
  }

  // Start the new renderer in a new SiteInstance, but in the current
  // BrowsingInstance.  It is important to immediately give this new
  // SiteInstance to a RenderViewHost (if it is different than our current
  // SiteInstance), so that it is ref counted.  This will happen in
  // CreateRenderView.
  return current_instance->GetRelatedSiteInstance(dest_url);
}

const GURL& RenderFrameHostManager::GetCurrentURLForSiteInstance(
    SiteInstance* current_instance, NavigationEntry* current_entry) {
  // If this is a subframe that is potentially out of process from its parent,
  // don't consider using current_entry's url for SiteInstance selection, since
  // current_entry's url is for the main frame and may be in a different site
  // than this frame.
  // TODO(creis): Remove this when we can check the FrameNavigationEntry's url.
  // See http://crbug.com/369654
  if (!frame_tree_node_->IsMainFrame() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return frame_tree_node_->current_url();

  // If there is no last non-interstitial entry (and current_instance already
  // has a site), then we must have been opened from another tab.  We want
  // to compare against the URL of the page that opened us, but we can't
  // get to it directly.  The best we can do is check against the site of
  // the SiteInstance.  This will be correct when we intercept links and
  // script-based navigations, but for now, it could place some pages in a
  // new process unnecessarily.  We should only hit this case if a page tries
  // to open a new tab to an interstitial-inducing URL, and then navigates
  // the page to a different same-site URL.  (This seems very unlikely in
  // practice.)
  if (current_entry)
    return current_entry->GetURL();
  return current_instance->GetSiteURL();
}

void RenderFrameHostManager::CreatePendingRenderFrameHost(
    SiteInstance* old_instance,
    SiteInstance* new_instance,
    bool is_main_frame) {
  int create_render_frame_flags = 0;
  if (is_main_frame)
    create_render_frame_flags |= CREATE_RF_FOR_MAIN_FRAME_NAVIGATION;

  if (delegate_->IsHidden())
    create_render_frame_flags |= CREATE_RF_HIDDEN;

  int opener_route_id = CreateOpenerRenderViewsIfNeeded(
      old_instance, new_instance, &create_render_frame_flags);

  if (pending_render_frame_host_)
    CancelPending();

  // Create a non-swapped-out RFH with the given opener.
  pending_render_frame_host_ =
      CreateRenderFrame(new_instance, pending_web_ui(), opener_route_id,
                        create_render_frame_flags, nullptr);
}

int RenderFrameHostManager::CreateOpenerRenderViewsIfNeeded(
    SiteInstance* old_instance,
    SiteInstance* new_instance,
    int* create_render_frame_flags) {
  int opener_route_id = MSG_ROUTING_NONE;
  if (new_instance->IsRelatedSiteInstance(old_instance)) {
    opener_route_id =
        delegate_->CreateOpenerRenderViewsForRenderManager(new_instance);
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSitePerProcess)) {
      // Ensure that the frame tree has RenderFrameProxyHosts for the new
      // SiteInstance in all nodes except the current one.
      frame_tree_node_->frame_tree()->CreateProxiesForSiteInstance(
          frame_tree_node_, new_instance);
      // RenderFrames in different processes from their parent RenderFrames
      // in the frame tree require RenderWidgets for rendering and processing
      // input events.
      if (frame_tree_node_->parent() &&
          frame_tree_node_->parent()->current_frame_host()->GetSiteInstance() !=
              new_instance)
        *create_render_frame_flags |= CREATE_RF_NEEDS_RENDER_WIDGET_HOST;
    }
  }
  return opener_route_id;
}

scoped_ptr<RenderFrameHostImpl> RenderFrameHostManager::CreateRenderFrameHost(
    SiteInstance* site_instance,
    int view_routing_id,
    int frame_routing_id,
    int flags) {
  if (frame_routing_id == MSG_ROUTING_NONE)
    frame_routing_id = site_instance->GetProcess()->GetNextRoutingID();

  bool swapped_out = !!(flags & CREATE_RF_SWAPPED_OUT);
  bool hidden = !!(flags & CREATE_RF_HIDDEN);

  // Create a RVH for main frames, or find the existing one for subframes.
  FrameTree* frame_tree = frame_tree_node_->frame_tree();
  RenderViewHostImpl* render_view_host = nullptr;
  if (frame_tree_node_->IsMainFrame()) {
    render_view_host = frame_tree->CreateRenderViewHost(
        site_instance, view_routing_id, frame_routing_id, swapped_out, hidden);
  } else {
    render_view_host = frame_tree->GetRenderViewHost(site_instance);

    CHECK(render_view_host);
  }

  // TODO(creis): Pass hidden to RFH.
  scoped_ptr<RenderFrameHostImpl> render_frame_host = make_scoped_ptr(
      RenderFrameHostFactory::Create(
          site_instance, render_view_host, render_frame_delegate_,
          render_widget_delegate_, frame_tree, frame_tree_node_,
          frame_routing_id, flags).release());
  return render_frame_host.Pass();
}

// PlzNavigate
bool RenderFrameHostManager::CreateSpeculativeRenderFrameHost(
    const GURL& url,
    SiteInstance* old_instance,
    SiteInstance* new_instance,
    int bindings) {
  CHECK(new_instance);
  CHECK_NE(old_instance, new_instance);
  CHECK(!should_reuse_web_ui_);

  // Note: |speculative_web_ui_| must be initialized before starting the
  // |speculative_render_frame_host_| creation steps otherwise the WebUI
  // won't be properly initialized.
  speculative_web_ui_ = CreateWebUI(url, bindings);

  int create_render_frame_flags = 0;
  int opener_route_id =
      CreateOpenerRenderViewsIfNeeded(old_instance, new_instance,
                                      &create_render_frame_flags);

  if (frame_tree_node_->IsMainFrame())
    create_render_frame_flags |= CREATE_RF_FOR_MAIN_FRAME_NAVIGATION;
  if (delegate_->IsHidden())
    create_render_frame_flags |= CREATE_RF_HIDDEN;
  speculative_render_frame_host_ =
      CreateRenderFrame(new_instance, speculative_web_ui_.get(),
                        opener_route_id, create_render_frame_flags, nullptr);

  if (!speculative_render_frame_host_) {
    speculative_web_ui_.reset();
    return false;
  }
  return true;
}

scoped_ptr<RenderFrameHostImpl> RenderFrameHostManager::CreateRenderFrame(
    SiteInstance* instance,
    WebUIImpl* web_ui,
    int opener_route_id,
    int flags,
    int* view_routing_id_ptr) {
  bool swapped_out = !!(flags & CREATE_RF_SWAPPED_OUT);
  CHECK(instance);
  // Swapped out views should always be hidden.
  DCHECK(!swapped_out || (flags & CREATE_RF_HIDDEN));

  // TODO(nasko): Remove the following CHECK once cross-site navigation no
  // longer relies on swapped out RFH for the top-level frame.
  if (!frame_tree_node_->IsMainFrame())
    CHECK(!swapped_out);

  scoped_ptr<RenderFrameHostImpl> new_render_frame_host;
  bool success = true;
  if (view_routing_id_ptr)
    *view_routing_id_ptr = MSG_ROUTING_NONE;

  // We are creating a pending, speculative or swapped out RFH here. We should
  // never create it in the same SiteInstance as our current RFH.
  CHECK_NE(render_frame_host_->GetSiteInstance(), instance);

  // Check if we've already created an RFH for this SiteInstance.  If so, try
  // to re-use the existing one, which has already been initialized.  We'll
  // remove it from the list of proxy hosts below if it will be active.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy && proxy->render_frame_host()) {
    if (view_routing_id_ptr)
      *view_routing_id_ptr = proxy->GetRenderViewHost()->GetRoutingID();
    // Delete the existing RenderFrameProxyHost, but reuse the RenderFrameHost.
    // Prevent the process from exiting while we're trying to use it.
    if (!swapped_out) {
      new_render_frame_host = proxy->PassFrameHostOwnership();
      new_render_frame_host->GetProcess()->AddPendingView();

      proxy_hosts_.erase(instance->GetId());
      delete proxy;

      // When a new render view is created by the renderer, the new WebContents
      // gets a RenderViewHost in the SiteInstance of its opener WebContents.
      // If not used in the first navigation, this RVH is swapped out and is not
      // granted bindings, so we may need to grant them when swapping it in.
      if (web_ui && !new_render_frame_host->GetProcess()->IsIsolatedGuest()) {
        int required_bindings = web_ui->GetBindings();
        RenderViewHost* render_view_host =
            new_render_frame_host->render_view_host();
        if ((render_view_host->GetEnabledBindings() & required_bindings) !=
            required_bindings) {
          render_view_host->AllowBindings(required_bindings);
        }
      }
    }
  } else {
    // Create a new RenderFrameHost if we don't find an existing one.
    new_render_frame_host = CreateRenderFrameHost(instance, MSG_ROUTING_NONE,
                                                  MSG_ROUTING_NONE, flags);
    RenderViewHostImpl* render_view_host =
        new_render_frame_host->render_view_host();
    int proxy_routing_id = MSG_ROUTING_NONE;

    // Prevent the process from exiting while we're trying to navigate in it.
    // Otherwise, if the new RFH is swapped out already, store it.
    if (!swapped_out) {
      new_render_frame_host->GetProcess()->AddPendingView();
    } else {
      proxy = new RenderFrameProxyHost(
          new_render_frame_host->GetSiteInstance(), frame_tree_node_);
      proxy_hosts_[instance->GetId()] = proxy;
      proxy_routing_id = proxy->GetRoutingID();
      proxy->TakeFrameHostOwnership(new_render_frame_host.Pass());
    }

    success =
        InitRenderView(render_view_host, opener_route_id, proxy_routing_id,
                       !!(flags & CREATE_RF_FOR_MAIN_FRAME_NAVIGATION));
    if (success) {
      if (frame_tree_node_->IsMainFrame()) {
        // Don't show the main frame's view until we get a DidNavigate from it.
        // Only the RenderViewHost for the top-level RenderFrameHost has a
        // RenderWidgetHostView; RenderWidgetHosts for out-of-process iframes
        // will be created later and hidden.
        if (render_view_host->GetView())
          render_view_host->GetView()->Hide();
      } else if (!swapped_out) {
        // Init the RFH, so a RenderFrame is created in the renderer.
        DCHECK(new_render_frame_host.get());
        success = InitRenderFrame(new_render_frame_host.get());
      }
    }

    if (success) {
      if (view_routing_id_ptr)
        *view_routing_id_ptr = render_view_host->GetRoutingID();
    }
  }

  // Returns the new RFH if it isn't swapped out.
  if (success && !swapped_out) {
    DCHECK(new_render_frame_host->GetSiteInstance() == instance);
    return new_render_frame_host.Pass();
  }
  return nullptr;
}

int RenderFrameHostManager::CreateRenderFrameProxy(SiteInstance* instance) {
  // A RenderFrameProxyHost should never be created in the same SiteInstance as
  // the current RFH.
  CHECK(instance);
  CHECK_NE(instance, render_frame_host_->GetSiteInstance());

  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy)
    return proxy->GetRoutingID();

  proxy = new RenderFrameProxyHost(instance, frame_tree_node_);
  proxy_hosts_[instance->GetId()] = proxy;
  proxy->InitRenderFrameProxy();
  return proxy->GetRoutingID();
}

void RenderFrameHostManager::EnsureRenderViewInitialized(
    FrameTreeNode* source,
    RenderViewHostImpl* render_view_host,
    SiteInstance* instance) {
  DCHECK(frame_tree_node_->IsMainFrame());

  if (render_view_host->IsRenderViewLive())
    return;

  // Recreate the opener chain.
  int opener_route_id =
      delegate_->CreateOpenerRenderViewsForRenderManager(instance);
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  InitRenderView(render_view_host, opener_route_id, proxy->GetRoutingID(),
                 source->IsMainFrame());
}

bool RenderFrameHostManager::InitRenderView(
    RenderViewHostImpl* render_view_host,
    int opener_route_id,
    int proxy_routing_id,
    bool for_main_frame_navigation) {
  // We may have initialized this RenderViewHost for another RenderFrameHost.
  if (render_view_host->IsRenderViewLive())
    return true;

  // If the ongoing navigation is to a WebUI and the RenderView is not in a
  // guest process, tell the RenderViewHost about any bindings it will need
  // enabled.
  WebUIImpl* dest_web_ui = nullptr;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    dest_web_ui =
        should_reuse_web_ui_ ? web_ui_.get() : speculative_web_ui_.get();
  } else {
    dest_web_ui = pending_web_ui();
  }
  if (dest_web_ui && !render_view_host->GetProcess()->IsIsolatedGuest()) {
    render_view_host->AllowBindings(dest_web_ui->GetBindings());
  } else {
    // Ensure that we don't create an unprivileged RenderView in a WebUI-enabled
    // process unless it's swapped out.
    if (render_view_host->is_active()) {
      CHECK(!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                render_view_host->GetProcess()->GetID()));
    }
  }

  return delegate_->CreateRenderViewForRenderManager(render_view_host,
                                                     opener_route_id,
                                                     proxy_routing_id,
                                                     for_main_frame_navigation);
}

bool RenderFrameHostManager::InitRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host->IsRenderFrameLive())
    return true;

  int parent_routing_id = MSG_ROUTING_NONE;
  int proxy_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    parent_routing_id = frame_tree_node_->parent()->render_manager()->
        GetRoutingIdForSiteInstance(render_frame_host->GetSiteInstance());
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }
  // Check whether there is an existing proxy for this frame in this
  // SiteInstance. If there is, the new RenderFrame needs to be able to find
  // the proxy it is replacing, so that it can fully initialize itself.
  // NOTE: This is the only time that a RenderFrameProxyHost can be in the same
  // SiteInstance as its RenderFrameHost. This is only the case until the
  // RenderFrameHost commits, at which point it will replace and delete the
  // RenderFrameProxyHost.
  RenderFrameProxyHost* existing_proxy =
      GetRenderFrameProxyHost(render_frame_host->GetSiteInstance());
  if (existing_proxy) {
    proxy_routing_id = existing_proxy->GetRoutingID();
    CHECK_NE(proxy_routing_id, MSG_ROUTING_NONE);
    if (!existing_proxy->is_render_frame_proxy_live())
      existing_proxy->InitRenderFrameProxy();
  }
  return delegate_->CreateRenderFrameForRenderManager(render_frame_host,
                                                      parent_routing_id,
                                                      proxy_routing_id);
}

int RenderFrameHostManager::GetRoutingIdForSiteInstance(
    SiteInstance* site_instance) {
  if (render_frame_host_->GetSiteInstance() == site_instance)
    return render_frame_host_->GetRoutingID();

  RenderFrameProxyHostMap::iterator iter =
      proxy_hosts_.find(site_instance->GetId());
  if (iter != proxy_hosts_.end())
    return iter->second->GetRoutingID();

  return MSG_ROUTING_NONE;
}

void RenderFrameHostManager::CommitPending() {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CommitPending",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  bool browser_side_navigation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation);
  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared |pending_web_ui_| the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  if (!browser_side_navigation) {
    DCHECK(!speculative_web_ui_);
    // Next commit the Web UI, if any. Either replace |web_ui_| with
    // |pending_web_ui_|, or clear |web_ui_| if there is no pending WebUI, or
    // leave |web_ui_| as is if reusing it.
    DCHECK(!(pending_web_ui_.get() && pending_and_current_web_ui_.get()));
    if (pending_web_ui_) {
      web_ui_.reset(pending_web_ui_.release());
    } else if (!pending_and_current_web_ui_.get()) {
      web_ui_.reset();
    } else {
      DCHECK_EQ(pending_and_current_web_ui_.get(), web_ui_.get());
      pending_and_current_web_ui_.reset();
    }
  } else {
    // PlzNavigate
    if (!should_reuse_web_ui_)
      web_ui_.reset(speculative_web_ui_.release());
    DCHECK(!speculative_web_ui_);
  }

  // It's possible for the pending_render_frame_host_ to be nullptr when we
  // aren't crossing process boundaries. If so, we just needed to handle the Web
  // UI committing above and we're done.
  if (!pending_render_frame_host_ && !speculative_render_frame_host_) {
    if (will_focus_location_bar)
      delegate_->SetFocusToLocationBar(false);
    return;
  }

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = !will_focus_location_bar &&
                           render_frame_host_->GetView() &&
                           render_frame_host_->GetView()->HasFocus();

  bool is_main_frame = frame_tree_node_->IsMainFrame();

  // Swap in the pending or speculative frame and make it active. Also ensure
  // the FrameTree stays in sync.
  scoped_ptr<RenderFrameHostImpl> old_render_frame_host;
  if (!browser_side_navigation) {
    DCHECK(!speculative_render_frame_host_);
    old_render_frame_host =
        SetRenderFrameHost(pending_render_frame_host_.Pass());
  } else {
    // PlzNavigate
    DCHECK(speculative_render_frame_host_);
    old_render_frame_host =
        SetRenderFrameHost(speculative_render_frame_host_.Pass());
  }
  cross_navigation_pending_ = false;

  if (is_main_frame)
    render_frame_host_->render_view_host()->AttachToFrameTree();

  // The process will no longer try to exit, so we can decrement the count.
  render_frame_host_->GetProcess()->RemovePendingView();

  // Show the new view (or a sad tab) if necessary.
  bool new_rfh_has_view = !!render_frame_host_->GetView();
  if (!delegate_->IsHidden() && new_rfh_has_view) {
    // In most cases, we need to show the new view.
    render_frame_host_->GetView()->Show();
  }
  if (!new_rfh_has_view) {
    // If the view is gone, then this RenderViewHost died while it was hidden.
    // We ignored the RenderProcessGone call at the time, so we should send it
    // now to make sure the sad tab shows up, etc.
    DCHECK(!render_frame_host_->IsRenderFrameLive());
    DCHECK(!render_frame_host_->render_view_host()->IsRenderViewLive());
    delegate_->RenderProcessGoneFromRenderManager(
        render_frame_host_->render_view_host());
  }

  // For top-level frames, also hide the old RenderViewHost's view.
  // TODO(creis): As long as show/hide are on RVH, we don't want to hide on
  // subframe navigations or we will interfere with the top-level frame.
  if (is_main_frame && old_render_frame_host->render_view_host()->GetView())
    old_render_frame_host->render_view_host()->GetView()->Hide();

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager();

  if (will_focus_location_bar) {
    delegate_->SetFocusToLocationBar(false);
  } else if (focus_render_view && render_frame_host_->GetView()) {
    render_frame_host_->GetView()->Focus();
  }

  // Notify that we've swapped RenderFrameHosts. We do this before shutting down
  // the RFH so that we can clean up RendererResources related to the RFH first.
  delegate_->NotifySwappedFromRenderManager(
      old_render_frame_host.get(), render_frame_host_.get(), is_main_frame);

  // Swap out the old frame now that the new one is visible.
  // This will swap it out and then put it on the proxy list (if there are other
  // active views in its SiteInstance) or schedule it for deletion when the swap
  // out ack arrives (or immediately if the process isn't live).
  // In the --site-per-process case, old subframe RHFs are not kept alive inside
  // the proxy.
  SwapOutOldFrame(old_render_frame_host.Pass());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess) &&
      !is_main_frame) {
    // If this is a subframe, it should have a CrossProcessFrameConnector
    // created already.  Use it to link the new RFH's view to the proxy that
    // belongs to the parent frame's SiteInstance.
    // Note: We do this after swapping out the old RFH because that may create
    // the proxy we're looking for.
    RenderFrameProxyHost* proxy_to_parent = GetProxyToParent();
    if (proxy_to_parent) {
      proxy_to_parent->SetChildRWHView(render_frame_host_->GetView());
    }

    // Since the new RenderFrameHost is now committed, there must be no proxies
    // for its SiteInstance. Delete any existing ones.
    RenderFrameProxyHostMap::iterator iter =
        proxy_hosts_.find(render_frame_host_->GetSiteInstance()->GetId());
    if (iter != proxy_hosts_.end()) {
      delete iter->second;
      proxy_hosts_.erase(iter);
    }
  }

  // After all is done, there must never be a proxy in the list which has the
  // same SiteInstance as the current RenderFrameHost.
  CHECK(proxy_hosts_.find(render_frame_host_->GetSiteInstance()->GetId()) ==
        proxy_hosts_.end());
}

void RenderFrameHostManager::ShutdownRenderFrameProxyHostsInSiteInstance(
    int32 site_instance_id) {
  // First remove any swapped out RFH for this SiteInstance from our own list.
  ClearProxiesInSiteInstance(site_instance_id, frame_tree_node_);

  // Use the safe RenderWidgetHost iterator for now to find all RenderViewHosts
  // in the SiteInstance, then tell their respective FrameTrees to remove all
  // RenderFrameProxyHosts corresponding to them.
  // TODO(creis): Replace this with a RenderFrameHostIterator that protects
  // against use-after-frees if a later element is deleted before getting to it.
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (!widget->IsRenderView())
      continue;
    RenderViewHostImpl* rvh =
        static_cast<RenderViewHostImpl*>(RenderViewHost::From(widget));
    if (site_instance_id == rvh->GetSiteInstance()->GetId()) {
      // This deletes all RenderFrameHosts using the |rvh|, which then causes
      // |rvh| to Shutdown.
      FrameTree* tree = rvh->GetDelegate()->GetFrameTree();
      tree->ForEach(base::Bind(
          &RenderFrameHostManager::ClearProxiesInSiteInstance,
          site_instance_id));
    }
  }
}

RenderFrameHostImpl* RenderFrameHostManager::UpdateStateForNavigate(
    const GURL& dest_url,
    SiteInstance* source_instance,
    SiteInstance* dest_instance,
    ui::PageTransition transition,
    bool dest_is_restore,
    bool dest_is_view_source_mode,
    const GlobalRequestID& transferred_request_id,
    int bindings) {
  // If we are currently navigating cross-process, we want to get back to normal
  // and then navigate as usual.
  if (cross_navigation_pending_) {
    if (pending_render_frame_host_)
      CancelPending();
    cross_navigation_pending_ = false;
  }

  SiteInstance* current_instance = render_frame_host_->GetSiteInstance();
  scoped_refptr<SiteInstance> new_instance = GetSiteInstanceForNavigation(
      dest_url, source_instance, dest_instance, transition,
      dest_is_restore, dest_is_view_source_mode);

  const NavigationEntry* current_entry =
      delegate_->GetLastCommittedNavigationEntryForRenderManager();

  DCHECK(!cross_navigation_pending_);

  if (new_instance.get() != current_instance) {
    TRACE_EVENT_INSTANT2(
        "navigation",
        "RenderFrameHostManager::UpdateStateForNavigate:New SiteInstance",
        TRACE_EVENT_SCOPE_THREAD,
        "current_instance id", current_instance->GetId(),
        "new_instance id", new_instance->GetId());

    // New SiteInstance: create a pending RFH to navigate.

    // This will possibly create (set to nullptr) a Web UI object for the
    // pending page. We'll use this later to give the page special access. This
    // must happen before the new renderer is created below so it will get
    // bindings. It must also happen after the above conditional call to
    // CancelPending(), otherwise CancelPending may clear the pending_web_ui_
    // and the page will not have its bindings set appropriately.
    SetPendingWebUI(dest_url, bindings);
    CreatePendingRenderFrameHost(current_instance, new_instance.get(),
                                 frame_tree_node_->IsMainFrame());
    if (!pending_render_frame_host_.get()) {
      return nullptr;
    }

    // Check if our current RFH is live before we set up a transition.
    if (!render_frame_host_->IsRenderFrameLive()) {
      if (!cross_navigation_pending_) {
        // The current RFH is not live.  There's no reason to sit around with a
        // sad tab or a newly created RFH while we wait for the pending RFH to
        // navigate.  Just switch to the pending RFH now and go back to non
        // cross-navigating (Note that we don't care about on{before}unload
        // handlers if the current RFH isn't live.)
        CommitPending();
        return render_frame_host_.get();
      } else {
        NOTREACHED();
        return render_frame_host_.get();
      }
    }
    // Otherwise, it's safe to treat this as a pending cross-site transition.

    // We now have a pending RFH.
    DCHECK(!cross_navigation_pending_);
    cross_navigation_pending_ = true;

    // We need to wait until the beforeunload handler has run, unless we are
    // transferring an existing request (in which case it has already run).
    // Suspend the new render view (i.e., don't let it send the cross-site
    // Navigate message) until we hear back from the old renderer's
    // beforeunload handler.  If the handler returns false, we'll have to
    // cancel the request.
    //
    DCHECK(!pending_render_frame_host_->are_navigations_suspended());
    bool is_transfer = transferred_request_id != GlobalRequestID();
    if (is_transfer) {
      // We don't need to stop the old renderer or run beforeunload/unload
      // handlers, because those have already been done.
      DCHECK(cross_site_transferring_request_->request_id() ==
             transferred_request_id);
    } else {
      // Also make sure the old render view stops, in case a load is in
      // progress.  (We don't want to do this for transfers, since it will
      // interrupt the transfer with an unexpected DidStopLoading.)
      render_frame_host_->Send(new FrameMsg_Stop(
          render_frame_host_->GetRoutingID()));
      pending_render_frame_host_->SetNavigationsSuspended(true,
                                                          base::TimeTicks());
      // Unless we are transferring an existing request, we should now tell the
      // old render view to run its beforeunload handler, since it doesn't
      // otherwise know that the cross-site request is happening.  This will
      // trigger a call to OnBeforeUnloadACK with the reply.
      render_frame_host_->DispatchBeforeUnload(true);
    }

    return pending_render_frame_host_.get();
  }

  // Otherwise the same SiteInstance can be used.  Navigate render_frame_host_.

  // It's possible to swap out the current RFH and then decide to navigate in it
  // anyway (e.g., a cross-process navigation that redirects back to the
  // original site).  In that case, we have a proxy for the current RFH but
  // haven't deleted it yet.  The new navigation will swap it back in, so we can
  // delete the proxy.
  DeleteRenderFrameProxyHost(new_instance.get());

  if (ShouldReuseWebUI(current_entry, dest_url)) {
    pending_web_ui_.reset();
    pending_and_current_web_ui_ = web_ui_->AsWeakPtr();
  } else {
    SetPendingWebUI(dest_url, bindings);
    // Make sure the new RenderViewHost has the right bindings.
    if (pending_web_ui() &&
        !render_frame_host_->GetProcess()->IsIsolatedGuest()) {
      render_frame_host_->render_view_host()->AllowBindings(
          pending_web_ui()->GetBindings());
    }
  }

  if (pending_web_ui() && render_frame_host_->IsRenderFrameLive()) {
    pending_web_ui()->GetController()->RenderViewReused(
        render_frame_host_->render_view_host());
  }

  // The renderer can exit view source mode when any error or cancellation
  // happen. We must overwrite to recover the mode.
  if (dest_is_view_source_mode) {
    render_frame_host_->render_view_host()->Send(
        new ViewMsg_EnableViewSourceMode(
            render_frame_host_->render_view_host()->GetRoutingID()));
  }

  return render_frame_host_.get();
}

void RenderFrameHostManager::CancelPending() {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CancelPending",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  DiscardUnusedFrame(UnsetPendingRenderFrameHost());
}

scoped_ptr<RenderFrameHostImpl>
RenderFrameHostManager::UnsetPendingRenderFrameHost() {
  scoped_ptr<RenderFrameHostImpl> pending_render_frame_host =
      pending_render_frame_host_.Pass();

  RenderFrameDevToolsAgentHost::OnCancelPendingNavigation(
      pending_render_frame_host.get(),
      render_frame_host_.get());

  // We no longer need to prevent the process from exiting.
  pending_render_frame_host->GetProcess()->RemovePendingView();

  pending_web_ui_.reset();
  pending_and_current_web_ui_.reset();

  return pending_render_frame_host.Pass();
}

scoped_ptr<RenderFrameHostImpl> RenderFrameHostManager::SetRenderFrameHost(
    scoped_ptr<RenderFrameHostImpl> render_frame_host) {
  // Swap the two.
  scoped_ptr<RenderFrameHostImpl> old_render_frame_host =
      render_frame_host_.Pass();
  render_frame_host_ = render_frame_host.Pass();

  if (frame_tree_node_->IsMainFrame()) {
    // Update the count of top-level frames using this SiteInstance.  All
    // subframes are in the same BrowsingInstance as the main frame, so we only
    // count top-level ones.  This makes the value easier for consumers to
    // interpret.
    if (render_frame_host_) {
      render_frame_host_->GetSiteInstance()->
          IncrementRelatedActiveContentsCount();
    }
    if (old_render_frame_host) {
      old_render_frame_host->GetSiteInstance()->
          DecrementRelatedActiveContentsCount();
    }
  }

  return old_render_frame_host.Pass();
}

bool RenderFrameHostManager::IsRVHOnSwappedOutList(
    RenderViewHostImpl* rvh) const {
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(
      rvh->GetSiteInstance());
  if (!proxy)
    return false;
  // If there is a proxy without RFH, it is for a subframe in the SiteInstance
  // of |rvh|. Subframes should be ignored in this case.
  if (!proxy->render_frame_host())
    return false;
  return IsOnSwappedOutList(proxy->render_frame_host());
}

bool RenderFrameHostManager::IsOnSwappedOutList(
    RenderFrameHostImpl* rfh) const {
  if (!rfh->GetSiteInstance())
    return false;

  RenderFrameProxyHostMap::const_iterator iter = proxy_hosts_.find(
      rfh->GetSiteInstance()->GetId());
  if (iter == proxy_hosts_.end())
    return false;

  return iter->second->render_frame_host() == rfh;
}

RenderViewHostImpl* RenderFrameHostManager::GetSwappedOutRenderViewHost(
   SiteInstance* instance) const {
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(instance);
  if (proxy)
    return proxy->GetRenderViewHost();
  return NULL;
}

RenderFrameProxyHost* RenderFrameHostManager::GetRenderFrameProxyHost(
    SiteInstance* instance) const {
  RenderFrameProxyHostMap::const_iterator iter =
      proxy_hosts_.find(instance->GetId());
  if (iter != proxy_hosts_.end())
    return iter->second;

  return NULL;
}

void RenderFrameHostManager::DeleteRenderFrameProxyHost(
    SiteInstance* instance) {
  RenderFrameProxyHostMap::iterator iter = proxy_hosts_.find(instance->GetId());
  if (iter != proxy_hosts_.end()) {
    delete iter->second;
    proxy_hosts_.erase(iter);
  }
}

}  // namespace content
