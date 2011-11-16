// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_contents.h"

#include <algorithm>
#include <utility>

#include "base/process_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_render_view_host_observer.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_request_details.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/notification_service.h"
#include "content/common/view_messages.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#endif

namespace prerender {

namespace {

// Compares URLs ignoring any ref for the purposes of matching URLs when
// prerendering.
struct PrerenderURLPredicate {
  explicit PrerenderURLPredicate(const GURL& url)
      : url_(url) {
  }

  bool operator()(const GURL& url) const {
    return url.scheme() == url_.scheme() &&
           url.host() == url_.host() &&
           url.port() == url_.port() &&
           url.path() == url_.path() &&
           url.query() == url_.query();
  }
  GURL url_;
};

}  // end namespace

class PrerenderContentsFactoryImpl : public PrerenderContents::Factory {
 public:
  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, PrerenderTracker* prerender_tracker,
      Profile* profile, const GURL& url, const GURL& referrer,
      Origin origin, uint8 experiment_id) OVERRIDE {
    return new PrerenderContents(prerender_manager, prerender_tracker, profile,
                                 url, referrer, origin, experiment_id);
  }
};

PrerenderContents::PendingPrerenderData::PendingPrerenderData(
    Origin origin,
    const GURL& url,
    const GURL& referrer)
    : origin(origin),
      url(url),
      referrer(referrer) {
}

// TabContentsDelegateImpl -----------------------------------------------------

class PrerenderContents::TabContentsDelegateImpl
    : public TabContentsDelegate {
 public:
  explicit TabContentsDelegateImpl(PrerenderContents* prerender_contents) :
      prerender_contents_(prerender_contents) {
  }

  // TabContentsDelegate implementation:
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE {
    add_page_vector_.push_back(
        scoped_refptr<history::HistoryAddPageArgs>(add_page_args.Clone()));
    return false;
  }

  virtual bool CanDownload(TabContents* source, int request_id) OVERRIDE {
    prerender_contents_->Destroy(FINAL_STATUS_DOWNLOAD);
    // Cancel the download.
    return false;
  }

  virtual void OnStartDownload(TabContents* source,
                               DownloadItem* download) OVERRIDE {
    // Prerendered pages should never be able to download files.
    NOTREACHED();
  }

  virtual bool OnGoToEntryOffset(int offset) OVERRIDE {
    // This isn't allowed because the history merge operation
    // does not work if there are renderer issued challenges.
    // TODO(cbentzel): Cancel in this case? May not need to do
    // since render-issued offset navigations are not guaranteed,
    // but indicates that the page cares about the history.
    return false;
  }

  virtual void JSOutOfMemory(TabContents* tab) OVERRIDE {
    prerender_contents_->OnJSOutOfMemory();
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return prerender_contents_->ShouldSuppressDialogs();
  }

  // Commits the History of Pages to the given TabContents.
  void CommitHistory(TabContentsWrapper* tab) {
    for (size_t i = 0; i < add_page_vector_.size(); ++i)
      tab->history_tab_helper()->UpdateHistoryForNavigation(
          add_page_vector_[i].get());
  }

 private:
  typedef std::vector<scoped_refptr<history::HistoryAddPageArgs> >
      AddPageVector;

  // Caches pages to be added to the history.
  AddPageVector add_page_vector_;

  PrerenderContents* prerender_contents_;
};

void PrerenderContents::AddPendingPrerender(Origin origin,
                                            const GURL& url,
                                            const GURL& referrer) {
  pending_prerender_list_.push_back(
      PendingPrerenderData(origin, url, referrer));
}

bool PrerenderContents::IsPendingEntry(const GURL& url) const {
  for (PendingPrerenderList::const_iterator it =
           pending_prerender_list_.begin();
       it != pending_prerender_list_.end();
       ++it) {
    if (it->url == url)
      return true;
  }
  return false;
}

void PrerenderContents::StartPendingPrerenders() {
  PendingPrerenderList pending_prerender_list;
  pending_prerender_list.swap(pending_prerender_list_);
  for (PendingPrerenderList::iterator it = pending_prerender_list.begin();
       it != pending_prerender_list.end();
       ++it) {
    prerender_manager_->AddPrerender(it->origin,
                                     std::make_pair(child_id_, route_id_),
                                     it->url,
                                     it->referrer,
                                     NULL);
  }
}

PrerenderContents::PrerenderContents(
    PrerenderManager* prerender_manager,
    PrerenderTracker* prerender_tracker,
    Profile* profile,
    const GURL& url,
    const GURL& referrer,
    Origin origin,
    uint8 experiment_id)
    : prerender_manager_(prerender_manager),
      prerender_tracker_(prerender_tracker),
      prerender_url_(url),
      referrer_(referrer),
      profile_(profile),
      page_id_(0),
      has_stopped_loading_(false),
      final_status_(FINAL_STATUS_MAX),
      prerendering_has_started_(false),
      prerendering_has_been_cancelled_(false),
      child_id_(-1),
      route_id_(-1),
      starting_page_id_(-1),
      origin_(origin),
      experiment_id_(experiment_id) {
  DCHECK(prerender_manager != NULL);
}

bool PrerenderContents::Init() {
  return AddAliasURL(prerender_url_);
}

// static
PrerenderContents::Factory* PrerenderContents::CreateFactory() {
  return new PrerenderContentsFactoryImpl();
}

void PrerenderContents::StartPrerendering(
    const RenderViewHost* source_render_view_host,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(profile_ != NULL);
  DCHECK(!prerendering_has_started_);
  DCHECK(prerender_contents_.get() == NULL);

  prerendering_has_started_ = true;
  TabContents* new_contents = new TabContents(profile_, NULL, MSG_ROUTING_NONE,
                                              NULL, session_storage_namespace);
  prerender_contents_.reset(new TabContentsWrapper(new_contents));
  TabContentsObserver::Observe(new_contents);

  gfx::Rect tab_bounds;
  if (source_render_view_host) {
    DCHECK(source_render_view_host->view() != NULL);
    TabContents* source_tc =
        source_render_view_host->delegate()->GetAsTabContents();
    if (source_tc) {
      // So that history merging will work, get the max page ID
      // of the old page as a starting id.
      starting_page_id_ = source_tc->GetMaxPageID();

      // Set the size of the new TC to that of the old TC.
      source_tc->view()->GetContainerBounds(&tab_bounds);
    }
  } else {
    int max_page_id = -1;
    // Get the largest page ID of all open tabs as a starting id.
    for (BrowserList::BrowserVector::const_iterator browser_iter =
            BrowserList::begin();
         browser_iter != BrowserList::end();
         ++browser_iter) {
      const Browser* browser = *browser_iter;
      int num_tabs = browser->tab_count();
      for (int tab_index = 0; tab_index < num_tabs; ++tab_index) {
        TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
        if (tab_contents != NULL)
          max_page_id = std::max(max_page_id, tab_contents->GetMaxPageID());
      }
    }
    starting_page_id_ = max_page_id;

    // Try to get the active tab of the active browser and use that for tab
    // bounds. If the browser has never been active, we will fail to get a size
    // but we shouldn't be prerendering in that case anyway.
    Browser* active_browser = BrowserList::GetLastActiveWithProfile(profile_);
    if (active_browser) {
      TabContents* active_tab_contents = active_browser->GetTabContentsAt(
          active_browser->active_index());
      active_tab_contents->view()->GetContainerBounds(&tab_bounds);
    }
  }

  // Add a safety margin of kPrerenderPageIdOffset to the starting page id (for
  // things such as redirects).
  if (starting_page_id_ < 0)
    starting_page_id_ = 0;
  starting_page_id_ += kPrerenderPageIdOffset;
  prerender_contents_->controller().set_max_restored_page_id(starting_page_id_);

  tab_contents_delegate_.reset(new TabContentsDelegateImpl(this));
  new_contents->set_delegate(tab_contents_delegate_.get());

  // Set the size of the prerender TabContents.
  prerender_contents_->view()->SizeContents(tab_bounds.size());

  // Register as an observer of the RenderViewHost so we get messages.
  render_view_host_observer_.reset(
      new PrerenderRenderViewHostObserver(this, render_view_host_mutable()));

  child_id_ = render_view_host()->process()->id();
  route_id_ = render_view_host()->routing_id();

  // Register this with the ResourceDispatcherHost as a prerender
  // RenderViewHost. This must be done before the Navigate message to catch all
  // resource requests, but as it is on the same thread as the Navigate message
  // (IO) there is no race condition.
  prerender_tracker_->OnPrerenderingStarted(
      child_id_,
      route_id_,
      prerender_manager_);

  // Close ourselves when the application is shutting down.
  notification_registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  // Register for our parent profile to shutdown, so we can shut ourselves down
  // as well (should only be called for OTR profiles, as we should receive
  // APP_TERMINATING before non-OTR profiles are destroyed).
  // TODO(tburkard): figure out if this is needed.
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile_));

  // Register to inform new RenderViews that we're prerendering.
  notification_registrar_.Add(
      this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
      content::Source<TabContents>(new_contents));

  // Register for redirect notifications sourced from |this|.
  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<RenderViewHostDelegate>(GetRenderViewHostDelegate()));

  // Register for new windows from any source.
  notification_registrar_.Add(
      this, content::NOTIFICATION_CREATING_NEW_WINDOW_CANCELLED,
      content::Source<TabContents>(new_contents));

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  content::PageTransition transition = content::PAGE_TRANSITION_LINK;
  if (origin_ == ORIGIN_OMNIBOX_ORIGINAL ||
      origin_ == ORIGIN_OMNIBOX_CONSERVATIVE) {
    transition = content::PAGE_TRANSITION_TYPED;
  }
  new_contents->controller().LoadURL(prerender_url_, referrer_, transition,
                                     std::string());
}

bool PrerenderContents::GetChildId(int* child_id) const {
  CHECK(child_id);
  DCHECK_GE(child_id_, -1);
  *child_id = child_id_;
  return child_id_ != -1;
}

bool PrerenderContents::GetRouteId(int* route_id) const {
  CHECK(route_id);
  DCHECK_GE(route_id_, -1);
  *route_id = route_id_;
  return route_id_ != -1;
}

void PrerenderContents::set_final_status(FinalStatus final_status) {
  DCHECK(final_status >= FINAL_STATUS_USED && final_status < FINAL_STATUS_MAX);
  DCHECK(final_status_ == FINAL_STATUS_MAX ||
         final_status_ == FINAL_STATUS_CONTROL_GROUP ||
         final_status_ == FINAL_STATUS_MATCH_COMPLETE_DUMMY);

  // Don't override final_status_ if it's FINAL_STATUS_CONTROL_GROUP or
  // FINAL_STATUS_MATCH_COMPLETE_DUMMY, otherwise data will be collected
  // in the Prerender.FinalStatus histogram.
  if (final_status_ == FINAL_STATUS_CONTROL_GROUP ||
      final_status_ == FINAL_STATUS_MATCH_COMPLETE_DUMMY)
    return;

  final_status_ = final_status;
}

PrerenderContents::~PrerenderContents() {
  DCHECK(final_status_ != FINAL_STATUS_MAX);
  DCHECK(prerendering_has_been_cancelled_ ||
         final_status_ == FINAL_STATUS_USED ||
         final_status_ == FINAL_STATUS_CONTROL_GROUP ||
         final_status_ == FINAL_STATUS_MATCH_COMPLETE_DUMMY);
  DCHECK(origin_ != ORIGIN_MAX);

  // If we haven't even started prerendering, we were just in the control
  // group (or a match complete dummy), which means we do not want to record
  // the status.
  if (prerendering_has_started())
    prerender_manager_->RecordFinalStatus(origin_, experiment_id_,
                                          final_status_);

  if (child_id_ != -1 && route_id_ != -1)
    prerender_tracker_->OnPrerenderingFinished(child_id_, route_id_);

  // If we still have a TabContents, clean up anything we need to and then
  // destroy it.
  if (prerender_contents_.get())
    delete ReleasePrerenderContents();

  // The following URLs are no longer rendering.
  prerender_tracker_->RemovePrerenderURLsOnUIThread(alias_urls_);
}

void PrerenderContents::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      Destroy(FINAL_STATUS_PROFILE_DESTROYED);
      return;

    case content::NOTIFICATION_APP_TERMINATING:
      Destroy(FINAL_STATUS_APP_TERMINATING);
      return;

    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      // RESOURCE_RECEIVED_REDIRECT can come for any resource on a page.
      // If it's a redirect on the top-level resource, the name needs
      // to be remembered for future matching, and if it redirects to
      // an https resource, it needs to be canceled. If a subresource
      // is redirected, nothing changes.
      DCHECK(content::Source<RenderViewHostDelegate>(source).ptr() ==
             GetRenderViewHostDelegate());
      ResourceRedirectDetails* resource_redirect_details =
          content::Details<ResourceRedirectDetails>(details).ptr();
      CHECK(resource_redirect_details);
      if (resource_redirect_details->resource_type() ==
          ResourceType::MAIN_FRAME) {
        if (!AddAliasURL(resource_redirect_details->new_url()))
          return;
      }
      break;
    }

    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      if (prerender_contents_.get()) {
        DCHECK_EQ(content::Source<TabContents>(source).ptr(),
                  prerender_contents_->tab_contents());

        content::Details<RenderViewHost> new_render_view_host(details);
        OnRenderViewHostCreated(new_render_view_host.ptr());

        // When a new RenderView is created for a prerendering TabContents,
        // tell the new RenderView it's being used for prerendering before any
        // navigations occur.  Note that this is always triggered before the
        // first navigation, so there's no need to send the message just after
        // the TabContents is created.
        new_render_view_host->Send(
            new ChromeViewMsg_SetIsPrerendering(
                new_render_view_host->routing_id(),
                true));

        // Make sure the size of the RenderViewHost has been passed to the new
        // RenderView.  Otherwise, the size may not be sent until the
        // RenderViewReady event makes it from the render process to the UI
        // thread of the browser process.  When the RenderView receives its
        // size, is also sets itself to be visible, which would then break the
        // visibility API.
        new_render_view_host->WasResized();
        prerender_contents_->tab_contents()->HideContents();
      }
      break;
    }

    case content::NOTIFICATION_CREATING_NEW_WINDOW_CANCELLED: {
      if (prerender_contents_.get()) {
        CHECK(content::Source<TabContents>(source).ptr() ==
              prerender_contents_->tab_contents());
        // Since we don't want to permit child windows that would have a
        // window.opener property, terminate prerendering.
        Destroy(FINAL_STATUS_CREATE_NEW_WINDOW);
      }
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void PrerenderContents::OnRenderViewHostCreated(
    RenderViewHost* new_render_view_host) {
}

void PrerenderContents::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& urls) {
  VLOG(1) << "PrerenderContents::OnUpdateFaviconURL" << icon_url_;
  for (std::vector<FaviconURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    if (it->icon_type == FaviconURL::FAVICON) {
      icon_url_ = it->icon_url;
      VLOG(1) << icon_url_;
      return;
    }
  }
}

bool PrerenderContents::AddAliasURL(const GURL& url) {
  const bool http = url.SchemeIs(chrome::kHttpScheme);
  const bool https = url.SchemeIs(chrome::kHttpsScheme);
  if (!(http || https)) {
    Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return false;
  }
  if (https && !prerender_manager_->config().https_allowed) {
    Destroy(FINAL_STATUS_HTTPS);
    return false;
  }
  if (prerender_manager_->HasRecentlyBeenNavigatedTo(url)) {
    Destroy(FINAL_STATUS_RECENTLY_VISITED);
    return false;
  }

  alias_urls_.push_back(url);
  prerender_tracker_->AddPrerenderURLOnUIThread(url);
  return true;
}

void PrerenderContents::AddAliasURLsFromOtherPrerenderContents(
    PrerenderContents* other_pc) {
  for (std::vector<GURL>::const_iterator it = other_pc->alias_urls_.begin();
       it != other_pc->alias_urls_.end();
       ++it) {
    alias_urls_.push_back(*it);
    prerender_tracker_->AddPrerenderURLOnUIThread(*it);
  }
}

bool PrerenderContents::MatchesURL(const GURL& url, GURL* matching_url) const {
  std::vector<GURL>::const_iterator matching_url_iterator =
      std::find_if(alias_urls_.begin(),
                   alias_urls_.end(),
                   PrerenderURLPredicate(url));
  if (matching_url_iterator != alias_urls_.end()) {
    if (matching_url)
      *matching_url = *matching_url_iterator;
    return true;
  }
  return false;
}

void PrerenderContents::OnJSOutOfMemory() {
  Destroy(FINAL_STATUS_JS_OUT_OF_MEMORY);
}

void PrerenderContents::RenderViewGone(base::TerminationStatus status) {
  Destroy(FINAL_STATUS_RENDERER_CRASHED);
}

void PrerenderContents::DidStopLoading() {
  has_stopped_loading_ = true;
}

void PrerenderContents::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    RenderViewHost* render_view_host) {
  if (is_main_frame) {
    if (!AddAliasURL(validated_url))
      return;

    // Usually, this event fires if the user clicks or enters a new URL.
    // Neither of these can happen in the case of an invisible prerender.
    // So the cause is: Some JavaScript caused a new URL to be loaded.  In that
    // case, the spinner would start again in the browser, so we must reset
    // has_stopped_loading_ so that the spinner won't be stopped.
    has_stopped_loading_ = false;
  }
}

bool PrerenderContents::ShouldSuppressDialogs() {
  // Always suppress JavaScript messages if they're triggered by a page being
  // prerendered.
  // We still want to show the user the message when they navigate to this
  // page, so cancel this prerender.
  Destroy(FINAL_STATUS_JAVASCRIPT_ALERT);
  return true;
}

void PrerenderContents::Destroy(FinalStatus final_status) {
  if (prerendering_has_been_cancelled_)
    return;

  if (child_id_ != -1 && route_id_ != -1) {
    // Cancel the prerender in the PrerenderTracker.  This is needed
    // because destroy may be called directly from the UI thread without calling
    // TryCancel().  This is difficult to completely avoid, since prerendering
    // can be cancelled before a RenderView is created.
    bool is_cancelled = prerender_tracker_->TryCancel(
        child_id_, route_id_, final_status);
    CHECK(is_cancelled);

    // A different final status may have been set already from another thread.
    // If so, use it instead.
    if (!prerender_tracker_->GetFinalStatus(child_id_, route_id_,
                                            &final_status)) {
      NOTREACHED();
    }
  }
  set_final_status(final_status);

  prerendering_has_been_cancelled_ = true;
  // This has to be done after setting the final status, as it adds the
  // prerender to the history.
  prerender_manager_->MoveEntryToPendingDelete(this, final_status);

  // We may destroy the PrerenderContents before we have initialized the
  // RenderViewHost. Otherwise set the Observer's PrerenderContents to NULL to
  // avoid any more messages being sent.
  if (render_view_host_observer_.get())
    render_view_host_observer_->set_prerender_contents(NULL);
}

base::ProcessMetrics* PrerenderContents::MaybeGetProcessMetrics() {
  if (process_metrics_.get() == NULL) {
    // If a PrenderContents hasn't started prerending, don't be fully formed.
    if (!render_view_host() || !render_view_host()->process())
      return NULL;
    base::ProcessHandle handle = render_view_host()->process()->GetHandle();
    if (handle == base::kNullProcessHandle)
      return NULL;
#if !defined(OS_MACOSX)
    process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
#else
    process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(
        handle,
        MachBroker::GetInstance()));
#endif
  }

  return process_metrics_.get();
}

void PrerenderContents::DestroyWhenUsingTooManyResources() {
  base::ProcessMetrics* metrics = MaybeGetProcessMetrics();
  if (metrics == NULL)
    return;

  size_t private_bytes, shared_bytes;
  if (metrics->GetMemoryBytes(&private_bytes, &shared_bytes) &&
      private_bytes > prerender_manager_->config().max_bytes) {
      Destroy(FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);
  }
}

TabContentsWrapper* PrerenderContents::ReleasePrerenderContents() {
  prerender_contents_->tab_contents()->set_delegate(NULL);
  render_view_host_observer_.reset();
  TabContentsObserver::Observe(NULL);
  return prerender_contents_.release();
}

RenderViewHostDelegate* PrerenderContents::GetRenderViewHostDelegate() {
  if (!prerender_contents_.get())
    return NULL;
  return prerender_contents_->tab_contents();
}

RenderViewHost* PrerenderContents::render_view_host_mutable() {
  return const_cast<RenderViewHost*>(render_view_host());
}

const RenderViewHost* PrerenderContents::render_view_host() const {
  if (!prerender_contents_.get())
    return NULL;
  return prerender_contents_->render_view_host();
}

void PrerenderContents::CommitHistory(TabContentsWrapper* tab) {
  if (tab_contents_delegate_.get())
    tab_contents_delegate_->CommitHistory(tab);
}

Value* PrerenderContents::GetAsValue() const {
  if (!prerender_contents_.get())
    return NULL;
  DictionaryValue* dict_value = new DictionaryValue();
  dict_value->SetString("url", prerender_url_.spec());
  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta duration = current_time - load_start_time_;
  dict_value->SetInteger("duration", duration.InSeconds());
  return dict_value;
}

bool PrerenderContents::IsCrossSiteNavigationPending() const {
  if (!prerender_contents_.get() || !prerender_contents_->tab_contents())
    return false;
  const TabContents* tab_contents = prerender_contents_->tab_contents();
  return (tab_contents->GetSiteInstance() !=
          tab_contents->GetPendingSiteInstance());
}


}  // namespace prerender
