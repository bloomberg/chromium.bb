// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/render_view_host_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/webui/web_ui.h"
#include "content/browser/webui/web_ui_factory.h"
#include "content/common/content_client.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "content/common/url_constants.h"
#include "content/common/view_messages.h"

namespace base {
class WaitableEvent;
}

RenderViewHostManager::RenderViewHostManager(
    RenderViewHostDelegate* render_view_delegate,
    Delegate* delegate)
    : delegate_(delegate),
      cross_navigation_pending_(false),
      render_view_delegate_(render_view_delegate),
      render_view_host_(NULL),
      pending_render_view_host_(NULL),
      interstitial_page_(NULL) {
}

RenderViewHostManager::~RenderViewHostManager() {
  if (pending_render_view_host_)
    CancelPending();

  // We should always have a main RenderViewHost.
  RenderViewHost* render_view_host = render_view_host_;
  render_view_host_ = NULL;
  render_view_host->Shutdown();

  // Shut down any swapped out RenderViewHosts.
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    iter->second->Shutdown();
  }
}

void RenderViewHostManager::Init(Profile* profile,
                                 SiteInstance* site_instance,
                                 int routing_id) {
  // Create a RenderViewHost, once we have an instance.  It is important to
  // immediately give this SiteInstance to a RenderViewHost so that it is
  // ref counted.
  if (!site_instance)
    site_instance = SiteInstance::CreateSiteInstance(profile);
  render_view_host_ = RenderViewHostFactory::Create(
      site_instance, render_view_delegate_, routing_id, delegate_->
      GetControllerForRenderManager().session_storage_namespace());

  // Keep track of renderer processes as they start to shut down.
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSING,
                 NotificationService::AllSources());
}

RenderWidgetHostView* RenderViewHostManager::GetRenderWidgetHostView() const {
  if (!render_view_host_)
    return NULL;
  return render_view_host_->view();
}

RenderViewHost* RenderViewHostManager::Navigate(const NavigationEntry& entry) {
  // Create a pending RenderViewHost. It will give us the one we should use
  RenderViewHost* dest_render_view_host = UpdateRendererStateForNavigate(entry);
  if (!dest_render_view_host)
    return NULL;  // We weren't able to create a pending render view host.

  // If the current render_view_host_ isn't live, we should create it so
  // that we don't show a sad tab while the dest_render_view_host fetches
  // its first page.  (Bug 1145340)
  if (dest_render_view_host != render_view_host_ &&
      !render_view_host_->IsRenderViewLive()) {
    // Note: we don't call InitRenderView here because we are navigating away
    // soon anyway, and we don't have the NavigationEntry for this host.
    delegate_->CreateRenderViewForRenderManager(render_view_host_);
  }

  // If the renderer crashed, then try to create a new one to satisfy this
  // navigation request.
  if (!dest_render_view_host->IsRenderViewLive()) {
    if (!InitRenderView(dest_render_view_host, entry))
      return NULL;

    // Now that we've created a new renderer, be sure to hide it if it isn't
    // our primary one.  Otherwise, we might crash if we try to call Show()
    // on it later.
    if (dest_render_view_host != render_view_host_ &&
        dest_render_view_host->view()) {
      dest_render_view_host->view()->Hide();
    } else {
      // This is our primary renderer, notify here as we won't be calling
      // CommitPending (which does the notify).
      RenderViewHostSwitchedDetails details;
      details.new_host = render_view_host_;
      details.old_host = NULL;
      NotificationService::current()->Notify(
          NotificationType::RENDER_VIEW_HOST_CHANGED,
          Source<NavigationController>(
              &delegate_->GetControllerForRenderManager()),
          Details<RenderViewHostSwitchedDetails>(&details));
    }
  }

  return dest_render_view_host;
}

void RenderViewHostManager::Stop() {
  render_view_host_->Send(new ViewMsg_Stop(render_view_host_->routing_id()));

  // If we are cross-navigating, we should stop the pending renderers.  This
  // will lead to a DidFailProvisionalLoad, which will properly destroy them.
  if (cross_navigation_pending_) {
    pending_render_view_host_->Send(
        new ViewMsg_Stop(pending_render_view_host_->routing_id()));
  }
}

void RenderViewHostManager::SetIsLoading(bool is_loading) {
  render_view_host_->SetIsLoading(is_loading);
  if (pending_render_view_host_)
    pending_render_view_host_->SetIsLoading(is_loading);
}

bool RenderViewHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  if (!cross_navigation_pending_)
    return true;

  // If the tab becomes unresponsive during unload while doing a
  // cross-site navigation, proceed with the navigation.  (This assumes that
  // the pending RenderViewHost is still responsive.)
  int pending_request_id = pending_render_view_host_->GetPendingRequestId();
  if (pending_request_id == -1) {
    // Haven't gotten around to starting the request, because we're still
    // waiting for the beforeunload handler to finish.  We'll pretend that it
    // did finish, to let the navigation proceed.  Note that there's a danger
    // that the beforeunload handler will later finish and possibly return
    // false (meaning the navigation should not proceed), but we'll ignore it
    // in this case because it took too long.
    if (pending_render_view_host_->are_navigations_suspended())
      pending_render_view_host_->SetNavigationsSuspended(false);
  } else {
    // The request has been started and paused while we're waiting for the
    // unload handler to finish.  We'll pretend that it did, by notifying the
    // IO thread to let the response continue.  The pending renderer will then
    // be swapped in as part of the usual DidNavigate logic.  (If the unload
    // handler later finishes, this call will be ignored because the state in
    // CrossSiteResourceHandler will already be cleaned up.)
    ViewMsg_SwapOut_Params params;
    params.new_render_process_host_id =
        pending_render_view_host_->process()->id();
    params.new_request_id = pending_request_id;
    current_host()->process()->CrossSiteSwapOutACK(params);
  }
  return false;
}

void RenderViewHostManager::DidNavigateMainFrame(
    RenderViewHost* render_view_host) {
  if (!cross_navigation_pending_) {
    DCHECK(!pending_render_view_host_);

    // We should only hear this from our current renderer.
    DCHECK(render_view_host == render_view_host_);

    // Even when there is no pending RVH, there may be a pending Web UI.
    if (pending_web_ui_.get())
      CommitPending();
    return;
  }

  if (render_view_host == pending_render_view_host_) {
    // The pending cross-site navigation completed, so show the renderer.
    // If it committed without sending network requests (e.g., data URLs),
    // then we still need to swap out the old RVH first and run its unload
    // handler.  OK for that to happen in the background.
    if (pending_render_view_host_->GetPendingRequestId() == -1) {
      OnCrossSiteResponse(pending_render_view_host_->process()->id(),
                          pending_render_view_host_->routing_id());
    }

    CommitPending();
    cross_navigation_pending_ = false;
  } else if (render_view_host == render_view_host_) {
    // A navigation in the original page has taken place.  Cancel the pending
    // one.
    CancelPending();
    cross_navigation_pending_ = false;
  } else {
    // No one else should be sending us DidNavigate in this state.
    DCHECK(false);
  }
}

void RenderViewHostManager::SetWebUIPostCommit(WebUI* web_ui) {
  DCHECK(!web_ui_.get());
  web_ui_.reset(web_ui);
}

void RenderViewHostManager::RendererAbortedProvisionalLoad(
    RenderViewHost* render_view_host) {
  // We used to cancel the pending renderer here for cross-site downloads.
  // However, it's not safe to do that because the download logic repeatedly
  // looks for this TabContents based on a render view ID.  Instead, we just
  // leave the pending renderer around until the next navigation event
  // (Navigate, DidNavigate, etc), which will clean it up properly.
  // TODO(creis): All of this will go away when we move the cross-site logic
  // to ResourceDispatcherHost, so that we intercept responses rather than
  // navigation events.  (That's necessary to support onunload anyway.)  Once
  // we've made that change, we won't create a pending renderer until we know
  // the response is not a download.
}

void RenderViewHostManager::RendererProcessClosing(
    RenderProcessHost* render_process_host) {
  // Remove any swapped out RVHs from this process, so that we don't try to
  // swap them back in while the process is exiting.  Start by finding them,
  // since there could be more than one.
  std::list<int> ids_to_remove;
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    if (iter->second->process() == render_process_host)
      ids_to_remove.push_back(iter->first);
  }

  // Now delete them.
  while (!ids_to_remove.empty()) {
    swapped_out_hosts_[ids_to_remove.back()]->Shutdown();
    swapped_out_hosts_.erase(ids_to_remove.back());
    ids_to_remove.pop_back();
  }
}

void RenderViewHostManager::ShouldClosePage(bool for_cross_site_transition,
                                            bool proceed) {
  if (for_cross_site_transition) {
    // Ignore if we're not in a cross-site navigation.
    if (!cross_navigation_pending_)
      return;

    if (proceed) {
      // Ok to unload the current page, so proceed with the cross-site
      // navigation.  Note that if navigations are not currently suspended, it
      // might be because the renderer was deemed unresponsive and this call was
      // already made by ShouldCloseTabOnUnresponsiveRenderer.  In that case, it
      // is ok to do nothing here.
      if (pending_render_view_host_ &&
          pending_render_view_host_->are_navigations_suspended())
        pending_render_view_host_->SetNavigationsSuspended(false);
    } else {
      // Current page says to cancel.
      CancelPending();
      cross_navigation_pending_ = false;
    }
  } else {
    // Non-cross site transition means closing the entire tab.
    bool proceed_to_fire_unload;
    delegate_->BeforeUnloadFiredFromRenderManager(proceed,
                                                  &proceed_to_fire_unload);

    if (proceed_to_fire_unload) {
      // This is not a cross-site navigation, the tab is being closed.
      render_view_host_->ClosePage();
    }
  }
}

void RenderViewHostManager::OnCrossSiteResponse(int new_render_process_host_id,
                                                int new_request_id) {
  // Should only see this while we have a pending renderer.
  if (!cross_navigation_pending_)
    return;
  DCHECK(pending_render_view_host_);

  // Tell the old renderer it is being swapped out.  This will fire the unload
  // handler (without firing the beforeunload handler a second time).  When the
  // unload handler finishes and the navigation completes, we will send a
  // message to the ResourceDispatcherHost with the given pending request IDs,
  // allowing the pending RVH's response to resume.
  render_view_host_->SwapOut(new_render_process_host_id, new_request_id);

  // ResourceDispatcherHost has told us to run the onunload handler, which
  // means it is not a download or unsafe page, and we are going to perform the
  // navigation.  Thus, we no longer need to remember that the RenderViewHost
  // is part of a pending cross-site request.
  pending_render_view_host_->SetHasPendingCrossSiteRequest(false,
                                                           new_request_id);
}

void RenderViewHostManager::OnCrossSiteNavigationCanceled() {
  DCHECK(cross_navigation_pending_);
  cross_navigation_pending_ = false;
  if (pending_render_view_host_)
    CancelPending();
}

void RenderViewHostManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::RENDERER_PROCESS_CLOSING:
      RendererProcessClosing(Source<RenderProcessHost>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}

bool RenderViewHostManager::ShouldTransitionCrossSite() {
  // True if we are using process-per-site-instance (default) or
  // process-per-site (kProcessPerSite).
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerTab);
}

bool RenderViewHostManager::ShouldSwapProcessesForNavigation(
    const NavigationEntry* cur_entry,
    const NavigationEntry* new_entry) const {
  DCHECK(new_entry);

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't.  If there's no cur_entry, check the current RVH's
  // site, which might already be committed to a Web UI URL (such as the NTP).
  const GURL& current_url = (cur_entry) ? cur_entry->url() :
      render_view_host_->site_instance()->site();
  Profile* profile = delegate_->GetControllerForRenderManager().profile();
  const content::WebUIFactory* web_ui_factory = content::WebUIFactory::Get();
  if (web_ui_factory->UseWebUIForURL(profile, current_url)) {
    // Force swap if it's not an acceptable URL for Web UI.
    if (!web_ui_factory->IsURLAcceptableForWebUI(profile, new_entry->url()))
      return true;
  } else {
    // Force swap if it's a Web UI URL.
    if (web_ui_factory->UseWebUIForURL(profile, new_entry->url()))
      return true;
  }

  if (!cur_entry) {
    // Always choose a new process when navigating to extension URLs. The
    // process grouping logic will combine all of a given extension's pages
    // into the same process.
    if (new_entry->url().SchemeIs(chrome::kExtensionScheme))
      return true;

    return false;
  }

  // We can't switch a RenderView between view source and non-view source mode
  // without screwing up the session history sometimes (when navigating between
  // "view-source:http://foo.com/" and "http://foo.com/", WebKit doesn't treat
  // it as a new navigation). So require a view switch.
  if (cur_entry->IsViewSourceMode() != new_entry->IsViewSourceMode())
    return true;

  // Also, we must switch if one is an extension and the other is not the exact
  // same extension.
  if (cur_entry->url().SchemeIs(chrome::kExtensionScheme) ||
      new_entry->url().SchemeIs(chrome::kExtensionScheme)) {
    if (cur_entry->url().GetOrigin() != new_entry->url().GetOrigin())
      return true;
  }

  return false;
}

SiteInstance* RenderViewHostManager::GetSiteInstanceForEntry(
    const NavigationEntry& entry,
    SiteInstance* curr_instance) {
  // NOTE: This is only called when ShouldTransitionCrossSite is true.

  // If the entry has an instance already, we should use it.
  if (entry.site_instance())
    return entry.site_instance();

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
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerSite) &&
      entry.transition_type() == PageTransition::GENERATED)
    return curr_instance;

  const GURL& dest_url = entry.url();
  NavigationController& controller = delegate_->GetControllerForRenderManager();
  Profile* profile = controller.profile();

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored or it's a Web UI as described below.
  if (!curr_instance->has_site()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the curr_instance has a site, because for now, we want
    // to compare against the current URL and not the SiteInstance's site.  In
    // this case, there is no current URL, so comparing against the site is ok.
    // See additional comments below.)
    if (curr_instance->HasRelatedSiteInstance(dest_url))
      return curr_instance->GetRelatedSiteInstance(dest_url);

    // For extensions and Web UI URLs (such as the new tab page), we do not
    // want to use the curr_instance if it has no site, since it will have a
    // RenderProcessHost of TYPE_NORMAL.  Create a new SiteInstance for this
    // URL instead (with the correct process type).
    if (content::WebUIFactory::Get()->UseWebUIForURL(profile, dest_url)) {
      return SiteInstance::CreateSiteInstanceForURL(profile, dest_url);
    }

    // Normally the "site" on the SiteInstance is set lazily when the load
    // actually commits. This is to support better process sharing in case
    // the site redirects to some other site: we want to use the destination
    // site in the site instance.
    //
    // In the case of session restore, as it loads all the pages immediately
    // we need to set the site first, otherwise after a restore none of the
    // pages would share renderers in process-per-site.
    if (entry.restore_type() != NavigationEntry::RESTORE_NONE)
      curr_instance->SetSite(dest_url);

    return curr_instance;
  }

  // Otherwise, only create a new SiteInstance for cross-site navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = curr_instance->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationEntry* curr_entry = controller.GetLastCommittedEntry();
  if (interstitial_page_) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    curr_entry = controller.GetEntryAtOffset(-1);
  }
  // If there is no last non-interstitial entry (and curr_instance already
  // has a site), then we must have been opened from another tab.  We want
  // to compare against the URL of the page that opened us, but we can't
  // get to it directly.  The best we can do is check against the site of
  // the SiteInstance.  This will be correct when we intercept links and
  // script-based navigations, but for now, it could place some pages in a
  // new process unnecessarily.  We should only hit this case if a page tries
  // to open a new tab to an interstitial-inducing URL, and then navigates
  // the page to a different same-site URL.  (This seems very unlikely in
  // practice.)
  const GURL& current_url = (curr_entry) ? curr_entry->url() :
      curr_instance->site();

  if (SiteInstance::IsSameWebSite(profile, current_url, dest_url)) {
    return curr_instance;
  } else if (ShouldSwapProcessesForNavigation(curr_entry, &entry)) {
    // When we're swapping, we need to force the site instance AND browsing
    // instance to be different ones. This addresses special cases where we use
    // a single BrowsingInstance for all pages of a certain type (e.g., New Tab
    // Pages), keeping them in the same process. When you navigate away from
    // that page, we want to explicity ignore that BrowsingInstance and group
    // this page into the appropriate SiteInstance for its URL.
    return SiteInstance::CreateSiteInstanceForURL(profile, dest_url);
  } else {
    // Start the new renderer in a new SiteInstance, but in the current
    // BrowsingInstance.  It is important to immediately give this new
    // SiteInstance to a RenderViewHost (if it is different than our current
    // SiteInstance), so that it is ref counted.  This will happen in
    // CreatePendingRenderView.
    return curr_instance->GetRelatedSiteInstance(dest_url);
  }
}

bool RenderViewHostManager::CreatePendingRenderView(
    const NavigationEntry& entry, SiteInstance* instance) {
  NavigationEntry* curr_entry =
      delegate_->GetControllerForRenderManager().GetLastCommittedEntry();
  if (curr_entry) {
    DCHECK(!curr_entry->content_state().empty());
    // TODO(creis): Should send a message to the RenderView to let it know
    // we're about to switch away, so that it sends an UpdateState message.
  }

  // Check if we've already created an RVH for this SiteInstance.
  CHECK(instance);
  RenderViewHostMap::iterator iter =
      swapped_out_hosts_.find(instance->id());
  if (iter != swapped_out_hosts_.end()) {
    // Re-use the existing RenderViewHost, which has already been initialized.
    // We'll remove it from the list of swapped out hosts if it commits.
    pending_render_view_host_ = iter->second;

    // Prevent the process from exiting while we're trying to use it.
    pending_render_view_host_->process()->AddPendingView();

    return true;
  }

  pending_render_view_host_ = RenderViewHostFactory::Create(
      instance, render_view_delegate_, MSG_ROUTING_NONE, delegate_->
      GetControllerForRenderManager().session_storage_namespace());

  // Prevent the process from exiting while we're trying to use it.
  pending_render_view_host_->process()->AddPendingView();

  bool success = InitRenderView(pending_render_view_host_, entry);
  if (success) {
    // Don't show the view until we get a DidNavigate from it.
    pending_render_view_host_->view()->Hide();
  } else {
    CancelPending();
  }
  return success;
}

bool RenderViewHostManager::InitRenderView(RenderViewHost* render_view_host,
                                           const NavigationEntry& entry) {
  // If the pending navigation is to a WebUI, tell the RenderView about any
  // bindings it will need enabled.
  if (pending_web_ui_.get())
    render_view_host->AllowBindings(pending_web_ui_->bindings());

  return delegate_->CreateRenderViewForRenderManager(render_view_host);
}

void RenderViewHostManager::CommitPending() {
  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared |pending_web_ui_| the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  // Next commit the Web UI, if any.
  web_ui_.swap(pending_web_ui_);
  if (web_ui_.get() && pending_web_ui_.get() && !pending_render_view_host_)
    web_ui_->DidBecomeActiveForReusedRenderView();
  pending_web_ui_.reset();

  // It's possible for the pending_render_view_host_ to be NULL when we aren't
  // crossing process boundaries. If so, we just needed to handle the Web UI
  // committing above and we're done.
  if (!pending_render_view_host_) {
    if (will_focus_location_bar)
      delegate_->SetFocusToLocationBar(false);
    return;
  }

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = !will_focus_location_bar &&
      render_view_host_->view() && render_view_host_->view()->HasFocus();

  // Swap in the pending view and make it active.
  RenderViewHost* old_render_view_host = render_view_host_;
  render_view_host_ = pending_render_view_host_;
  pending_render_view_host_ = NULL;

  // The process will no longer try to exit, so we can decrement the count.
  render_view_host_->process()->RemovePendingView();

  // If the view is gone, then this RenderViewHost died while it was hidden.
  // We ignored the RenderViewGone call at the time, so we should send it now
  // to make sure the sad tab shows up, etc.
  if (render_view_host_->view())
    render_view_host_->view()->Show();
  else
    delegate_->RenderViewGoneFromRenderManager(render_view_host_);

  // Hide the old view now that the new one is visible.
  if (old_render_view_host->view()) {
    old_render_view_host->view()->Hide();
    old_render_view_host->WasSwappedOut();
  }

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager();

  if (will_focus_location_bar)
    delegate_->SetFocusToLocationBar(false);
  else if (focus_render_view && render_view_host_->view())
    render_view_host_->view()->Focus();

  RenderViewHostSwitchedDetails details;
  details.new_host = render_view_host_;
  details.old_host = old_render_view_host;
  NotificationService::current()->Notify(
      NotificationType::RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&delegate_->GetControllerForRenderManager()),
      Details<RenderViewHostSwitchedDetails>(&details));

  // If the pending view was on the swapped out list, we can remove it.
  swapped_out_hosts_.erase(render_view_host_->site_instance()->id());

  // If the old RVH is live, we are swapping it out and should keep track of it
  // in case we navigate back to it.
  if (old_render_view_host->IsRenderViewLive()) {
    DCHECK(old_render_view_host->is_swapped_out());
    swapped_out_hosts_[old_render_view_host->site_instance()->id()] =
        old_render_view_host;
  } else {
    old_render_view_host->Shutdown();
  }

  // Let the task manager know that we've swapped RenderViewHosts, since it
  // might need to update its process groupings.
  delegate_->NotifySwappedFromRenderManager();
}

RenderViewHost* RenderViewHostManager::UpdateRendererStateForNavigate(
    const NavigationEntry& entry) {
  // If we are cross-navigating, then we want to get back to normal and navigate
  // as usual.
  if (cross_navigation_pending_) {
    if (pending_render_view_host_)
      CancelPending();
    cross_navigation_pending_ = false;
  }

  // This will possibly create (set to NULL) a Web UI object for the pending
  // page. We'll use this later to give the page special access. This must
  // happen before the new renderer is created below so it will get bindings.
  // It must also happen after the above conditional call to CancelPending(),
  // otherwise CancelPending may clear the pending_web_ui_ and the page will
  // not have it's bindings set appropriately.
  pending_web_ui_.reset(delegate_->CreateWebUIForRenderManager(entry.url()));

  // render_view_host_ will not be deleted before the end of this method, so we
  // don't have to worry about this SiteInstance's ref count dropping to zero.
  SiteInstance* curr_instance = render_view_host_->site_instance();

  // Determine if we need a new SiteInstance for this entry.
  // Again, new_instance won't be deleted before the end of this method, so it
  // is safe to use a normal pointer here.
  SiteInstance* new_instance = curr_instance;
  bool force_swap = ShouldSwapProcessesForNavigation(
          delegate_->GetLastCommittedNavigationEntryForRenderManager(),
          &entry);
  if (ShouldTransitionCrossSite() || force_swap)
    new_instance = GetSiteInstanceForEntry(entry, curr_instance);

  if (new_instance != curr_instance || force_swap) {
    // New SiteInstance.
    DCHECK(!cross_navigation_pending_);

    // Create a pending RVH and navigate it.
    bool success = CreatePendingRenderView(entry, new_instance);
    if (!success)
      return NULL;

    // Check if our current RVH is live before we set up a transition.
    if (!render_view_host_->IsRenderViewLive()) {
      if (!cross_navigation_pending_) {
        // The current RVH is not live.  There's no reason to sit around with a
        // sad tab or a newly created RVH while we wait for the pending RVH to
        // navigate.  Just switch to the pending RVH now and go back to non
        // cross-navigating (Note that we don't care about on{before}unload
        // handlers if the current RVH isn't live.)
        CommitPending();
        return render_view_host_;
      } else {
        NOTREACHED();
        return render_view_host_;
      }
    }
    // Otherwise, it's safe to treat this as a pending cross-site transition.

    // Make sure the old render view stops, in case a load is in progress.
    render_view_host_->Send(new ViewMsg_Stop(render_view_host_->routing_id()));

    // Suspend the new render view (i.e., don't let it send the cross-site
    // Navigate message) until we hear back from the old renderer's
    // onbeforeunload handler.  If the handler returns false, we'll have to
    // cancel the request.
    DCHECK(!pending_render_view_host_->are_navigations_suspended());
    pending_render_view_host_->SetNavigationsSuspended(true);

    // Tell the CrossSiteRequestManager that this RVH has a pending cross-site
    // request, so that ResourceDispatcherHost will know to tell us to run the
    // old page's onunload handler before it sends the response.
    pending_render_view_host_->SetHasPendingCrossSiteRequest(true, -1);

    // We now have a pending RVH.
    DCHECK(!cross_navigation_pending_);
    cross_navigation_pending_ = true;

    // Tell the old render view to run its onbeforeunload handler, since it
    // doesn't otherwise know that the cross-site request is happening.  This
    // will trigger a call to ShouldClosePage with the reply.
    render_view_host_->FirePageBeforeUnload(true);

    return pending_render_view_host_;
  } else {
    if (pending_web_ui_.get() && render_view_host_->IsRenderViewLive())
      pending_web_ui_->RenderViewReused(render_view_host_);

    // The renderer can exit view source mode when any error or cancellation
    // happen. We must overwrite to recover the mode.
    if (entry.IsViewSourceMode()) {
      render_view_host_->Send(
          new ViewMsg_EnableViewSourceMode(render_view_host_->routing_id()));
    }
  }

  // Same SiteInstance can be used.  Navigate render_view_host_ if we are not
  // cross navigating.
  DCHECK(!cross_navigation_pending_);
  return render_view_host_;
}

void RenderViewHostManager::CancelPending() {
  RenderViewHost* pending_render_view_host = pending_render_view_host_;
  pending_render_view_host_ = NULL;

  // We no longer need to prevent the process from exiting.
  pending_render_view_host->process()->RemovePendingView();

  // The pending RVH may already be on the swapped out list if we started to
  // swap it back in and then canceled.  If so, make sure it gets swapped out
  // again.  If it's not on the swapped out list (e.g., aborting a pending
  // load), then it's safe to shut down.
  if (IsSwappedOut(pending_render_view_host)) {
    // Any currently suspended navigations are no longer needed.
    pending_render_view_host->CancelSuspendedNavigations();

    // We can pass -1,-1 because there is no pending response in the
    // ResourceDispatcherHost to unpause.
    pending_render_view_host->SwapOut(-1, -1);
  } else {
    // We won't be coming back, so shut this one down.
    pending_render_view_host->Shutdown();
  }

  pending_web_ui_.reset();
}

void RenderViewHostManager::RenderViewDeleted(RenderViewHost* rvh) {
  // We are doing this in order to work around and to track a crasher
  // (http://crbug.com/23411) where it seems that pending_render_view_host_ is
  // deleted (not sure from where) but not NULLed.
  if (rvh == pending_render_view_host_) {
    // If you hit this NOTREACHED, please report it in the following bug
    // http://crbug.com/23411 Make sure to include what you were doing when it
    // happened  (navigating to a new page, closing a tab...) and if you can
    // reproduce.
    NOTREACHED();
    pending_render_view_host_ = NULL;
  }

  // Make sure deleted RVHs are not kept in the swapped out list while we are
  // still alive.  (If render_view_host_ is null, we're already being deleted.)
  if (!render_view_host_)
    return;
  // We can't look it up by SiteInstance ID, which may no longer be valid.
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    if (iter->second == rvh) {
      swapped_out_hosts_.erase(iter);
      break;
    }
  }
}

void RenderViewHostManager::SwapInRenderViewHost(RenderViewHost* rvh) {
  // TODO(creis): Abstract out the common code between this and CommitPending.
  web_ui_.reset();

  // Make sure the current RVH is swapped out so that it filters out any
  // disruptive messages from the renderer.  We can pass -1,-1 because there is
  // no pending response in the ResourceDispatcherHost to unpause.
  render_view_host_->SwapOut(-1, -1);

  // Swap in the new view and make it active.
  RenderViewHost* old_render_view_host = render_view_host_;
  render_view_host_ = rvh;
  render_view_host_->set_delegate(render_view_delegate_);
  // Remove old RenderWidgetHostView with mocked out methods so it can be
  // replaced with a new one that's a child of |delegate_|'s view.
  scoped_ptr<RenderWidgetHostView> old_view(render_view_host_->view());
  render_view_host_->set_view(NULL);
  delegate_->CreateViewAndSetSizeForRVH(render_view_host_);
  render_view_host_->ActivateDeferredPluginHandles();
  // If the view is gone, then this RenderViewHost died while it was hidden.
  // We ignored the RenderViewGone call at the time, so we should send it now
  // to make sure the sad tab shows up, etc.
  if (render_view_host_->view()) {
    // The Hide() is needed to sync the state of |render_view_host_|, which is
    // hidden, with the newly created view, which does not know the
    // RenderViewHost is hidden.
    // TODO(tburkard,cbentzel): Figure out if this hack can be removed
    //                          (http://crbug.com/79891).
    render_view_host_->view()->Hide();
    render_view_host_->view()->Show();
  }

  // Hide the current view and prepare to swap it out.
  if (old_render_view_host->view()) {
    old_render_view_host->view()->Hide();
    old_render_view_host->WasSwappedOut();
  }

  delegate_->UpdateRenderViewSizeForRenderManager();

  RenderViewHostSwitchedDetails details;
  details.new_host = render_view_host_;
  details.old_host = old_render_view_host;
  NotificationService::current()->Notify(
      NotificationType::RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&delegate_->GetControllerForRenderManager()),
      Details<RenderViewHostSwitchedDetails>(&details));

  // If the given RVH was on the swapped out list, we can remove it.
  swapped_out_hosts_.erase(render_view_host_->site_instance()->id());

  // If the old RVH is live, we are swapping it out and should keep track of it
  // in case we navigate back to it.
  if (old_render_view_host->IsRenderViewLive()) {
    DCHECK(old_render_view_host->is_swapped_out());
    swapped_out_hosts_[old_render_view_host->site_instance()->id()] =
        old_render_view_host;
  } else {
    old_render_view_host->Shutdown();
  }

  // Let the task manager know that we've swapped RenderViewHosts, since it
  // might need to update its process groupings.
  delegate_->NotifySwappedFromRenderManager();
}

bool RenderViewHostManager::IsSwappedOut(RenderViewHost* rvh) {
  if (!rvh->site_instance())
    return false;

  return swapped_out_hosts_.find(rvh->site_instance()->id()) !=
      swapped_out_hosts_.end();
}
