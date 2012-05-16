// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_contents.h"

#include <algorithm>
#include <utility>

#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_render_view_host_observer.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/rect.h"

using content::DownloadItem;
using content::OpenURLParams;
using content::RenderViewHost;
using content::ResourceRedirectDetails;
using content::WebContents;

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

// Tells the render process at |child_id| whether |url| is a new prerendered
// page, or whether |url| is being removed as a prerendered page. Currently
// this will only inform the render process that created the prerendered page
// with <link rel="prerender"> tags about it. This means that if the user
// clicks on a link for a prerendered URL in a different page, the prerender
// will not be swapped in.
void InformRenderProcessAboutPrerender(const GURL& url,
                                       bool is_add,
                                       int child_id) {
  if (child_id < 0)
    return;
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(child_id);
  if (!render_process_host)
    return;
  IPC::Message* message = NULL;
  if (is_add)
    message = new PrerenderMsg_AddPrerenderURL(url);
  else
    message = new PrerenderMsg_RemovePrerenderURL(url);
  render_process_host->Send(message);
}

}  // namespace

class PrerenderContentsFactoryImpl : public PrerenderContents::Factory {
 public:
  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, PrerenderTracker* prerender_tracker,
      Profile* profile, const GURL& url, const content::Referrer& referrer,
      Origin origin, uint8 experiment_id) OVERRIDE {
    return new PrerenderContents(prerender_manager, prerender_tracker, profile,
                                 url, referrer, origin, experiment_id);
  }
};

struct PrerenderContents::PendingPrerenderInfo {
  PendingPrerenderInfo(const GURL& url,
                       const content::Referrer& referrer,
                       const gfx::Size& size);
  const GURL url;
  const content::Referrer referrer;
  const gfx::Size size;
};

PrerenderContents::PendingPrerenderInfo::PendingPrerenderInfo(
    const GURL& url,
    const content::Referrer& referrer,
    const gfx::Size& size)
    : url(url),
      referrer(referrer),
      size(size) {
}

// TabContentsDelegateImpl -----------------------------------------------------

class PrerenderContents::TabContentsDelegateImpl
    : public content::WebContentsDelegate {
 public:
  explicit TabContentsDelegateImpl(PrerenderContents* prerender_contents) :
      prerender_contents_(prerender_contents) {
  }

 virtual WebContents* OpenURLFromTab(WebContents* source,
                                     const OpenURLParams& params) OVERRIDE {
    // |OpenURLFromTab| is typically called when a frame performs a navigation
    // that requires the browser to perform the transition instead of WebKit.
    // Examples include prerendering a site that redirects to an app URL,
    // or if --enable-strict-site-isolation is specified and the prerendered
    // frame redirects to a different origin.
    // TODO(cbentzel): Consider supporting this is if it is a common case
    // during prerenders.
    prerender_contents_->Destroy(FINAL_STATUS_OPEN_URL);
    return NULL;
  }

  // content::WebContentsDelegate implementation:
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE {
    add_page_vector_.push_back(
        scoped_refptr<history::HistoryAddPageArgs>(add_page_args.Clone()));
    return false;
  }

  virtual bool CanDownload(RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method) OVERRIDE {
    prerender_contents_->Destroy(FINAL_STATUS_DOWNLOAD);
    // Cancel the download.
    return false;
  }

  virtual void OnStartDownload(WebContents* source,
                               DownloadItem* download) OVERRIDE {
    // Prerendered pages should never be able to download files.
    NOTREACHED();
  }

  virtual bool ShouldCreateWebContents(
      WebContents* web_contents,
      int route_id,
      WindowContainerType window_container_type,
      const string16& frame_name,
      const GURL& target_url) OVERRIDE {
    // Since we don't want to permit child windows that would have a
    // window.opener property, terminate prerendering.
    prerender_contents_->Destroy(FINAL_STATUS_CREATE_NEW_WINDOW);
    // Cancel the popup.
    return false;
  }

  virtual bool OnGoToEntryOffset(int offset) OVERRIDE {
    // This isn't allowed because the history merge operation
    // does not work if there are renderer issued challenges.
    // TODO(cbentzel): Cancel in this case? May not need to do
    // since render-issued offset navigations are not guaranteed,
    // but indicates that the page cares about the history.
    return false;
  }

  virtual void JSOutOfMemory(WebContents* tab) OVERRIDE {
    prerender_contents_->OnJSOutOfMemory();
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return prerender_contents_->ShouldSuppressDialogs();
  }

  // Commits the History of Pages to the given TabContentsWrapper.
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

void PrerenderContents::AddPendingPrerender(const GURL& url,
                                            const content::Referrer& referrer,
                                            const gfx::Size& size) {
  pending_prerender_list_.push_back(PendingPrerenderInfo(url, referrer, size));
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
    prerender_manager_->AddPrerenderFromLinkRelPrerender(
        child_id_, route_id_, it->url, it->referrer, it->size);
  }
}

PrerenderContents::PrerenderContents(
    PrerenderManager* prerender_manager,
    PrerenderTracker* prerender_tracker,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin,
    uint8 experiment_id)
    : prerendering_has_started_(false),
      prerender_manager_(prerender_manager),
      prerender_tracker_(prerender_tracker),
      prerender_url_(url),
      referrer_(referrer),
      profile_(profile),
      page_id_(0),
      has_stopped_loading_(false),
      has_finished_loading_(false),
      final_status_(FINAL_STATUS_MAX),
      match_complete_status_(MATCH_COMPLETE_DEFAULT),
      prerendering_has_been_cancelled_(false),
      child_id_(-1),
      route_id_(-1),
      origin_(origin),
      experiment_id_(experiment_id),
      creator_child_id_(-1) {
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
    int creator_child_id,
    const gfx::Size& size,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK(profile_ != NULL);
  DCHECK(!prerendering_has_started_);
  DCHECK(prerender_contents_.get() == NULL);
  DCHECK_EQ(-1, creator_child_id_);
  DCHECK(size_.IsEmpty());
  DCHECK_EQ(1U, alias_urls_.size());

  creator_child_id_ = creator_child_id;

  size_ = size;
  if (size_.IsEmpty()) {
    size_ = prerender_manager_->config().default_tab_bounds.size();
#if !defined(OS_ANDROID)
    // Try to get the active tab of the active browser and use that for tab
    // bounds. If the browser has never been active, we will fail to get a size
    // but we shouldn't be prerendering in that case anyway.
    //
    // This code is unneeded on Android as we do not have a Browser object so we
    // can't get the size, and |default_tab_bounds| will be set to the right
    // value.
    if (Browser* active_browser =
            browser::FindLastActiveWithProfile(profile_)) {
      if (WebContents* active_web_contents = active_browser->GetWebContentsAt(
              active_browser->active_index())) {
        gfx::Rect container_bounds;
        active_web_contents->GetView()->GetContainerBounds(&container_bounds);
        size_ = container_bounds.size();
      }
    }
#endif  // !defined(OS_ANDROID)
  }

  prerendering_has_started_ = true;
  InformRenderProcessAboutPrerender(prerender_url_, true,
                                    creator_child_id_);

  WebContents* new_contents = CreateWebContents(session_storage_namespace);
  prerender_contents_.reset(new TabContentsWrapper(new_contents));
  content::WebContentsObserver::Observe(new_contents);

  tab_contents_delegate_.reset(new TabContentsDelegateImpl(this));
  new_contents->SetDelegate(tab_contents_delegate_.get());

  // Set the size of the prerender WebContents.
  prerender_contents_->web_contents()->GetView()->SizeContents(size_);

  // Register as an observer of the RenderViewHost so we get messages.
  render_view_host_observer_.reset(
      new PrerenderRenderViewHostObserver(this, GetRenderViewHostMutable()));

  child_id_ = GetRenderViewHost()->GetProcess()->GetID();
  route_id_ = GetRenderViewHost()->GetRoutingID();

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
      content::Source<WebContents>(new_contents));

  // Register for redirect notifications sourced from |this|.
  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<WebContents>(GetWebContents()));

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  new_contents->GetController().LoadURL(
      prerender_url_,
      referrer_,
      (origin_ == ORIGIN_OMNIBOX ? content::PAGE_TRANSITION_TYPED :
                                   content::PAGE_TRANSITION_LINK),
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
  DCHECK(final_status_ == FINAL_STATUS_MAX);

  final_status_ = final_status;
}

PrerenderContents::~PrerenderContents() {
  DCHECK(final_status_ != FINAL_STATUS_MAX);
  DCHECK(prerendering_has_been_cancelled_ ||
         final_status_ == FINAL_STATUS_USED);
  DCHECK(origin_ != ORIGIN_MAX);

  prerender_manager_->RecordFinalStatusWithMatchCompleteStatus(
      origin_,
      experiment_id_,
      match_complete_status_,
      final_status_);

  if (child_id_ != -1 && route_id_ != -1) {
    prerender_tracker_->OnPrerenderingFinished(child_id_, route_id_);
    for (std::vector<GURL>::const_iterator it = alias_urls_.begin();
         it != alias_urls_.end();
         ++it) {
      InformRenderProcessAboutPrerender(*it, false, creator_child_id_);
    }
  }

  // If we still have a WebContents, clean up anything we need to and then
  // destroy it.
  if (prerender_contents_.get())
    delete ReleasePrerenderContents();
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
      DCHECK(content::Source<WebContents>(source).ptr() == GetWebContents());
      ResourceRedirectDetails* resource_redirect_details =
          content::Details<ResourceRedirectDetails>(details).ptr();
      CHECK(resource_redirect_details);
      if (resource_redirect_details->resource_type ==
          ResourceType::MAIN_FRAME) {
        if (!AddAliasURL(resource_redirect_details->new_url))
          return;
      }
      break;
    }

    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      if (prerender_contents_.get()) {
        DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                  prerender_contents_->web_contents());

        content::Details<RenderViewHost> new_render_view_host(details);
        OnRenderViewHostCreated(new_render_view_host.ptr());

        // When a new RenderView is created for a prerendering WebContents,
        // tell the new RenderView it's being used for prerendering before any
        // navigations occur.  Note that this is always triggered before the
        // first navigation, so there's no need to send the message just after
        // the WebContents is created.
        new_render_view_host->Send(
            new PrerenderMsg_SetIsPrerendering(
                new_render_view_host->GetRoutingID(),
                true));

        // Make sure the size of the RenderViewHost has been passed to the new
        // RenderView.  Otherwise, the size may not be sent until the
        // RenderViewReady event makes it from the render process to the UI
        // thread of the browser process.  When the RenderView receives its
        // size, is also sets itself to be visible, which would then break the
        // visibility API.
        new_render_view_host->WasResized();
        // TODO(tburkard): Put this back in once pixel counting experiments
        // are done.
        //        prerender_contents_->web_contents()->HideContents();
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

WebContents* PrerenderContents::CreateWebContents(
    content::SessionStorageNamespace* session_storage_namespace) {
  return  WebContents::Create(profile_, NULL, MSG_ROUTING_NONE, NULL,
                              session_storage_namespace);
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
  InformRenderProcessAboutPrerender(url, true, creator_child_id_);
  return true;
}

void PrerenderContents::AddAliasURLsFromOtherPrerenderContents(
    PrerenderContents* other_pc) {
  for (std::vector<GURL>::const_iterator it = other_pc->alias_urls_.begin();
       it != other_pc->alias_urls_.end();
       ++it) {
    alias_urls_.push_back(*it);
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
    has_finished_loading_ = false;
  }
}

void PrerenderContents::DidFinishLoad(int64 frame_id,
                                      const GURL& validated_url,
                                      bool is_main_frame) {
  if (is_main_frame)
    has_finished_loading_ = true;
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
    if (!GetRenderViewHost() || !GetRenderViewHost()->GetProcess())
      return NULL;
    base::ProcessHandle handle = GetRenderViewHost()->GetProcess()->GetHandle();
    if (handle == base::kNullProcessHandle)
      return NULL;
#if !defined(OS_MACOSX)
    process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
#else
    process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(
        handle,
        content::BrowserChildProcessHost::GetPortProvider()));
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
  prerender_contents_->web_contents()->SetDelegate(NULL);
  render_view_host_observer_.reset();
  content::WebContentsObserver::Observe(NULL);
  return prerender_contents_.release();
}

WebContents* PrerenderContents::GetWebContents() {
  if (!prerender_contents_.get())
    return NULL;
  return prerender_contents_->web_contents();
}

RenderViewHost* PrerenderContents::GetRenderViewHostMutable() {
  return const_cast<RenderViewHost*>(GetRenderViewHost());
}

const RenderViewHost* PrerenderContents::GetRenderViewHost() const {
  if (!prerender_contents_.get())
    return NULL;
  return prerender_contents_->web_contents()->GetRenderViewHost();
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
  if (!prerender_contents_.get() || !prerender_contents_->web_contents())
    return false;
  const WebContents* web_contents = prerender_contents_->web_contents();
  return (web_contents->GetSiteInstance() !=
          web_contents->GetPendingSiteInstance());
}


}  // namespace prerender
