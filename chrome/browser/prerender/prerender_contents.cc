// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_contents.h"

#include <algorithm>
#include <utility>

#include "base/process_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_render_widget_host_view.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/download/download_tab_helper.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_request_details.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mach_broker_mac.h"
#endif

namespace prerender {

namespace {

// Compares URLs ignoring any ref for the purposes of matching URLs when
// prerendering.
struct PrerenderUrlPredicate {
  explicit PrerenderUrlPredicate(const GURL& url)
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
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const GURL& referrer) OVERRIDE {
    return new PrerenderContents(prerender_manager, profile, url, referrer);
  }
};

// TabContentsDelegateImpl -----------------------------------------------------

class PrerenderContents::TabContentsDelegateImpl
    : public TabContentsDelegate {
 public:
  explicit TabContentsDelegateImpl(PrerenderContents* prerender_contents) :
      prerender_contents_(prerender_contents) {
  }
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type) {
    add_page_vector_.push_back(
        scoped_refptr<history::HistoryAddPageArgs>(add_page_args.Clone()));
    return false;
  }
  // Commits the History of Pages to the given TabContents.
  void CommitHistory(TabContents* tc) {
    DCHECK(tc != NULL);
    for (size_t i = 0; i < add_page_vector_.size(); ++i)
      tc->UpdateHistoryForNavigation(add_page_vector_[i].get());
  }

 private:
  typedef std::vector<scoped_refptr<history::HistoryAddPageArgs> >
      AddPageVector;

  // Caches pages to be added to the history.
  AddPageVector add_page_vector_;

  PrerenderContents* prerender_contents_;
};

PrerenderContents::PrerenderContents(PrerenderManager* prerender_manager,
                                     Profile* profile,
                                     const GURL& url,
                                     const GURL& referrer)
    : prerender_manager_(prerender_manager),
      render_view_host_(NULL),
      prerender_url_(url),
      referrer_(referrer),
      profile_(profile),
      page_id_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_contents_observer_registrar_(this)),
      has_stopped_loading_(false),
      final_status_(FINAL_STATUS_MAX),
      prerendering_has_started_(false),
      child_id_(-1),
      route_id_(-1) {
  DCHECK(prerender_manager != NULL);
}

bool PrerenderContents::Init() {
  if (!AddAliasURL(prerender_url_))
    return false;
  return true;
}

// static
PrerenderContents::Factory* PrerenderContents::CreateFactory() {
  return new PrerenderContentsFactoryImpl();
}

void PrerenderContents::StartPrerenderingOld(
    const RenderViewHost* source_render_view_host) {
  DCHECK(profile_ != NULL);
  DCHECK(!prerendering_has_started_);
  DCHECK(source_render_view_host != NULL);
  DCHECK(source_render_view_host->view() != NULL);
  prerendering_has_started_ = true;
  SiteInstance* site_instance = SiteInstance::CreateSiteInstance(profile_);
  render_view_host_ = new RenderViewHost(site_instance, this, MSG_ROUTING_NONE,
                                         NULL);

  // Register as an observer of the RenderViewHost so we get messages.
  render_view_host_observer_.reset(
      new PrerenderRenderViewHostObserver(this, render_view_host_mutable()));

  // Create the RenderView, so it can receive messages.
  render_view_host_->CreateRenderView(string16());

  OnRenderViewHostCreated(render_view_host_);

  // Give the RVH a PrerenderRenderWidgetHostView, both so its size can be set
  // and so that the prerender can be cancelled under certain circumstances.
  PrerenderRenderWidgetHostView* view =
      new PrerenderRenderWidgetHostView(render_view_host_, this);
  view->Init(source_render_view_host->view());

  child_id_ = render_view_host_->process()->id();
  route_id_ = render_view_host_->routing_id();

  // Register this with the PrerenderTracker as a prerendering RenderViewHost.
  // This must be done before the Navigate message to catch all resource
  // requests.
  PrerenderTracker::GetInstance()->OnPrerenderingStarted(child_id_, route_id_,
                                                         prerender_manager_);

  // Close ourselves when the application is shutting down.
  notification_registrar_.Add(this, NotificationType::APP_TERMINATING,
                              NotificationService::AllSources());

  // Register for our parent profile to shutdown, so we can shut ourselves down
  // as well (should only be called for OTR profiles, as we should receive
  // APP_TERMINATING before non-OTR profiles are destroyed).
  // TODO(tburkard): figure out if this is needed.
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));

  // Register to cancel if Authentication is required.
  notification_registrar_.Add(this, NotificationType::AUTH_NEEDED,
                              NotificationService::AllSources());

  notification_registrar_.Add(this, NotificationType::AUTH_CANCELLED,
                 NotificationService::AllSources());

  // Register all responses to see if we should cancel.
  notification_registrar_.Add(this, NotificationType::DOWNLOAD_INITIATED,
                              NotificationService::AllSources());

  // Register for redirect notifications sourced from |this|.
  notification_registrar_.Add(
      this, NotificationType::RESOURCE_RECEIVED_REDIRECT,
      Source<RenderViewHostDelegate>(GetRenderViewHostDelegate()));

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  render_view_host_->Send(
      new ViewMsg_SetIsPrerendering(render_view_host_->routing_id(), true));

  ViewMsg_Navigate_Params params;
  params.page_id = -1;
  params.pending_history_list_offset = -1;
  params.current_history_list_offset = -1;
  params.current_history_list_length = 0;
  params.url = prerender_url_;
  params.transition = PageTransition::LINK;
  params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params.referrer = referrer_;
  params.request_time = base::Time::Now();

  render_view_host_->Navigate(params);
}

void PrerenderContents::StartPrerendering(
    const RenderViewHost* source_render_view_host) {
  if (!UseTabContents()) {
    VLOG(1) << "Starting prerendering with LEGACY code";
    StartPrerenderingOld(source_render_view_host);
    return;
  }
  VLOG(1) << "Starting prerendering with NEW code";
  DCHECK(profile_ != NULL);
  DCHECK(!prerendering_has_started_);
  DCHECK(prerender_contents_.get() == NULL);
  DCHECK(source_render_view_host != NULL);
  DCHECK(source_render_view_host->view() != NULL);

  prerendering_has_started_ = true;
  TabContents* new_contents = new TabContents(profile_, NULL, MSG_ROUTING_NONE,
                                              NULL, NULL);
  prerender_contents_.reset(new TabContentsWrapper(new_contents));
  tab_contents_observer_registrar_.Observe(new_contents);
  prerender_contents_->download_tab_helper()->set_delegate(this);

  TabContents* source_tc =
      source_render_view_host->delegate()->GetAsTabContents();
  if (source_tc) {
    // So that history merging will work, get the max page ID
    // of the old page, and add a safety margin of 10 to it (for things
    // such as redirects).
    int32 max_page_id = source_tc->GetMaxPageID();
    if (max_page_id != -1) {
      prerender_contents_->controller().set_max_restored_page_id(
          max_page_id + 10);
    }

    tab_contents_delegate_.reset(new TabContentsDelegateImpl(this));
    new_contents->set_delegate(tab_contents_delegate_.get());

    // Set the size of the new TC to that of the old TC.
    gfx::Rect tab_bounds;
    source_tc->view()->GetContainerBounds(&tab_bounds);
    prerender_contents_->view()->SizeContents(tab_bounds.size());
  }

  // Register as an observer of the RenderViewHost so we get messages.
  render_view_host_observer_.reset(
      new PrerenderRenderViewHostObserver(this, render_view_host_mutable()));

  child_id_ = render_view_host()->process()->id();
  route_id_ = render_view_host()->routing_id();

  // Register this with the ResourceDispatcherHost as a prerender
  // RenderViewHost. This must be done before the Navigate message to catch all
  // resource requests, but as it is on the same thread as the Navigate message
  // (IO) there is no race condition.
  PrerenderTracker::GetInstance()->OnPrerenderingStarted(child_id_, route_id_,
                                                         prerender_manager_);

  // Close ourselves when the application is shutting down.
  notification_registrar_.Add(this, NotificationType::APP_TERMINATING,
                              NotificationService::AllSources());

  // Register for our parent profile to shutdown, so we can shut ourselves down
  // as well (should only be called for OTR profiles, as we should receive
  // APP_TERMINATING before non-OTR profiles are destroyed).
  // TODO(tburkard): figure out if this is needed.
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));

  // Register to cancel if Authentication is required.
  notification_registrar_.Add(this, NotificationType::AUTH_NEEDED,
                              NotificationService::AllSources());

  notification_registrar_.Add(this, NotificationType::AUTH_CANCELLED,
                              NotificationService::AllSources());

  // Register to inform new RenderViews that we're prerendering.
  notification_registrar_.Add(
      this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
      Source<TabContents>(new_contents));

  // Register for redirect notifications sourced from |this|.
  notification_registrar_.Add(
      this, NotificationType::RESOURCE_RECEIVED_REDIRECT,
      Source<RenderViewHostDelegate>(GetRenderViewHostDelegate()));

  // Register for new windows from any source.
  notification_registrar_.Add(this,
                              NotificationType::CREATING_NEW_WINDOW_CANCELLED,
                              Source<TabContents>(new_contents));

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  new_contents->controller().LoadURL(prerender_url_,
                                     referrer_, PageTransition::LINK);
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
         final_status_ == FINAL_STATUS_CONTROL_GROUP);

  // Don't override final_status_ if it's FINAL_STATUS_CONTROL_GROUP,
  // otherwise data will be collected in the Prerender.FinalStatus histogram.
  if (final_status_ == FINAL_STATUS_CONTROL_GROUP)
    return;

  final_status_ = final_status;
}

FinalStatus PrerenderContents::final_status() const {
  return final_status_;
}

PrerenderContents::~PrerenderContents() {
  DCHECK(final_status_ != FINAL_STATUS_MAX);

  // If we haven't even started prerendering, we were just in the control
  // group, which means we do not want to record the status.
  if (prerendering_has_started())
    RecordFinalStatus(final_status_);

  // Only delete the RenderViewHost if we own it.
  if (render_view_host_)
    render_view_host_->Shutdown();

  if (child_id_ != -1 && route_id_ != -1) {
    PrerenderTracker::GetInstance()->OnPrerenderingFinished(
        child_id_, route_id_);
  }

  // If we still have a TabContents, clean up anything we need to and then
  // destroy it.
  if (prerender_contents_.get())
    delete ReleasePrerenderContents();
}

RenderViewHostDelegate::View* PrerenderContents::GetViewDelegate() {
  return this;
}

const GURL& PrerenderContents::GetURL() const {
  return url_;
}

ViewType::Type PrerenderContents::GetRenderViewType() const {
  return ViewType::BACKGROUND_CONTENTS;
}

int PrerenderContents::GetBrowserWindowID() const {
  return extension_misc::kUnknownWindowId;
}

void PrerenderContents::DidNavigate(
    RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  DCHECK_EQ(render_view_host_, render_view_host);

  // We only care when the outer frame changes.
  if (!PageTransition::IsMainFrame(params.transition))
    return;

  // Store the navigation params.
  navigate_params_.reset(new ViewHostMsg_FrameNavigate_Params());
  *navigate_params_ = params;

  if (!AddAliasURL(params.url))
    return;

  url_ = params.url;
}

void PrerenderContents::UpdateTitle(RenderViewHost* render_view_host,
                                    int32 page_id,
                                    const std::wstring& title) {
  DCHECK_EQ(render_view_host_, render_view_host);
  if (title.empty())
    return;

  title_ = WideToUTF16Hack(title);
  page_id_ = page_id;
}

bool PrerenderContents::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

void PrerenderContents::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PROFILE_DESTROYED:
      Destroy(FINAL_STATUS_PROFILE_DESTROYED);
      return;

    case NotificationType::APP_TERMINATING:
      Destroy(FINAL_STATUS_APP_TERMINATING);
      return;

    case NotificationType::AUTH_NEEDED:
    case NotificationType::AUTH_CANCELLED: {
      // Only respond to HTTP authentication notifications which
      // are required for this prerendered page.
      LoginNotificationDetails* details_ptr =
          Details<LoginNotificationDetails>(details).ptr();
      LoginHandler* handler = details_ptr->handler();
      DCHECK(handler != NULL);
      RenderViewHostDelegate* delegate = handler->GetRenderViewHostDelegate();
      if (delegate == GetRenderViewHostDelegate()) {
        Destroy(FINAL_STATUS_AUTH_NEEDED);
        return;
      }
      break;
    }

    // TODO(mmenke):  Once we get rid of the old RenderViewHost code, remove
    //                this.
    case NotificationType::DOWNLOAD_INITIATED: {
      // If the download is started from a RenderViewHost that we are
      // delegating, kill the prerender. This cancels any pending requests
      // though the download never actually started thanks to the
      // DownloadRequestLimiter.
      DCHECK(NotificationService::NoDetails() == details);
      RenderViewHost* render_view_host = Source<RenderViewHost>(source).ptr();
      CHECK(render_view_host != NULL);
      if (render_view_host->delegate() == GetRenderViewHostDelegate()) {
        Destroy(FINAL_STATUS_DOWNLOAD);
        return;
      }
      break;
    }

    case NotificationType::RESOURCE_RECEIVED_REDIRECT: {
      // RESOURCE_RECEIVED_REDIRECT can come for any resource on a page.
      // If it's a redirect on the top-level resource, the name needs
      // to be remembered for future matching, and if it redirects to
      // an https resource, it needs to be canceled. If a subresource
      // is redirected, nothing changes.
      DCHECK(Source<RenderViewHostDelegate>(source).ptr() ==
             GetRenderViewHostDelegate());
      ResourceRedirectDetails* resource_redirect_details =
          Details<ResourceRedirectDetails>(details).ptr();
      CHECK(resource_redirect_details);
      if (resource_redirect_details->resource_type() ==
          ResourceType::MAIN_FRAME) {
        if (!AddAliasURL(resource_redirect_details->new_url()))
          return;
      }
      break;
    }

    case NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      if (prerender_contents_.get()) {
        DCHECK_EQ(Source<TabContents>(source).ptr(),
                  prerender_contents_->tab_contents());

        Details<RenderViewHost> new_render_view_host(details);
        OnRenderViewHostCreated(new_render_view_host.ptr());

        // When a new RenderView is created for a prerendering TabContents,
        // tell the new RenderView it's being used for prerendering before any
        // navigations occur.  Note that this is always triggered before the
        // first navigation, so there's no need to send the message just after
        // the TabContents is created.
        new_render_view_host->Send(
            new ViewMsg_SetIsPrerendering(new_render_view_host->routing_id(),
                                          true));

        // Set the new TabContents and its RenderViewHost as hidden, to reduce
        // resource usage.  This can only be done after the first call to
        // LoadURL, so there's an actual RenderViewHost with a
        // RenderWidgetHostView to hide.
        //
        // Done here to prevent a race with loading the page.
        prerender_contents_->tab_contents()->HideContents();
      }
      break;
    }

    case NotificationType::CREATING_NEW_WINDOW_CANCELLED: {
      if (prerender_contents_.get()) {
        CHECK(Source<TabContents>(source).ptr() ==
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

void PrerenderContents::OnMessageBoxClosed(IPC::Message* reply_msg,
                                           bool success,
                                           const std::wstring& prompt) {
  render_view_host_->JavaScriptMessageBoxClosed(reply_msg, success, prompt);
}

gfx::NativeWindow PrerenderContents::GetMessageBoxRootWindow() {
  NOTIMPLEMENTED();
  return NULL;
}

TabContents* PrerenderContents::AsTabContents() {
  return NULL;
}

ExtensionHost* PrerenderContents::AsExtensionHost() {
  return NULL;
}

void PrerenderContents::UpdateInspectorSetting(const std::string& key,
                                               const std::string& value) {
  RenderViewHostDelegateHelper::UpdateInspectorSetting(profile_, key, value);
}

void PrerenderContents::ClearInspectorSettings() {
  RenderViewHostDelegateHelper::ClearInspectorSettings(profile_);
}

void PrerenderContents::Close(RenderViewHost* render_view_host) {
  Destroy(FINAL_STATUS_CLOSED);
}

RendererPreferences PrerenderContents::GetRendererPrefs(
    Profile* profile) const {
  RendererPreferences preferences;
  renderer_preferences_util::UpdateFromSystemSettings(&preferences, profile);
  return preferences;
}

WebPreferences PrerenderContents::GetWebkitPrefs() {
  return RenderViewHostDelegateHelper::GetWebkitPrefs(profile_,
                                                      false);  // is_web_ui
}

void PrerenderContents::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  // TODO(dominich): Remove when we switch to TabContents.
  // Since we don't want to permit child windows that would have a
  // window.opener property, terminate prerendering.
  Destroy(FINAL_STATUS_CREATE_NEW_WINDOW);
}

void PrerenderContents::CreateNewWidget(int route_id,
                                        WebKit::WebPopupType popup_type) {
  NOTREACHED();
}

void PrerenderContents::CreateNewFullscreenWidget(int route_id) {
  NOTREACHED();
}

void PrerenderContents::ShowCreatedWindow(int route_id,
                                          WindowOpenDisposition disposition,
                                          const gfx::Rect& initial_pos,
                                          bool user_gesture) {
  // TODO(tburkard): need to figure out what the correct behavior here is
  NOTIMPLEMENTED();
}

void PrerenderContents::ShowCreatedWidget(int route_id,
                                          const gfx::Rect& initial_pos) {
  NOTIMPLEMENTED();
}

void PrerenderContents::ShowCreatedFullscreenWidget(int route_id) {
  NOTIMPLEMENTED();
}

void PrerenderContents::OnRenderViewHostCreated(
    RenderViewHost* new_render_view_host) {
}

void PrerenderContents::OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                                          bool is_main_frame,
                                                          const GURL& url) {
  if (is_main_frame) {
    if (!AddAliasURL(url))
      return;

    // Usually, this event fires if the user clicks or enters a new URL.
    // Neither of these can happen in the case of an invisible prerender.
    // So the cause is: Some JavaScript caused a new URL to be loaded.  In that
    // case, the spinner would start again in the browser, so we must reset
    // has_stopped_loading_ so that the spinner won't be stopped.
    has_stopped_loading_ = false;
  }
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
  if (!url.SchemeIs("http")) {
    if (url.SchemeIs("https"))
      Destroy(FINAL_STATUS_HTTPS);
    else
      Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return false;
  }
  if (prerender_manager_->HasRecentlyBeenNavigatedTo(url)) {
    Destroy(FINAL_STATUS_RECENTLY_VISITED);
    return false;
  }
  alias_urls_.push_back(url);
  return true;
}

bool PrerenderContents::MatchesURL(const GURL& url) const {
  return std::find_if(alias_urls_.begin(),
                      alias_urls_.end(),
                      PrerenderUrlPredicate(url)) != alias_urls_.end();
}

void PrerenderContents::OnJSOutOfMemory() {
  Destroy(FINAL_STATUS_JS_OUT_OF_MEMORY);
}

void PrerenderContents::OnRunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    bool* did_suppress_message,
    std::wstring* prompt_field) {
  // Always suppress JavaScript messages if they're triggered by a page being
  // prerendered.
  *did_suppress_message = true;
  // We still want to show the user the message when they navigate to this
  // page, so cancel this prerender.
  Destroy(FINAL_STATUS_JAVASCRIPT_ALERT);
}

void PrerenderContents::OnRenderViewGone(int status, int exit_code) {
  Destroy(FINAL_STATUS_RENDERER_CRASHED);
}

void PrerenderContents::DidStopLoading() {
  has_stopped_loading_ = true;
}

void PrerenderContents::Destroy(FinalStatus final_status) {
  if (prerender_manager_->IsPendingDelete(this))
    return;

  if (child_id_ != -1 && route_id_ != -1) {
    // Cancel the prerender in the PrerenderTracker.  This is needed
    // because destroy may be called directly from the UI thread without calling
    // TryCancel().  This is difficult to completely avoid, since prerendering
    // can be cancelled before a RenderView is created.
    bool is_cancelled =
        PrerenderTracker::GetInstance()->TryCancel(child_id_, route_id_,
                                                   final_status);
    CHECK(is_cancelled);

    // A different final status may have been set already from another thread.
    // If so, use it instead.
    if (!PrerenderTracker::GetInstance()->GetFinalStatus(child_id_, route_id_,
                                                         &final_status)) {
      NOTREACHED();
    }
  }

  prerender_manager_->MoveEntryToPendingDelete(this);
  set_final_status(final_status);
  // We may destroy the PrerenderContents before we have initialized the
  // RenderViewHost. Otherwise set the Observer's PrerenderContents to NULL to
  // avoid any more messages being sent.
  if (render_view_host_observer_.get())
    render_view_host_observer_->set_prerender_contents(NULL);
}

void PrerenderContents::RendererUnresponsive(RenderViewHost* render_view_host,
                                             bool is_during_unload) {
  DCHECK_EQ(render_view_host_, render_view_host);
  Destroy(FINAL_STATUS_RENDERER_UNRESPONSIVE);
}

bool PrerenderContents::CanDownload(int request_id) {
  Destroy(FINAL_STATUS_DOWNLOAD);
  // Cancel the download.
  return false;
}

void PrerenderContents::OnStartDownload(DownloadItem* download,
                                        TabContentsWrapper* tab) {
  // Prerendered pages should never be able to download files.
  NOTREACHED();
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
  if (metrics->GetMemoryBytes(&private_bytes, &shared_bytes)) {
    if (private_bytes >
        prerender_manager_->max_prerender_memory_mb() * 1024 * 1024) {
      Destroy(FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);
    }
  }
}

TabContentsWrapper* PrerenderContents::ReleasePrerenderContents() {
  render_view_host_observer_.reset();
  prerender_contents_->download_tab_helper()->set_delegate(NULL);
  return prerender_contents_.release();
}

RenderViewHostDelegate* PrerenderContents::GetRenderViewHostDelegate() {
  if (UseTabContents()) {
    if (!prerender_contents_.get())
      return NULL;
    return prerender_contents_->tab_contents();
  } else {
    return this;
  }
}

RenderViewHost* PrerenderContents::render_view_host_mutable() {
  return const_cast<RenderViewHost*>(render_view_host());
}

const RenderViewHost* PrerenderContents::render_view_host() const {
  // TODO(mmenke): Replace with simple accessor once TabContents is always
  //               used.
  if (UseTabContents()) {
    if (!prerender_contents_.get())
      return NULL;
    return prerender_contents_->render_view_host();
  }
  return render_view_host_;
}

void PrerenderContents::CommitHistory(TabContents* tc) {
  if (tab_contents_delegate_.get())
    tab_contents_delegate_->CommitHistory(tc);
}

}  // namespace prerender
