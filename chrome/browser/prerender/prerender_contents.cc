// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_contents.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "apps/ui/web_contents_sizer.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/rect.h"

using content::BrowserThread;
using content::DownloadItem;
using content::OpenURLParams;
using content::RenderViewHost;
using content::ResourceRedirectDetails;
using content::ResourceType;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

namespace {

// Internal cookie event.
// Whenever a prerender interacts with the cookie store, either sending
// existing cookies that existed before the prerender started, or when a cookie
// is changed, we record these events for histogramming purposes.
enum InternalCookieEvent {
  INTERNAL_COOKIE_EVENT_MAIN_FRAME_SEND = 0,
  INTERNAL_COOKIE_EVENT_MAIN_FRAME_CHANGE = 1,
  INTERNAL_COOKIE_EVENT_OTHER_SEND = 2,
  INTERNAL_COOKIE_EVENT_OTHER_CHANGE = 3,
  INTERNAL_COOKIE_EVENT_MAX
};

// Indicates whether existing cookies were sent, and if they were third party
// cookies, and whether they were for blocking resources.
// Each value may be inclusive of previous values. We only care about the
// value with the highest index that has ever occurred in the course of a
// prerender.
enum CookieSendType {
  COOKIE_SEND_TYPE_NONE = 0,
  COOKIE_SEND_TYPE_FIRST_PARTY = 1,
  COOKIE_SEND_TYPE_THIRD_PARTY = 2,
  COOKIE_SEND_TYPE_THIRD_PARTY_BLOCKING_RESOURCE = 3,
  COOKIE_SEND_TYPE_MAX
};

void ResumeThrottles(
    std::vector<base::WeakPtr<PrerenderResourceThrottle> > throttles) {
  for (size_t i = 0; i < throttles.size(); i++) {
    if (throttles[i])
      throttles[i]->Resume();
  }
}

}  // namespace

// static
const int PrerenderContents::kNumCookieStatuses =
    (1 << INTERNAL_COOKIE_EVENT_MAX);

// static
const int PrerenderContents::kNumCookieSendTypes = COOKIE_SEND_TYPE_MAX;

class PrerenderContentsFactoryImpl : public PrerenderContents::Factory {
 public:
  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile,
      const GURL& url, const content::Referrer& referrer,
      Origin origin, uint8 experiment_id) OVERRIDE {
    return new PrerenderContents(prerender_manager, profile,
                                 url, referrer, origin, experiment_id);
  }
};

// WebContentsDelegateImpl -----------------------------------------------------

class PrerenderContents::WebContentsDelegateImpl
    : public content::WebContentsDelegate {
 public:
  explicit WebContentsDelegateImpl(PrerenderContents* prerender_contents)
      : prerender_contents_(prerender_contents) {
  }

  // content::WebContentsDelegate implementation:
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params) OVERRIDE {
    // |OpenURLFromTab| is typically called when a frame performs a navigation
    // that requires the browser to perform the transition instead of WebKit.
    // Examples include prerendering a site that redirects to an app URL,
    // or if --enable-strict-site-isolation is specified and the prerendered
    // frame redirects to a different origin.
    // TODO(cbentzel): Consider supporting this if it is a common case during
    // prerenders.
    prerender_contents_->Destroy(FINAL_STATUS_OPEN_URL);
    return NULL;
  }

  virtual void CloseContents(content::WebContents* contents) OVERRIDE {
    prerender_contents_->Destroy(FINAL_STATUS_CLOSED);
  }

  virtual void CanDownload(
      RenderViewHost* render_view_host,
      const GURL& url,
      const std::string& request_method,
      const base::Callback<void(bool)>& callback) OVERRIDE {
    prerender_contents_->Destroy(FINAL_STATUS_DOWNLOAD);
    // Cancel the download.
    callback.Run(false);
  }

   virtual bool ShouldCreateWebContents(
       WebContents* web_contents,
       int route_id,
       WindowContainerType window_container_type,
       const base::string16& frame_name,
       const GURL& target_url,
       const std::string& partition_id,
       SessionStorageNamespace* session_storage_namespace) OVERRIDE {
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

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    // We still want to show the user the message when they navigate to this
    // page, so cancel this prerender.
    prerender_contents_->Destroy(FINAL_STATUS_JAVASCRIPT_ALERT);
    // Always suppress JavaScript messages if they're triggered by a page being
    // prerendered.
    return true;
  }

  virtual void RegisterProtocolHandler(WebContents* web_contents,
                                       const std::string& protocol,
                                       const GURL& url,
                                       bool user_gesture) OVERRIDE {
    // TODO(mmenke): Consider supporting this if it is a common case during
    // prerenders.
    prerender_contents_->Destroy(FINAL_STATUS_REGISTER_PROTOCOL_HANDLER);
  }

  virtual gfx::Size GetSizeForNewRenderView(
      WebContents* web_contents) const OVERRIDE {
    // Have to set the size of the RenderView on initialization to be sure it is
    // set before the RenderView is hidden on all platforms (esp. Android).
    return prerender_contents_->size_;
  }

 private:
  PrerenderContents* prerender_contents_;
};

void PrerenderContents::Observer::OnPrerenderStopLoading(
    PrerenderContents* contents) {
}

void PrerenderContents::Observer::OnPrerenderDomContentLoaded(
    PrerenderContents* contents) {
}

void PrerenderContents::Observer::OnPrerenderCreatedMatchCompleteReplacement(
    PrerenderContents* contents, PrerenderContents* replacement) {
}

PrerenderContents::Observer::Observer() {
}

PrerenderContents::Observer::~Observer() {
}

PrerenderContents::PrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin,
    uint8 experiment_id)
    : prerendering_has_started_(false),
      session_storage_namespace_id_(-1),
      prerender_manager_(prerender_manager),
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
      creator_child_id_(-1),
      cookie_status_(0),
      cookie_send_type_(COOKIE_SEND_TYPE_NONE),
      network_bytes_(0) {
  DCHECK(prerender_manager != NULL);
}

PrerenderContents* PrerenderContents::CreateMatchCompleteReplacement() {
  PrerenderContents* new_contents = prerender_manager_->CreatePrerenderContents(
      prerender_url(), referrer(), origin(), experiment_id());

  new_contents->load_start_time_ = load_start_time_;
  new_contents->session_storage_namespace_id_ = session_storage_namespace_id_;
  new_contents->set_match_complete_status(
      PrerenderContents::MATCH_COMPLETE_REPLACEMENT_PENDING);

  const bool did_init = new_contents->Init();
  DCHECK(did_init);
  DCHECK_EQ(alias_urls_.front(), new_contents->alias_urls_.front());
  DCHECK_EQ(1u, new_contents->alias_urls_.size());
  new_contents->alias_urls_ = alias_urls_;
  // Erase all but the first alias URL; the replacement has adopted the
  // remainder without increasing the renderer-side reference count.
  alias_urls_.resize(1);
  new_contents->set_match_complete_status(
      PrerenderContents::MATCH_COMPLETE_REPLACEMENT);
  NotifyPrerenderCreatedMatchCompleteReplacement(new_contents);
  return new_contents;
}

bool PrerenderContents::Init() {
  return AddAliasURL(prerender_url_);
}

// static
PrerenderContents::Factory* PrerenderContents::CreateFactory() {
  return new PrerenderContentsFactoryImpl();
}

// static
PrerenderContents* PrerenderContents::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;
  PrerenderManager* prerender_manager = PrerenderManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!prerender_manager)
    return NULL;
  return prerender_manager->GetPrerenderContents(web_contents);
}

void PrerenderContents::StartPrerendering(
    int creator_child_id,
    const gfx::Size& size,
    SessionStorageNamespace* session_storage_namespace,
    net::URLRequestContextGetter* request_context) {
  DCHECK(profile_ != NULL);
  DCHECK(!size.IsEmpty());
  DCHECK(!prerendering_has_started_);
  DCHECK(prerender_contents_.get() == NULL);
  DCHECK_EQ(-1, creator_child_id_);
  DCHECK(size_.IsEmpty());
  DCHECK_EQ(1U, alias_urls_.size());

  creator_child_id_ = creator_child_id;
  session_storage_namespace_id_ = session_storage_namespace->id();
  size_ = size;

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();
  start_time_ = base::Time::Now();

  // Everything after this point sets up the WebContents object and associated
  // RenderView for the prerender page. Don't do this for members of the
  // control group.
  if (prerender_manager_->IsControlGroup(experiment_id()))
    return;

  if (origin_ == ORIGIN_LOCAL_PREDICTOR &&
      IsLocalPredictorPrerenderAlwaysControlEnabled()) {
    return;
  }

  prerendering_has_started_ = true;

  alias_session_storage_namespace = session_storage_namespace->CreateAlias();
  prerender_contents_.reset(
      CreateWebContents(alias_session_storage_namespace.get()));
  TabHelpers::AttachTabHelpers(prerender_contents_.get());
  content::WebContentsObserver::Observe(prerender_contents_.get());

  web_contents_delegate_.reset(new WebContentsDelegateImpl(this));
  prerender_contents_.get()->SetDelegate(web_contents_delegate_.get());
  // Set the size of the prerender WebContents.
  apps::ResizeWebContents(prerender_contents_.get(), size_);

  child_id_ = GetRenderViewHost()->GetProcess()->GetID();
  route_id_ = GetRenderViewHost()->GetRoutingID();

  // Log transactions to see if we could merge session storage namespaces in
  // the event of a mismatch.
  alias_session_storage_namespace->AddTransactionLogProcessId(child_id_);

  // Add the RenderProcessHost to the Prerender Manager.
  prerender_manager()->AddPrerenderProcessHost(
      GetRenderViewHost()->GetProcess());

  // In the prerender tracker, create a Prerender Cookie Store to keep track of
  // cookie changes performed by the prerender. Once the prerender is shown,
  // the cookie changes will be committed to the actual cookie store,
  // otherwise, they will be discarded.
  // If |request_context| is NULL, the feature must be disabled, so the
  // operation will not be performed.
  if (request_context) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PrerenderTracker::AddPrerenderCookieStoreOnIOThread,
                   base::Unretained(prerender_manager()->prerender_tracker()),
                   GetRenderViewHost()->GetProcess()->GetID(),
                   make_scoped_refptr(request_context),
                   base::Bind(&PrerenderContents::Destroy,
                              AsWeakPtr(),
                              FINAL_STATUS_COOKIE_CONFLICT)));
  }

  NotifyPrerenderStart();

  // Close ourselves when the application is shutting down.
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  // Register to inform new RenderViews that we're prerendering.
  notification_registrar_.Add(
      this, content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
      content::Source<WebContents>(prerender_contents_.get()));

  // Transfer over the user agent override.
  prerender_contents_.get()->SetUserAgentOverride(
      prerender_manager_->config().user_agent_override);

  content::NavigationController::LoadURLParams load_url_params(
      prerender_url_);
  load_url_params.referrer = referrer_;
  load_url_params.transition_type = content::PAGE_TRANSITION_LINK;
  if (origin_ == ORIGIN_OMNIBOX) {
    load_url_params.transition_type = content::PageTransitionFromInt(
        content::PAGE_TRANSITION_TYPED |
        content::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  } else if (origin_ == ORIGIN_INSTANT) {
    load_url_params.transition_type = content::PageTransitionFromInt(
        content::PAGE_TRANSITION_GENERATED |
        content::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  }
  load_url_params.override_user_agent =
      prerender_manager_->config().is_overriding_user_agent ?
      content::NavigationController::UA_OVERRIDE_TRUE :
      content::NavigationController::UA_OVERRIDE_FALSE;
  prerender_contents_.get()->GetController().LoadURLWithParams(load_url_params);
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

void PrerenderContents::SetFinalStatus(FinalStatus final_status) {
  DCHECK_GE(final_status, FINAL_STATUS_USED);
  DCHECK_LT(final_status, FINAL_STATUS_MAX);

  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);

  final_status_ = final_status;
}

PrerenderContents::~PrerenderContents() {
  DCHECK_NE(FINAL_STATUS_MAX, final_status());
  DCHECK(
      prerendering_has_been_cancelled() || final_status() == FINAL_STATUS_USED);
  DCHECK_NE(ORIGIN_MAX, origin());
  // Since a lot of prerenders terminate before any meaningful cookie action
  // would have happened, only record the cookie status for prerenders who
  // were used, cancelled, or timed out.
  if (prerendering_has_started_ && final_status() == FINAL_STATUS_USED) {
    prerender_manager_->RecordCookieStatus(origin(), experiment_id(),
                                           cookie_status_);
    prerender_manager_->RecordCookieSendType(origin(), experiment_id(),
                                             cookie_send_type_);
  }
  prerender_manager_->RecordFinalStatusWithMatchCompleteStatus(
      origin(), experiment_id(), match_complete_status(), final_status());

  bool used = final_status() == FINAL_STATUS_USED ||
              final_status() == FINAL_STATUS_WOULD_HAVE_BEEN_USED;
  prerender_manager_->RecordNetworkBytes(origin(), used, network_bytes_);

  // Broadcast the removal of aliases.
  for (content::RenderProcessHost::iterator host_iterator =
           content::RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd();
       host_iterator.Advance()) {
    content::RenderProcessHost* host = host_iterator.GetCurrentValue();
    host->Send(new PrerenderMsg_OnPrerenderRemoveAliases(alias_urls_));
  }

  // If we still have a WebContents, clean up anything we need to and then
  // destroy it.
  if (prerender_contents_.get())
    delete ReleasePrerenderContents();
}

void PrerenderContents::AddObserver(Observer* observer) {
  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);
  observer_list_.AddObserver(observer);
}

void PrerenderContents::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PrerenderContents::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    // TODO(davidben): Try to remove this in favor of relying on
    // FINAL_STATUS_PROFILE_DESTROYED.
    case chrome::NOTIFICATION_APP_TERMINATING:
      Destroy(FINAL_STATUS_APP_TERMINATING);
      return;

    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
      if (prerender_contents_.get()) {
        DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                  prerender_contents_.get());

        content::Details<RenderViewHost> new_render_view_host(details);
        OnRenderViewHostCreated(new_render_view_host.ptr());

        // Make sure the size of the RenderViewHost has been passed to the new
        // RenderView.  Otherwise, the size may not be sent until the
        // RenderViewReady event makes it from the render process to the UI
        // thread of the browser process.  When the RenderView receives its
        // size, is also sets itself to be visible, which would then break the
        // visibility API.
        new_render_view_host->WasResized();
        prerender_contents_->WasHidden();
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
    SessionStorageNamespace* session_storage_namespace) {
  // TODO(ajwong): Remove the temporary map once prerendering is aware of
  // multiple session storage namespaces per tab.
  content::SessionStorageNamespaceMap session_storage_namespace_map;
  session_storage_namespace_map[std::string()] = session_storage_namespace;
  return WebContents::CreateWithSessionStorage(
      WebContents::CreateParams(profile_), session_storage_namespace_map);
}

void PrerenderContents::NotifyPrerenderStart() {
  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnPrerenderStart(this));
}

void PrerenderContents::NotifyPrerenderStopLoading() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnPrerenderStopLoading(this));
}

void PrerenderContents::NotifyPrerenderDomContentLoaded() {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnPrerenderDomContentLoaded(this));
}

void PrerenderContents::NotifyPrerenderStop() {
  DCHECK_NE(FINAL_STATUS_MAX, final_status_);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnPrerenderStop(this));
  observer_list_.Clear();
}

void PrerenderContents::NotifyPrerenderCreatedMatchCompleteReplacement(
    PrerenderContents* replacement) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnPrerenderCreatedMatchCompleteReplacement(this,
                                                               replacement));
}

bool PrerenderContents::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  // The following messages we do want to consume.
  IPC_BEGIN_MESSAGE_MAP(PrerenderContents, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CancelPrerenderForPrinting,
                        OnCancelPrerenderForPrinting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool PrerenderContents::CheckURL(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    DCHECK_NE(MATCH_COMPLETE_REPLACEMENT_PENDING, match_complete_status_);
    Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return false;
  }
  if (match_complete_status_ != MATCH_COMPLETE_REPLACEMENT_PENDING &&
      prerender_manager_->HasRecentlyBeenNavigatedTo(origin(), url)) {
    Destroy(FINAL_STATUS_RECENTLY_VISITED);
    return false;
  }
  return true;
}

bool PrerenderContents::AddAliasURL(const GURL& url) {
  if (!CheckURL(url))
    return false;

  alias_urls_.push_back(url);

  for (content::RenderProcessHost::iterator host_iterator =
           content::RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd();
       host_iterator.Advance()) {
    content::RenderProcessHost* host = host_iterator.GetCurrentValue();
    host->Send(new PrerenderMsg_OnPrerenderAddAlias(url));
  }

  return true;
}

bool PrerenderContents::Matches(
    const GURL& url,
    const SessionStorageNamespace* session_storage_namespace) const {
  if (session_storage_namespace &&
      session_storage_namespace_id_ != session_storage_namespace->id()) {
    return false;
  }
  return std::count_if(alias_urls_.begin(), alias_urls_.end(),
                       std::bind2nd(std::equal_to<GURL>(), url)) != 0;
}

void PrerenderContents::RenderProcessGone(base::TerminationStatus status) {
  Destroy(FINAL_STATUS_RENDERER_CRASHED);
}

void PrerenderContents::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // When a new RenderFrame is created for a prerendering WebContents, tell the
  // new RenderFrame it's being used for prerendering before any navigations
  // occur.  Note that this is always triggered before the first navigation, so
  // there's no need to send the message just after the WebContents is created.
  render_frame_host->Send(new PrerenderMsg_SetIsPrerendering(
      render_frame_host->GetRoutingID(), true));
}

void PrerenderContents::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  has_stopped_loading_ = true;
  NotifyPrerenderStopLoading();
}

void PrerenderContents::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent())
    NotifyPrerenderDomContentLoaded();
}

void PrerenderContents::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (!render_frame_host->GetParent()) {
    if (!CheckURL(validated_url))
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

void PrerenderContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->GetParent())
    has_finished_loading_ = true;
}

void PrerenderContents::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // If the prerender made a second navigation entry, abort the prerender. This
  // avoids having to correctly implement a complex history merging case (this
  // interacts with location.replace) and correctly synchronize with the
  // renderer. The final status may be monitored to see we need to revisit this
  // decision. This does not affect client redirects as those do not push new
  // history entries. (Calls to location.replace, navigations before onload, and
  // <meta http-equiv=refresh> with timeouts under 1 second do not create
  // entries in Blink.)
  if (prerender_contents_->GetController().GetEntryCount() > 1) {
    Destroy(FINAL_STATUS_NEW_NAVIGATION_ENTRY);
    return;
  }

  // Add each redirect as an alias. |params.url| is included in
  // |params.redirects|.
  //
  // TODO(davidben): We do not correctly patch up history for renderer-initated
  // navigations which add history entries. http://crbug.com/305660.
  for (size_t i = 0; i < params.redirects.size(); i++) {
    if (!AddAliasURL(params.redirects[i]))
      return;
  }
}

void PrerenderContents::DidGetRedirectForResourceRequest(
    RenderViewHost* render_view_host,
    const content::ResourceRedirectDetails& details) {
  // DidGetRedirectForResourceRequest can come for any resource on a page.  If
  // it's a redirect on the top-level resource, the name needs to be remembered
  // for future matching, and if it redirects to an https resource, it needs to
  // be canceled. If a subresource is redirected, nothing changes.
  if (details.resource_type != ResourceType::MAIN_FRAME)
    return;
  CheckURL(details.new_url);
}

void PrerenderContents::Destroy(FinalStatus final_status) {
  DCHECK_NE(final_status, FINAL_STATUS_USED);

  if (prerendering_has_been_cancelled_)
    return;

  SetFinalStatus(final_status);

  prerendering_has_been_cancelled_ = true;
  prerender_manager_->AddToHistory(this);
  prerender_manager_->MoveEntryToPendingDelete(this, final_status);

  // Note that if this PrerenderContents was made into a MatchComplete
  // replacement by MoveEntryToPendingDelete, NotifyPrerenderStop will
  // not reach the PrerenderHandle. Rather
  // OnPrerenderCreatedMatchCompleteReplacement will propogate that
  // information to the referer.
  if (!prerender_manager_->IsControlGroup(experiment_id()) &&
      (prerendering_has_started() ||
       match_complete_status() == MATCH_COMPLETE_REPLACEMENT)) {
    NotifyPrerenderStop();
  }
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

WebContents* PrerenderContents::ReleasePrerenderContents() {
  prerender_contents_->SetDelegate(NULL);
  content::WebContentsObserver::Observe(NULL);
  if (alias_session_storage_namespace)
    alias_session_storage_namespace->RemoveTransactionLogProcessId(child_id_);
  return prerender_contents_.release();
}

RenderViewHost* PrerenderContents::GetRenderViewHostMutable() {
  return const_cast<RenderViewHost*>(GetRenderViewHost());
}

const RenderViewHost* PrerenderContents::GetRenderViewHost() const {
  if (!prerender_contents_.get())
    return NULL;
  return prerender_contents_->GetRenderViewHost();
}

void PrerenderContents::DidNavigate(
    const history::HistoryAddPageArgs& add_page_args) {
  add_page_vector_.push_back(add_page_args);
}

void PrerenderContents::CommitHistory(WebContents* tab) {
  HistoryTabHelper* history_tab_helper = HistoryTabHelper::FromWebContents(tab);
  for (size_t i = 0; i < add_page_vector_.size(); ++i)
    history_tab_helper->UpdateHistoryForNavigation(add_page_vector_[i]);
}

base::Value* PrerenderContents::GetAsValue() const {
  if (!prerender_contents_.get())
    return NULL;
  base::DictionaryValue* dict_value = new base::DictionaryValue();
  dict_value->SetString("url", prerender_url_.spec());
  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta duration = current_time - load_start_time_;
  dict_value->SetInteger("duration", duration.InSeconds());
  dict_value->SetBoolean("is_loaded", prerender_contents_ &&
                                      !prerender_contents_->IsLoading());
  return dict_value;
}

bool PrerenderContents::IsCrossSiteNavigationPending() const {
  if (!prerender_contents_)
    return false;
  return (prerender_contents_->GetSiteInstance() !=
          prerender_contents_->GetPendingSiteInstance());
}

void PrerenderContents::PrepareForUse() {
  SetFinalStatus(FINAL_STATUS_USED);

  if (prerender_contents_.get()) {
    prerender_contents_->SendToAllFrames(
        new PrerenderMsg_SetIsPrerendering(MSG_ROUTING_NONE, false));
  }

  NotifyPrerenderStop();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ResumeThrottles, resource_throttles_));
  resource_throttles_.clear();
}

SessionStorageNamespace* PrerenderContents::GetSessionStorageNamespace() const {
  if (!prerender_contents())
    return NULL;
  return prerender_contents()->GetController().
      GetDefaultSessionStorageNamespace();
}

void PrerenderContents::OnCancelPrerenderForPrinting() {
  Destroy(FINAL_STATUS_WINDOW_PRINT);
}

void PrerenderContents::RecordCookieEvent(CookieEvent event,
                                          bool is_main_frame_http_request,
                                          bool is_third_party_cookie,
                                          bool is_for_blocking_resource,
                                          base::Time earliest_create_date) {
  // We don't care about sent cookies that were created after this prerender
  // started.
  // The reason is that for the purpose of the histograms emitted, we only care
  // about cookies that existed before the prerender was started, but not
  // about cookies that were created as part of the prerender. Using the
  // earliest creation timestamp of all cookies provided by the cookie monster
  // is a heuristic that yields the desired result pretty closely.
  // In particular, we pretend no other WebContents make changes to the cookies
  // relevant to the prerender, which may not actually always be the case, but
  // hopefully most of the times.
  if (event == COOKIE_EVENT_SEND && earliest_create_date > start_time_)
    return;

  InternalCookieEvent internal_event = INTERNAL_COOKIE_EVENT_MAX;

  if (is_main_frame_http_request) {
    if (event == COOKIE_EVENT_SEND) {
      internal_event = INTERNAL_COOKIE_EVENT_MAIN_FRAME_SEND;
    } else {
      internal_event = INTERNAL_COOKIE_EVENT_MAIN_FRAME_CHANGE;
    }
  } else {
    if (event == COOKIE_EVENT_SEND) {
      internal_event = INTERNAL_COOKIE_EVENT_OTHER_SEND;
    } else {
      internal_event = INTERNAL_COOKIE_EVENT_OTHER_CHANGE;
    }
  }

  DCHECK_GE(internal_event, 0);
  DCHECK_LT(internal_event, INTERNAL_COOKIE_EVENT_MAX);

  cookie_status_ |= (1 << internal_event);

  DCHECK_GE(cookie_status_, 0);
  DCHECK_LT(cookie_status_, kNumCookieStatuses);

  CookieSendType send_type = COOKIE_SEND_TYPE_NONE;
  if (event == COOKIE_EVENT_SEND) {
    if (!is_third_party_cookie) {
      send_type = COOKIE_SEND_TYPE_FIRST_PARTY;
    } else {
      if (is_for_blocking_resource) {
        send_type = COOKIE_SEND_TYPE_THIRD_PARTY_BLOCKING_RESOURCE;
      } else {
        send_type = COOKIE_SEND_TYPE_THIRD_PARTY;
      }
    }
  }
  DCHECK_GE(send_type, 0);
  DCHECK_LT(send_type, COOKIE_SEND_TYPE_MAX);

  if (cookie_send_type_ < send_type)
    cookie_send_type_ = send_type;
}

 void PrerenderContents::AddResourceThrottle(
     const base::WeakPtr<PrerenderResourceThrottle>& throttle) {
   resource_throttles_.push_back(throttle);
 }

 void PrerenderContents::AddNetworkBytes(int64 bytes) {
   network_bytes_ += bytes;
 }

}  // namespace prerender
