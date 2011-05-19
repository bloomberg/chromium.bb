// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents.h"

#include <cmath>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/load_notification_details.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/renderer_host/resource_request_details.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/provisional_load_details.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/user_metrics.h"
#include "content/browser/webui/web_ui_factory.h"
#include "content/common/bindings_policy.h"
#include "content/common/content_client.h"
#include "content/common/content_restriction.h"
#include "content/common/navigation_types.h"
#include "content/common/notification_service.h"
#include "content/common/url_constants.h"
#include "content/common/view_messages.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_MACOSX)
#include "ui/gfx/surface/io_surface_support_mac.h"
#endif  // defined(OS_MACOSX)

// Cross-Site Navigations
//
// If a TabContents is told to navigate to a different web site (as determined
// by SiteInstance), it will replace its current RenderViewHost with a new
// RenderViewHost dedicated to the new SiteInstance.  This works as follows:
//
// - Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_view_host_ and moves into the PENDING
//   RendererState.
// - The pending RVH is "suspended," so that no navigation messages are sent to
//   its renderer until the onbeforeunload JavaScript handler has a chance to
//   run in the current RVH.
// - The pending RVH tells CrossSiteRequestManager (a thread-safe singleton)
//   that it has a pending cross-site request.  ResourceDispatcherHost will
//   check for this when the response arrives.
// - The current RVH runs its onbeforeunload handler.  If it returns false, we
//   cancel all the pending logic and go back to NORMAL.  Otherwise we allow
//   the pending RVH to send the navigation request to its renderer.
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread.  It
//   checks CrossSiteRequestManager to see that the RVH responsible has a
//   pending cross-site request, and then installs a CrossSiteEventHandler.
// - When RDH receives a response, the BufferedEventHandler determines whether
//   it is a download.  If so, it sends a message to the new renderer causing
//   it to cancel the request, and the download proceeds in the download
//   thread.  For now, we stay in a PENDING state (with a pending RVH) until
//   the next DidNavigate event for this TabContents.  This isn't ideal, but it
//   doesn't affect any functionality.
// - After RDH receives a response and determines that it is safe and not a
//   download, it pauses the response to first run the old page's onunload
//   handler.  It does this by asynchronously calling the OnCrossSiteResponse
//   method of TabContents on the UI thread, which sends a SwapOut message
//   to the current RVH.
// - Once the onunload handler is finished, a SwapOut_ACK message is sent to
//   the ResourceDispatcherHost, who unpauses the response.  Data is then sent
//   to the pending RVH.
// - The pending renderer sends a FrameNavigate message that invokes the
//   DidNavigate method.  This replaces the current RVH with the
//   pending RVH and goes back to the NORMAL RendererState.
// - The previous renderer is kept swapped out in RenderViewHostManager in case
//   the user goes back.  The process only stays live if another tab is using
//   it, but if so, the existing frame relationships will be maintained.

namespace {

// Amount of time we wait between when a key event is received and the renderer
// is queried for its state and pushed to the NavigationEntry.
const int kQueryStateDelay = 5000;

const int kSyncWaitDelay = 40;

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kAlternateErrorPagesEnabled,
  prefs::kDefaultZoomLevel,
  prefs::kWebKitJavaEnabled,
  prefs::kWebKitJavascriptEnabled,
  prefs::kWebKitLoadsImagesAutomatically,
  prefs::kWebKitPluginsEnabled,
  prefs::kWebKitUsesUniversalDetector,
  prefs::kWebKitStandardFontFamily,
  prefs::kWebKitSerifFontFamily,
  prefs::kWebKitSansSerifFontFamily,
  prefs::kWebKitFixedFontFamily,
  prefs::kWebKitDefaultFontSize,
  prefs::kWebKitDefaultFixedFontSize,
  prefs::kWebKitMinimumFontSize,
  prefs::kWebKitMinimumLogicalFontSize,
  prefs::kWebkitTabsToLinks,
  prefs::kWebKitAllowRunningInsecureContent,
  prefs::kWebKitAllowDisplayingInsecureContent,
  prefs::kDefaultCharset,
  prefs::kEnableReferrers
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

#if defined(OS_WIN)

BOOL CALLBACK InvalidateWindow(HWND hwnd, LPARAM lparam) {
  // Note: erase is required to properly paint some widgets borders. This can
  // be seen with textfields.
  InvalidateRect(hwnd, NULL, TRUE);
  return TRUE;
}
#endif

ViewMsg_Navigate_Type::Value GetNavigationType(
    Profile* profile, const NavigationEntry& entry,
    NavigationController::ReloadType reload_type) {
  switch (reload_type) {
    case NavigationController::RELOAD:
      return ViewMsg_Navigate_Type::RELOAD;
    case NavigationController::RELOAD_IGNORING_CACHE:
      return ViewMsg_Navigate_Type::RELOAD_IGNORING_CACHE;
    case NavigationController::NO_RELOAD:
      break;  // Fall through to rest of function.
  }

  if (entry.restore_type() == NavigationEntry::RESTORE_LAST_SESSION &&
      profile->DidLastSessionExitCleanly())
    return ViewMsg_Navigate_Type::RESTORE;

  return ViewMsg_Navigate_Type::NORMAL;
}

void MakeNavigateParams(const NavigationEntry& entry,
                        const NavigationController& controller,
                        NavigationController::ReloadType reload_type,
                        ViewMsg_Navigate_Params* params) {
  params->page_id = entry.page_id();
  params->pending_history_list_offset = controller.GetIndexOfEntry(&entry);
  params->current_history_list_offset = controller.last_committed_entry_index();
  params->current_history_list_length = controller.entry_count();
  params->url = entry.url();
  params->referrer = entry.referrer();
  params->transition = entry.transition_type();
  params->state = entry.content_state();
  params->navigation_type =
      GetNavigationType(controller.profile(), entry, reload_type);
  params->request_time = base::Time::Now();
}

}  // namespace


// TabContents ----------------------------------------------------------------

TabContents::TabContents(Profile* profile,
                         SiteInstance* site_instance,
                         int routing_id,
                         const TabContents* base_tab_contents,
                         SessionStorageNamespace* session_storage_namespace)
    : delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(controller_(
          this, profile, session_storage_namespace)),
      ALLOW_THIS_IN_INITIALIZER_LIST(view_(
          TabContentsView::Create(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(render_manager_(this, this)),
      bookmark_drag_(NULL),
      is_loading_(false),
      crashed_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      crashed_error_code_(0),
      waiting_for_response_(false),
      max_page_id_(-1),
      load_state_(net::LOAD_STATE_IDLE),
      upload_size_(0),
      upload_position_(0),
      received_page_title_(false),
      displayed_insecure_content_(false),
      capturing_contents_(false),
      is_being_destroyed_(false),
      notify_disconnection_(false),
#if defined(OS_WIN)
      message_box_active_(CreateEvent(NULL, TRUE, FALSE, NULL)),
#endif
      suppress_javascript_messages_(false),
      is_showing_before_unload_dialog_(false),
      opener_web_ui_type_(WebUI::kNoWebUI),
      closed_by_user_gesture_(false),
      minimum_zoom_percent_(
          static_cast<int>(WebKit::WebView::minTextSizeMultiplier * 100)),
      maximum_zoom_percent_(
          static_cast<int>(WebKit::WebView::maxTextSizeMultiplier * 100)),
      temporary_zoom_settings_(false),
      content_restrictions_(0) {
  renderer_preferences_util::UpdateFromSystemSettings(
      &renderer_preferences_, profile);

  render_manager_.Init(profile, site_instance, routing_id);

  // We have the initial size of the view be based on the size of the passed in
  // tab contents (normally a tab from the same window).
  view_->CreateView(base_tab_contents ?
      base_tab_contents->view()->GetContainerSize() : gfx::Size());

  // Register for notifications about all interested prefs change.
  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      pref_change_registrar_.Add(kPrefsToObserve[i], this);
  }

  registrar_.Add(this, NotificationType::RENDER_WIDGET_HOST_DESTROYED,
                 NotificationService::AllSources());
#if defined(OS_LINUX)
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
#endif

  registrar_.Add(this, NotificationType::USER_STYLE_SHEET_UPDATED,
                 NotificationService::AllSources());

  // Listen for Google URL changes.
  registrar_.Add(this, NotificationType::GOOGLE_URL_UPDATED,
                 NotificationService::AllSources());

  // Can only add observers after render_manager_.Init() is called, since that's
  // what sets up the render_view_host which TabContentObserver's constructor
  // uses to get the routing_id.
  AddObservers();
}

TabContents::~TabContents() {
  is_being_destroyed_ = true;

  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();
  pref_change_registrar_.RemoveAll();

  NotifyDisconnected();
  browser::HideHungRendererDialog(this);

  // First cleanly close all child windows.
  // TODO(mpcomplete): handle case if MaybeCloseChildWindows() already asked
  // some of these to close.  CloseWindows is async, so it might get called
  // twice before it runs.
  CloseConstrainedWindows();

  // Notify any observer that have a reference on this tab contents.
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(this),
      NotificationService::NoDetails());

  // TODO(brettw) this should be moved to the view.
#if defined(OS_WIN)
  // If we still have a window handle, destroy it. GetNativeView can return
  // NULL if this contents was part of a window that closed.
  if (GetNativeView()) {
    RenderViewHost* host = render_view_host();
    if (host && host->view())
      host->view()->WillWmDestroy();
  }
#endif

  // OnCloseStarted isn't called in unit tests.
  if (!tab_close_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Tab.Close",
        base::TimeTicks::Now() - tab_close_start_time_);
  }

  FOR_EACH_OBSERVER(TabContentsObserver, observers_, TabContentsDestroyed());

  net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
}

void TabContents::AddObservers() {
  net::NetworkChangeNotifier::AddOnlineStateObserver(this);
}

bool TabContents::OnMessageReceived(const IPC::Message& message) {
  if (web_ui() && web_ui()->OnMessageReceived(message))
    return true;

  ObserverListBase<TabContentsObserver>::Iterator it(observers_);
  TabContentsObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    if (observer->OnMessageReceived(message))
      return true;

  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(TabContents, message, message_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidRedirectProvisionalLoad,
                        OnDidRedirectProvisionalLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFailProvisionalLoadWithError,
                        OnDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLoadResourceFromMemoryCache,
                        OnDidLoadResourceFromMemoryCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDisplayInsecureContent,
                        OnDidDisplayInsecureContent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidRunInsecureContent,
                        OnDidRunInsecureContent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentLoadedInFrame,
                        OnDocumentLoadedInFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateContentRestrictions,
                        OnUpdateContentRestrictions)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GoToEntryAtOffset, OnGoToEntryAtOffset)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateZoomLimits, OnUpdateZoomLimits)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnFocusedNodeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!message_is_ok) {
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RVD"));
    GetRenderProcessHost()->ReceivedBadMessage();
  }

  return handled;
}

// Returns true if contains content rendered by an extension.
bool TabContents::HostsExtension() const {
  return GetURL().SchemeIs(chrome::kExtensionScheme);
}

RenderProcessHost* TabContents::GetRenderProcessHost() const {
  return render_manager_.current_host()->process();
}

const GURL& TabContents::GetURL() const {
  // We may not have a navigation entry yet
  NavigationEntry* entry = controller_.GetActiveEntry();
  return entry ? entry->virtual_url() : GURL::EmptyGURL();
}

const string16& TabContents::GetTitle() const {
  // Transient entries take precedence. They are used for interstitial pages
  // that are shown on top of existing pages.
  NavigationEntry* entry = controller_.GetTransientEntry();
  if (entry) {
    return entry->GetTitleForDisplay(profile()->GetPrefs()->
        GetString(prefs::kAcceptLanguages));
  }
  WebUI* our_web_ui = render_manager_.pending_web_ui() ?
      render_manager_.pending_web_ui() : render_manager_.web_ui();
  if (our_web_ui) {
    // Don't override the title in view source mode.
    entry = controller_.GetActiveEntry();
    if (!(entry && entry->IsViewSourceMode())) {
      // Give the Web UI the chance to override our title.
      const string16& title = our_web_ui->overridden_title();
      if (!title.empty())
        return title;
    }
  }

  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  entry = controller_.GetLastCommittedEntry();
  if (entry) {
    return entry->GetTitleForDisplay(profile()->GetPrefs()->
        GetString(prefs::kAcceptLanguages));
  }
  return EmptyString16();
}

int32 TabContents::GetMaxPageID() {
  if (GetSiteInstance())
    return GetSiteInstance()->max_page_id();
  else
    return max_page_id_;
}

void TabContents::UpdateMaxPageID(int32 page_id) {
  // Ensure both the SiteInstance and RenderProcessHost update their max page
  // IDs in sync. Only TabContents will also have site instances, except during
  // testing.
  if (GetSiteInstance())
    GetSiteInstance()->UpdateMaxPageID(page_id);
  GetRenderProcessHost()->UpdateMaxPageID(page_id);
}

SiteInstance* TabContents::GetSiteInstance() const {
  return render_manager_.current_host()->site_instance();
}

bool TabContents::ShouldDisplayURL() {
  // Don't hide the url in view source mode and with interstitials.
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (entry && (entry->IsViewSourceMode() ||
                entry->page_type() == INTERSTITIAL_PAGE)) {
    return true;
  }

  // We always display the URL for non-WebUI URLs to prevent spoofing.
  if (entry && !content::WebUIFactory::Get()->HasWebUIScheme(entry->url()))
    return true;

  WebUI* web_ui = GetWebUIForCurrentState();
  if (web_ui)
    return !web_ui->should_hide_url();
  return true;
}

void TabContents::AddObserver(TabContentsObserver* observer) {
  observers_.AddObserver(observer);
}

void TabContents::RemoveObserver(TabContentsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabContents::SetIsCrashed(base::TerminationStatus status, int error_code) {
  if (status == crashed_status_)
    return;

  crashed_status_ = status;
  crashed_error_code_ = error_code;
  NotifyNavigationStateChanged(INVALIDATE_TAB);
}

void TabContents::NotifyNavigationStateChanged(unsigned changed_flags) {
  if (delegate_)
    delegate_->NavigationStateChanged(this, changed_flags);
}

void TabContents::DidBecomeSelected() {
  controller_.SetActive(true);
  RenderWidgetHostView* rwhv = GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->DidBecomeSelected();
#if defined(OS_MACOSX)
    rwhv->SetActive(true);
#endif
  }

  WebCacheManager::GetInstance()->ObserveActivity(GetRenderProcessHost()->id());
  last_selected_time_ = base::TimeTicks::Now();
}

void TabContents::WasHidden() {
  if (!capturing_contents()) {
    // |render_view_host()| can be NULL if the user middle clicks a link to open
    // a tab in then background, then closes the tab before selecting it.  This
    // is because closing the tab calls TabContents::Destroy(), which removes
    // the |render_view_host()|; then when we actually destroy the window,
    // OnWindowPosChanged() notices and calls HideContents() (which calls us).
    RenderWidgetHostView* rwhv = GetRenderWidgetHostView();
    if (rwhv)
      rwhv->WasHidden();
  }

  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_HIDDEN,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::Activate() {
  if (delegate_)
    delegate_->ActivateContents(this);
}

void TabContents::Deactivate() {
  if (delegate_)
    delegate_->DeactivateContents(this);
}

void TabContents::ShowContents() {
  RenderWidgetHostView* rwhv = GetRenderWidgetHostView();
  if (rwhv)
    rwhv->DidBecomeSelected();
}

void TabContents::HideContents() {
  // TODO(pkasting): http://b/1239839  Right now we purposefully don't call
  // our superclass HideContents(), because some callers want to be very picky
  // about the order in which these get called.  In addition to making the code
  // here practically impossible to understand, this also means we end up
  // calling TabContents::WasHidden() twice if callers call both versions of
  // HideContents() on a TabContents.
  WasHidden();
}

bool TabContents::NeedToFireBeforeUnload() {
  // TODO(creis): Should we fire even for interstitial pages?
  return notify_disconnection() &&
      !showing_interstitial_page() &&
      !render_view_host()->SuddenTerminationAllowed();
}

void TabContents::OpenURL(const GURL& url, const GURL& referrer,
                          WindowOpenDisposition disposition,
                          PageTransition::Type transition) {
  if (delegate_) {
    delegate_->OpenURLFromTab(this, url, referrer, disposition, transition);
    // Notify observers.
    FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                      DidOpenURL(url, referrer, disposition, transition));
  }
}

bool TabContents::NavigateToPendingEntry(
    NavigationController::ReloadType reload_type) {
  return NavigateToEntry(*controller_.pending_entry(), reload_type);
}

bool TabContents::NavigateToEntry(
    const NavigationEntry& entry,
    NavigationController::ReloadType reload_type) {
  RenderViewHost* dest_render_view_host = render_manager_.Navigate(entry);
  if (!dest_render_view_host)
    return false;  // Unable to create the desired render view host.

  if (delegate_ && delegate_->ShouldEnablePreferredSizeNotifications()) {
    dest_render_view_host->Send(new ViewMsg_EnablePreferredSizeChangedMode(
        dest_render_view_host->routing_id(),
        kPreferredSizeWidth | kPreferredSizeHeightThisIsSlow));
  }

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  int enabled_bindings = dest_render_view_host->enabled_bindings();
  bool is_allowed_in_web_ui_renderer = content::GetContentClient()->
      browser()->GetWebUIFactory()->IsURLAcceptableForWebUI(profile(),
                                                            entry.url());
  CHECK(!BindingsPolicy::is_web_ui_enabled(enabled_bindings) ||
        is_allowed_in_web_ui_renderer);

  // Tell DevTools agent that it is attached prior to the navigation.
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (devtools_manager) {  // NULL in unit tests.
    devtools_manager->OnNavigatingToPendingEntry(render_view_host(),
                                                 dest_render_view_host,
                                                 entry.url());
  }

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderViewHost.
  ViewMsg_Navigate_Params navigate_params;
  MakeNavigateParams(entry, controller_, reload_type, &navigate_params);
  if (delegate_) {
    navigate_params.extra_headers =
        delegate_->GetNavigationHeaders(navigate_params.url);
  }
  dest_render_view_host->Navigate(navigate_params);

  if (entry.page_id() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.url().SchemeIs(chrome::kJavaScriptScheme))
      return false;
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver,
                    observers_,
                    NavigateToPendingEntry(entry.url(), reload_type));

  return true;
}

void TabContents::Stop() {
  render_manager_.Stop();
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, StopNavigation());
}

TabContents* TabContents::Clone() {
  // We create a new SiteInstance so that the new tab won't share processes
  // with the old one. This can be changed in the future if we need it to share
  // processes for some reason.
  TabContents* tc = new TabContents(profile(),
                                    SiteInstance::CreateSiteInstance(profile()),
                                    MSG_ROUTING_NONE, this, NULL);
  tc->controller().CopyStateFrom(controller_);
  return tc;
}

void TabContents::ShowPageInfo(const GURL& url,
                               const NavigationEntry::SSLStatus& ssl,
                               bool show_history) {
  if (!delegate_)
    return;

  delegate_->ShowPageInfo(profile(), url, ssl, show_history);
}

ConstrainedWindow* TabContents::CreateConstrainedDialog(
      ConstrainedWindowDelegate* delegate) {
  ConstrainedWindow* window =
      ConstrainedWindow::CreateConstrainedDialog(this, delegate);
  AddConstrainedDialog(window);
  return window;
}

void TabContents::AddConstrainedDialog(ConstrainedWindow* window) {
  child_windows_.push_back(window);

  if (child_windows_.size() == 1) {
    window->ShowConstrainedWindow();
    BlockTabContent(true);
  }
}

void TabContents::BlockTabContent(bool blocked) {
  RenderWidgetHostView* rwhv = GetRenderWidgetHostView();
  // 70% opaque grey.
  SkColor greyish = SkColorSetARGB(178, 0, 0, 0);
  if (rwhv)
    rwhv->SetVisuallyDeemphasized(blocked ? &greyish : NULL, false);
  // RenderViewHost may be NULL during shutdown.
  if (render_view_host())
    render_view_host()->set_ignore_input_events(blocked);
  if (delegate_)
    delegate_->SetTabContentBlocked(this, blocked);
}

void TabContents::AddNewContents(TabContents* new_contents,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  if (!delegate_)
    return;

  delegate_->AddNewContents(this, new_contents, disposition, initial_pos,
                            user_gesture);
}

gfx::NativeView TabContents::GetContentNativeView() const {
  return view_->GetContentNativeView();
}

gfx::NativeView TabContents::GetNativeView() const {
  return view_->GetNativeView();
}

void TabContents::GetContainerBounds(gfx::Rect *out) const {
  view_->GetContainerBounds(out);
}

void TabContents::Focus() {
  view_->Focus();
}

void TabContents::FocusThroughTabTraversal(bool reverse) {
  if (showing_interstitial_page()) {
    render_manager_.interstitial_page()->FocusThroughTabTraversal(reverse);
    return;
  }
  render_view_host()->SetInitialFocus(reverse);
}

bool TabContents::FocusLocationBarByDefault() {
  WebUI* web_ui = GetWebUIForCurrentState();
  if (web_ui)
    return web_ui->focus_location_bar_by_default();
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (entry && entry->url() == GURL(chrome::kAboutBlankURL))
    return true;
  return false;
}

void TabContents::SetFocusToLocationBar(bool select_all) {
  if (delegate())
    delegate()->SetFocusToLocationBar(select_all);
}

bool TabContents::ShouldShowBookmarkBar() {
  if (showing_interstitial_page())
    return false;

  // Do not show bookmarks bar if bookmarks aren't enabled.
  if (!browser_defaults::bookmarks_enabled)
    return false;

  if (!profile()->GetPrefs()->GetBoolean(prefs::kEnableBookmarkBar))
    return false;

  // See GetWebUIForCurrentState() comment for more info. This case is very
  // similar, but for non-first loads, we want to use the committed entry. This
  // is so the bookmarks bar disappears at the same time the page does.
  if (controller_.GetLastCommittedEntry()) {
    // Not the first load, always use the committed Web UI.
    return (render_manager_.web_ui() == NULL) ?
        false : render_manager_.web_ui()->force_bookmark_bar_visible();
  }

  // When it's the first load, we know either the pending one or the committed
  // one will have the Web UI in it (see GetWebUIForCurrentState), and only one
  // of them will be valid, so we can just check both.
  if (render_manager_.pending_web_ui())
    return render_manager_.pending_web_ui()->force_bookmark_bar_visible();
  return (render_manager_.web_ui() == NULL) ?
      false : render_manager_.web_ui()->force_bookmark_bar_visible();
}

void TabContents::WillClose(ConstrainedWindow* window) {
  ConstrainedWindowList::iterator i(
      std::find(child_windows_.begin(), child_windows_.end(), window));
  bool removed_topmost_window = i == child_windows_.begin();
  if (i != child_windows_.end())
    child_windows_.erase(i);
  if (child_windows_.empty()) {
    BlockTabContent(false);
  } else {
    if (removed_topmost_window)
      child_windows_[0]->ShowConstrainedWindow();
    BlockTabContent(true);
  }
}

void TabContents::EmailPageLocation() {
  std::string title = EscapeQueryParamValue(UTF16ToUTF8(GetTitle()), false);
  std::string page_url = EscapeQueryParamValue(GetURL().spec(), false);
  std::string mailto = std::string("mailto:?subject=Fwd:%20") +
      title + "&body=%0A%0A" + page_url;
  platform_util::OpenExternal(GURL(mailto));
}

bool TabContents::IsActiveEntry(int32 page_id) {
  NavigationEntry* active_entry = controller_.GetActiveEntry();
  return (active_entry != NULL &&
          active_entry->site_instance() == GetSiteInstance() &&
          active_entry->page_id() == page_id);
}

void TabContents::SetOverrideEncoding(const std::string& encoding) {
  set_encoding(encoding);
  render_view_host()->Send(new ViewMsg_SetPageEncoding(
      render_view_host()->routing_id(), encoding));
}

void TabContents::ResetOverrideEncoding() {
  reset_encoding();
  render_view_host()->Send(new ViewMsg_ResetPageEncodingToDefault(
      render_view_host()->routing_id()));
}

void TabContents::OnCloseStarted() {
  if (tab_close_start_time_.is_null())
    tab_close_start_time_ = base::TimeTicks::Now();
}

bool TabContents::ShouldAcceptDragAndDrop() const {
#if defined(OS_CHROMEOS)
  // ChromeOS panels (pop-ups) do not take drag-n-drop.
  // See http://crosbug.com/2413
  if (delegate() && delegate()->IsPopupOrPanel(this))
    return false;
  return true;
#else
  return true;
#endif
}

void TabContents::SystemDragEnded() {
  if (render_view_host())
    render_view_host()->DragSourceSystemDragEnded();
  if (delegate())
    delegate()->DragEnded();
}

void TabContents::UpdateHistoryForNavigation(
    scoped_refptr<history::HistoryAddPageArgs> add_page_args) {
  if (profile()->IsOffTheRecord())
    return;

  // Add to history service.
  HistoryService* hs = profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs)
    hs->AddPage(*add_page_args);
}

void TabContents::UpdateHistoryPageTitle(const NavigationEntry& entry) {
  if (profile()->IsOffTheRecord())
    return;

  HistoryService* hs = profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs)
    hs->SetPageTitle(entry.virtual_url(), entry.title());
}

double TabContents::GetZoomLevel() const {
  HostZoomMap* zoom_map = profile()->GetHostZoomMap();
  if (!zoom_map)
    return 0;

  double zoom_level;
  if (temporary_zoom_settings_) {
    zoom_level = zoom_map->GetTemporaryZoomLevel(
        GetRenderProcessHost()->id(), render_view_host()->routing_id());
  } else {
    zoom_level = zoom_map->GetZoomLevel(GetURL());
  }
  return zoom_level;
}

int TabContents::GetZoomPercent(bool* enable_increment,
                                bool* enable_decrement) {
  *enable_decrement = *enable_increment = false;
  int percent = static_cast<int>(
      WebKit::WebView::zoomLevelToZoomFactor(GetZoomLevel()) * 100);
  *enable_decrement = percent > minimum_zoom_percent_;
  *enable_increment = percent < maximum_zoom_percent_;
  return percent;
}

void TabContents::ViewSource() {
  if (!delegate_)
    return;

  NavigationEntry* active_entry = controller().GetActiveEntry();
  if (!active_entry)
    return;

  delegate_->ViewSourceForTab(this, active_entry->url());
}

void TabContents::ViewFrameSource(const GURL& url,
                                  const std::string& content_state) {
  if (!delegate_)
    return;

  delegate_->ViewSourceForFrame(this, url, content_state);
}

void TabContents::SetContentRestrictions(int restrictions) {
  content_restrictions_ = restrictions;
  delegate()->ContentRestrictionsChanged(this);
}

void TabContents::OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                                    bool is_main_frame,
                                                    const GURL& url) {
  bool is_error_page = (url.spec() == chrome::kUnreachableWebDataURL);
  GURL validated_url(url);
  render_view_host()->FilterURL(ChildProcessSecurityPolicy::GetInstance(),
      GetRenderProcessHost()->id(), &validated_url);

  RenderViewHost* rvh =
      render_manager_.pending_render_view_host() ?
          render_manager_.pending_render_view_host() : render_view_host();
  // Notify observers about the start of the provisional load.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidStartProvisionalLoadForFrame(frame_id, is_main_frame,
                    validated_url, is_error_page, rvh));

  if (is_main_frame) {
    // Notify observers about the provisional change in the main frame URL.
    FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                      ProvisionalChangeToMainFrameUrl(url));
  }
}

void TabContents::OnDidRedirectProvisionalLoad(int32 page_id,
                                               const GURL& source_url,
                                               const GURL& target_url) {
  // TODO(creis): Remove this method and have the pre-rendering code listen to
  // the ResourceDispatcherHost's RESOURCE_RECEIVED_REDIRECT notification
  // instead.  See http://crbug.com/78512.
  NavigationEntry* entry;
  if (page_id == -1)
    entry = controller_.pending_entry();
  else
    entry = controller_.GetEntryWithPageID(GetSiteInstance(), page_id);
  if (!entry || entry->url() != source_url)
    return;

  // Notify observers about the provisional change in the main frame URL.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    ProvisionalChangeToMainFrameUrl(target_url));
}

void TabContents::OnDidFailProvisionalLoadWithError(
    int64 frame_id,
    bool is_main_frame,
    int error_code,
    const GURL& url,
    bool showing_repost_interstitial) {
  VLOG(1) << "Failed Provisional Load: " << url.possibly_invalid_spec()
          << ", error_code: " << error_code
          << " is_main_frame: " << is_main_frame
          << " showing_repost_interstitial: " << showing_repost_interstitial
          << " frame_id: " << frame_id;
  GURL validated_url(url);
  render_view_host()->FilterURL(ChildProcessSecurityPolicy::GetInstance(),
      GetRenderProcessHost()->id(), &validated_url);

  if (net::ERR_ABORTED == error_code) {
    // EVIL HACK ALERT! Ignore failed loads when we're showing interstitials.
    // This means that the interstitial won't be torn down properly, which is
    // bad. But if we have an interstitial, go back to another tab type, and
    // then load the same interstitial again, we could end up getting the first
    // interstitial's "failed" message (as a result of the cancel) when we're on
    // the second one.
    //
    // We can't tell this apart, so we think we're tearing down the current page
    // which will cause a crash later one. There is also some code in
    // RenderViewHostManager::RendererAbortedProvisionalLoad that is commented
    // out because of this problem.
    //
    // http://code.google.com/p/chromium/issues/detail?id=2855
    // Because this will not tear down the interstitial properly, if "back" is
    // back to another tab type, the interstitial will still be somewhat alive
    // in the previous tab type. If you navigate somewhere that activates the
    // tab with the interstitial again, you'll see a flash before the new load
    // commits of the interstitial page.
    if (showing_interstitial_page()) {
      LOG(WARNING) << "Discarding message during interstitial.";
      return;
    }

    // This will discard our pending entry if we cancelled the load (e.g., if we
    // decided to download the file instead of load it). Only discard the
    // pending entry if the URLs match, otherwise the user initiated a navigate
    // before the page loaded so that the discard would discard the wrong entry.
    NavigationEntry* pending_entry = controller_.pending_entry();
    if (pending_entry && pending_entry->url() == validated_url) {
      DidCancelLoading();
    }

    render_manager_.RendererAbortedProvisionalLoad(render_view_host());
  }

  // Send out a notification that we failed a provisional load with an error.
  ProvisionalLoadDetails details(
      is_main_frame, controller_.IsURLInPageNavigation(validated_url),
      validated_url, std::string(), false, frame_id);
  details.set_error_code(error_code);

  NotificationService::current()->Notify(
      NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
      Source<NavigationController>(&controller_),
      Details<ProvisionalLoadDetails>(&details));

  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidFailProvisionalLoad(frame_id, is_main_frame,
                    validated_url, error_code));
}

void TabContents::OnDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info) {
  base::StatsCounter cache("WebKit.CacheHit");
  cache.Increment();

  // Send out a notification that we loaded a resource from our memory cache.
  int cert_id = 0, cert_status = 0, security_bits = -1, connection_status = 0;
  SSLManager::DeserializeSecurityInfo(security_info,
                                      &cert_id, &cert_status,
                                      &security_bits,
                                      &connection_status);
  LoadFromMemoryCacheDetails details(url, GetRenderProcessHost()->id(),
                                     cert_id, cert_status);

  NotificationService::current()->Notify(
      NotificationType::LOAD_FROM_MEMORY_CACHE,
      Source<NavigationController>(&controller_),
      Details<LoadFromMemoryCacheDetails>(&details));
}

void TabContents::OnDidDisplayInsecureContent() {
  UserMetrics::RecordAction(UserMetricsAction("SSL.DisplayedInsecureContent"));
  displayed_insecure_content_ = true;
  SSLManager::NotifySSLInternalStateChanged();
}

void TabContents::OnDidRunInsecureContent(
    const std::string& security_origin, const GURL& target_url) {
  LOG(INFO) << security_origin << " ran insecure content from "
            << target_url.possibly_invalid_spec();
  UserMetrics::RecordAction(UserMetricsAction("SSL.RanInsecureContent"));
  controller_.ssl_manager()->DidRunInsecureContent(security_origin);
}

void TabContents::OnDocumentLoadedInFrame(int64 frame_id) {
  controller_.DocumentLoadedInFrame();
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DocumentLoadedInFrame(frame_id));
}

void TabContents::OnDidFinishLoad(int64 frame_id) {
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidFinishLoad(frame_id));
}

void TabContents::OnUpdateContentRestrictions(int restrictions) {
  SetContentRestrictions(restrictions);
}

void TabContents::OnGoToEntryAtOffset(int offset) {
  if (!delegate_ || delegate_->OnGoToEntryOffset(offset)) {
    NavigationEntry* entry = controller_.GetEntryAtOffset(offset);
    if (!entry)
      return;
    // Note that we don't call NavigationController::GotToOffset() as we don't
    // want to create a pending navigation entry (it might end up lingering
    // http://crbug.com/51680).
    entry->set_transition_type(entry->transition_type() |
                               PageTransition::FORWARD_BACK);
    NavigateToEntry(*entry, NavigationController::NO_RELOAD);
  }
}

void TabContents::OnUpdateZoomLimits(int minimum_percent,
                                     int maximum_percent,
                                     bool remember) {
  minimum_zoom_percent_ = minimum_percent;
  maximum_zoom_percent_ = maximum_percent;
  temporary_zoom_settings_ = !remember;
}

void TabContents::OnFocusedNodeChanged(bool is_editable_node) {
  NotificationService::current()->Notify(
      NotificationType::FOCUS_CHANGED_IN_PAGE,
      Source<TabContents>(this),
      Details<const bool>(&is_editable_node));
}

// Notifies the RenderWidgetHost instance about the fact that the page is
// loading, or done loading and calls the base implementation.
void TabContents::SetIsLoading(bool is_loading,
                               LoadNotificationDetails* details) {
  if (is_loading == is_loading_)
    return;

  if (!is_loading) {
    load_state_ = net::LOAD_STATE_IDLE;
    load_state_host_.clear();
    upload_size_ = 0;
    upload_position_ = 0;
  }

  render_manager_.SetIsLoading(is_loading);

  is_loading_ = is_loading;
  waiting_for_response_ = is_loading;

  if (delegate_)
    delegate_->LoadingStateChanged(this);
  NotifyNavigationStateChanged(INVALIDATE_LOAD);

  NotificationType type = is_loading ? NotificationType::LOAD_START :
      NotificationType::LOAD_STOP;
  NotificationDetails det = NotificationService::NoDetails();
  if (details)
      det = Details<LoadNotificationDetails>(details);
  NotificationService::current()->Notify(type,
      Source<NavigationController>(&controller_),
      det);
}

WebUI* TabContents::GetWebUIForCurrentState() {
  // When there is a pending navigation entry, we want to use the pending WebUI
  // that goes along with it to control the basic flags. For example, we want to
  // show the pending URL in the URL bar, so we want the display_url flag to
  // be from the pending entry.
  //
  // The confusion comes because there are multiple possibilities for the
  // initial load in a tab as a side effect of the way the RenderViewHostManager
  // works.
  //
  //  - For the very first tab the load looks "normal". The new tab Web UI is
  //    the pending one, and we want it to apply here.
  //
  //  - For subsequent new tabs, they'll get a new SiteInstance which will then
  //    get switched to the one previously associated with the new tab pages.
  //    This switching will cause the manager to commit the RVH/WebUI. So we'll
  //    have a committed Web UI in this case.
  //
  // This condition handles all of these cases:
  //
  //  - First load in first tab: no committed nav entry + pending nav entry +
  //    pending dom ui:
  //    -> Use pending Web UI if any.
  //
  //  - First load in second tab: no committed nav entry + pending nav entry +
  //    no pending Web UI:
  //    -> Use the committed Web UI if any.
  //
  //  - Second navigation in any tab: committed nav entry + pending nav entry:
  //    -> Use pending Web UI if any.
  //
  //  - Normal state with no load: committed nav entry + no pending nav entry:
  //    -> Use committed Web UI.
  if (controller_.pending_entry() &&
      (controller_.GetLastCommittedEntry() ||
       render_manager_.pending_web_ui()))
    return render_manager_.pending_web_ui();
  return render_manager_.web_ui();
}

WebUI::TypeID TabContents::GetWebUITypeForCurrentState() {
  return content::WebUIFactory::Get()->GetWebUIType(profile(), GetURL());
}

void TabContents::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (opener_web_ui_type_ != WebUI::kNoWebUI) {
    // If this is a window.open navigation, use the same WebUI as the renderer
    // that opened the window, as long as both renderers have the same
    // privileges.
    if (delegate_ && opener_web_ui_type_ == GetWebUITypeForCurrentState()) {
      WebUI* web_ui = content::GetContentClient()->browser()->
          GetWebUIFactory()->CreateWebUIForURL(this, GetURL());
      // web_ui might be NULL if the URL refers to a non-existent extension.
      if (web_ui) {
        render_manager_.SetWebUIPostCommit(web_ui);
        web_ui->RenderViewCreated(render_view_host());
      }
    }
    opener_web_ui_type_ = WebUI::kNoWebUI;
  }

  if (details.is_user_initiated_main_frame_load()) {
    // Clear the status bubble. This is a workaround for a bug where WebKit
    // doesn't let us know that the cursor left an element during a
    // transition (this is also why the mouse cursor remains as a hand after
    // clicking on a link); see bugs 1184641 and 980803. We don't want to
    // clear the bubble when a user navigates to a named anchor in the same
    // page.
    UpdateTargetURL(details.entry->page_id(), GURL());
  }

  // Allow the new page to set the title again.
  received_page_title_ = false;

  if (!details.is_in_page) {
    // Once the main frame is navigated, we're no longer considered to have
    // displayed insecure content.
    displayed_insecure_content_ = false;
  }

  // Close constrained windows if necessary.
  if (!net::RegistryControlledDomainService::SameDomainOrHost(
      details.previous_url, details.entry->url()))
    CloseConstrainedWindows();

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidNavigateMainFramePostCommit(details, params));
}

void TabContents::DidNavigateAnyFramePostCommit(
    RenderViewHost* render_view_host,
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // If we navigate, start showing messages again. This does nothing to prevent
  // a malicious script from spamming messages, since the script could just
  // reload the page to stop blocking.
  suppress_javascript_messages_ = false;

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidNavigateAnyFramePostCommit(details, params));
}

void TabContents::CloseConstrainedWindows() {
  // Clear out any constrained windows since we are leaving this page entirely.
  // To ensure that we iterate over every element in child_windows_ we
  // need to use a copy of child_windows_. Otherwise if
  // window->CloseConstrainedWindow() modifies child_windows_ we could end up
  // skipping some elements.
  ConstrainedWindowList child_windows_copy(child_windows_);
  for (ConstrainedWindowList::iterator it = child_windows_copy.begin();
       it != child_windows_copy.end(); ++it) {
    ConstrainedWindow* window = *it;
    if (window) {
      window->CloseConstrainedWindow();
      BlockTabContent(false);
    }
  }
}

void TabContents::UpdateAlternateErrorPageURL() {
  GURL url = GetAlternateErrorPageURL();
  render_view_host()->Send(new ViewMsg_SetAltErrorPageURL(
      render_view_host()->routing_id(), url));
}

void TabContents::UpdateWebPreferences() {
  render_view_host()->Send(new ViewMsg_UpdateWebPreferences(
      render_view_host()->routing_id(), GetWebkitPrefs()));
}

void TabContents::UpdateZoomLevel() {
  render_view_host()->Send(new ViewMsg_SetZoomLevel(
      render_view_host()->routing_id(), GetZoomLevel()));
}

void TabContents::UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                             RenderViewHost* rvh) {
  // If we are creating a RVH for a restored controller, then we might
  // have more page IDs than the SiteInstance's current max page ID.  We must
  // make sure that the max page ID is larger than any restored page ID.
  // Note that it is ok for conflicting page IDs to exist in another tab
  // (i.e., NavigationController), but if any page ID is larger than the max,
  // the back/forward list will get confused.
  int max_restored_page_id = controller_.max_restored_page_id();
  if (max_restored_page_id > 0) {
    int curr_max_page_id = site_instance->max_page_id();
    if (max_restored_page_id > curr_max_page_id) {
      // Need to update the site instance immediately.
      site_instance->UpdateMaxPageID(max_restored_page_id);

      // Also tell the renderer to update its internal representation.  We
      // need to reserve enough IDs to make all restored page IDs less than
      // the max.
      if (curr_max_page_id < 0)
        curr_max_page_id = 0;
      rvh->Send(new ViewMsg_ReservePageIDRange(
          rvh->routing_id(), max_restored_page_id - curr_max_page_id));
    }
  }
}

scoped_refptr<history::HistoryAddPageArgs>
TabContents::CreateHistoryAddPageArgs(
    const GURL& virtual_url,
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  scoped_refptr<history::HistoryAddPageArgs> add_page_args(
      new history::HistoryAddPageArgs(
          params.url, base::Time::Now(), this, params.page_id, params.referrer,
          params.redirects, params.transition, history::SOURCE_BROWSED,
          details.did_replace_entry));
  if (PageTransition::IsMainFrame(params.transition) &&
      virtual_url != params.url) {
    // Hack on the "virtual" URL so that it will appear in history. For some
    // types of URLs, we will display a magic URL that is different from where
    // the page is actually navigated. We want the user to see in history what
    // they saw in the URL bar, so we add the virtual URL as a redirect.  This
    // only applies to the main frame, as the virtual URL doesn't apply to
    // sub-frames.
    add_page_args->url = virtual_url;
    if (!add_page_args->redirects.empty())
      add_page_args->redirects.back() = virtual_url;
  }
  return add_page_args;
}

bool TabContents::UpdateTitleForEntry(NavigationEntry* entry,
                                      const std::wstring& title) {
  // For file URLs without a title, use the pathname instead. In the case of a
  // synthesized title, we don't want the update to count toward the "one set
  // per page of the title to history."
  string16 final_title;
  bool explicit_set;
  if (entry->url().SchemeIsFile() && title.empty()) {
    final_title = UTF8ToUTF16(entry->url().ExtractFileName());
    explicit_set = false;  // Don't count synthetic titles toward the set limit.
  } else {
    TrimWhitespace(WideToUTF16Hack(title), TRIM_ALL, &final_title);
    explicit_set = true;
  }

  if (final_title == entry->title())
    return false;  // Nothing changed, don't bother.

  entry->set_title(final_title);

  if (!received_page_title_) {
    UpdateHistoryPageTitle(*entry);
    received_page_title_ = explicit_set;
  }

  // Lastly, set the title for the view.
  view_->SetPageTitle(UTF16ToWideHack(final_title));

  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_TITLE_UPDATED,
      Source<TabContents>(this),
      NotificationService::NoDetails());

  return true;
}

void TabContents::NotifySwapped() {
  // After sending out a swap notification, we need to send a disconnect
  // notification so that clients that pick up a pointer to |this| can NULL the
  // pointer.  See Bug 1230284.
  notify_disconnection_ = true;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_SWAPPED,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::NotifyConnected() {
  notify_disconnection_ = true;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_CONNECTED,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

void TabContents::NotifyDisconnected() {
  if (!notify_disconnection_)
    return;

  notify_disconnection_ = false;
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_DISCONNECTED,
      Source<TabContents>(this),
      NotificationService::NoDetails());
}

RenderViewHostDelegate::View* TabContents::GetViewDelegate() {
  return view_.get();
}

RenderViewHostDelegate::RendererManagement*
TabContents::GetRendererManagementDelegate() {
  return &render_manager_;
}

RenderViewHostDelegate::BookmarkDrag* TabContents::GetBookmarkDragDelegate() {
  return bookmark_drag_;
}

void TabContents::SetBookmarkDragDelegate(
    RenderViewHostDelegate::BookmarkDrag* bookmark_drag) {
  bookmark_drag_ = bookmark_drag;
}

RendererPreferences TabContents::GetRendererPrefs(Profile* profile) const {
  return renderer_preferences_;
}

TabContents* TabContents::GetAsTabContents() {
  return this;
}

ViewType::Type TabContents::GetRenderViewType() const {
  return ViewType::TAB_CONTENTS;
}

int TabContents::GetBrowserWindowID() const {
  return controller().window_id().id();
}

void TabContents::RenderViewCreated(RenderViewHost* render_view_host) {
  NotificationService::current()->Notify(
      NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
      Source<TabContents>(this),
      Details<RenderViewHost>(render_view_host));
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (!entry)
    return;

  // When we're creating views, we're still doing initial setup, so we always
  // use the pending Web UI rather than any possibly existing committed one.
  if (render_manager_.pending_web_ui())
    render_manager_.pending_web_ui()->RenderViewCreated(render_view_host);

  if (entry->IsViewSourceMode()) {
    // Put the renderer in view source mode.
    render_view_host->Send(
        new ViewMsg_EnableViewSourceMode(render_view_host->routing_id()));
  }

  view()->RenderViewCreated(render_view_host);
}

void TabContents::RenderViewReady(RenderViewHost* rvh) {
  if (rvh != render_view_host()) {
    // Don't notify the world, since this came from a renderer in the
    // background.
    return;
  }

  NotifyConnected();
  bool was_crashed = is_crashed();
  SetIsCrashed(base::TERMINATION_STATUS_STILL_RUNNING, 0);

  // Restore the focus to the tab (otherwise the focus will be on the top
  // window).
  if (was_crashed && !FocusLocationBarByDefault() &&
      (!delegate_ || delegate_->ShouldFocusPageAfterCrash())) {
    Focus();
  }
}

void TabContents::RenderViewGone(RenderViewHost* rvh,
                                 base::TerminationStatus status,
                                 int error_code) {
  if (rvh != render_view_host()) {
    // The pending page's RenderViewHost is gone.
    return;
  }

  // Let observers know first and give them a chance to act.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, RenderViewGone());

  SetIsLoading(false, NULL);
  NotifyDisconnected();
  SetIsCrashed(status, error_code);

  // Tell the view that we've crashed so it can prepare the sad tab page.
  // Only do this if we're not in browser shutdown, so that TabContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID)
    view_->OnTabCrashed(status, error_code);

  // Hide any visible hung renderer warning for this web contents' process.
  browser::HideHungRendererDialog(this);
}

void TabContents::RenderViewDeleted(RenderViewHost* rvh) {
  render_manager_.RenderViewDeleted(rvh);
}

void TabContents::DidNavigate(RenderViewHost* rvh,
                              const ViewHostMsg_FrameNavigate_Params& params) {
  int extra_invalidate_flags = 0;

  if (PageTransition::IsMainFrame(params.transition)) {
    bool was_bookmark_bar_visible = ShouldShowBookmarkBar();

    render_manager_.DidNavigateMainFrame(rvh);

    if (was_bookmark_bar_visible != ShouldShowBookmarkBar())
      extra_invalidate_flags |= INVALIDATE_BOOKMARK_BAR;
  }

  // Update the site of the SiteInstance if it doesn't have one yet.
  if (!GetSiteInstance()->has_site())
    GetSiteInstance()->SetSite(params.url);

  // Need to update MIME type here because it's referred to in
  // UpdateNavigationCommands() called by RendererDidNavigate() to
  // determine whether or not to enable the encoding menu.
  // It's updated only for the main frame. For a subframe,
  // RenderView::UpdateURL does not set params.contents_mime_type.
  // (see http://code.google.com/p/chromium/issues/detail?id=2929 )
  // TODO(jungshik): Add a test for the encoding menu to avoid
  // regressing it again.
  if (PageTransition::IsMainFrame(params.transition))
    contents_mime_type_ = params.contents_mime_type;

  NavigationController::LoadCommittedDetails details;
  bool did_navigate = controller_.RendererDidNavigate(
      params, extra_invalidate_flags, &details);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != NavigationType::NAV_IGNORE) {
    // For AUTO_SUBFRAME navigations, an event for the main frame is generated
    // that is not recorded in the navigation history. For the purpose of
    // tracking navigation events, we treat this event as a sub frame navigation
    // event.
    bool is_main_frame = did_navigate ? details.is_main_frame : false;
    PageTransition::Type transition_type = params.transition;
    // Whether or not a page transition was triggered by going backward or
    // forward in the history is only stored in the navigation controller's
    // entry list.
    if (did_navigate &&
        (controller_.GetActiveEntry()->transition_type() &
            PageTransition::FORWARD_BACK)) {
      transition_type = params.transition | PageTransition::FORWARD_BACK;
    }
    // Notify observers about the commit of the provisional load.
    FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                      DidCommitProvisionalLoadForFrame(params.frame_id,
                      is_main_frame, params.url, transition_type));
  }

  // Update history. Note that this needs to happen after the entry is complete,
  // which WillNavigate[Main,Sub]Frame will do before this function is called.
  if (params.should_update_history) {
    // Most of the time, the displayURL matches the loaded URL, but for about:
    // URLs, we use a data: URL as the real value.  We actually want to save the
    // about: URL to the history db and keep the data: URL hidden. This is what
    // the TabContents' URL getter does.
    scoped_refptr<history::HistoryAddPageArgs> add_page_args(
        CreateHistoryAddPageArgs(GetURL(), details, params));
    if (!delegate() ||
        delegate()->ShouldAddNavigationToHistory(*add_page_args,
                                                 details.type)) {
      UpdateHistoryForNavigation(add_page_args);
    }
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // Run post-commit tasks.
  if (details.is_main_frame)
    DidNavigateMainFramePostCommit(details, params);
  DidNavigateAnyFramePostCommit(rvh, details, params);
}

void TabContents::UpdateState(RenderViewHost* rvh,
                              int32 page_id,
                              const std::string& state) {
  // Ensure that this state update comes from either the active RVH or one of
  // the swapped out RVHs.  We don't expect to hear from any other RVHs.
  DCHECK(rvh == render_view_host() || render_manager_.IsSwappedOut(rvh));

  // We must be prepared to handle state updates for any page, these occur
  // when the user is scrolling and entering form data, as well as when we're
  // leaving a page, in which case our state may have already been moved to
  // the next page. The navigation controller will look up the appropriate
  // NavigationEntry and update it when it is notified via the delegate.

  int entry_index = controller_.GetEntryIndexWithPageID(
      rvh->site_instance(), page_id);
  if (entry_index < 0)
    return;
  NavigationEntry* entry = controller_.GetEntryAtIndex(entry_index);

  if (state == entry->content_state())
    return;  // Nothing to update.
  entry->set_content_state(state);
  controller_.NotifyEntryChanged(entry, entry_index);
}

void TabContents::UpdateTitle(RenderViewHost* rvh,
                              int32 page_id, const std::wstring& title) {
  // If we have a title, that's a pretty good indication that we've started
  // getting useful data.
  SetNotWaitingForResponse();

  DCHECK(rvh == render_view_host());
  NavigationEntry* entry = controller_.GetEntryWithPageID(rvh->site_instance(),
                                                          page_id);
  if (!entry || !UpdateTitleForEntry(entry, title))
    return;

  // Broadcast notifications when the UI should be updated.
  if (entry == controller_.GetEntryAtOffset(0))
    NotifyNavigationStateChanged(INVALIDATE_TITLE);
}

void TabContents::UpdateEncoding(RenderViewHost* render_view_host,
                                 const std::string& encoding) {
  set_encoding(encoding);
}

void TabContents::UpdateTargetURL(int32 page_id, const GURL& url) {
  if (delegate())
    delegate()->UpdateTargetURL(this, url);
}

void TabContents::UpdateInspectorSetting(const std::string& key,
                                         const std::string& value) {
  RenderViewHostDelegateHelper::UpdateInspectorSetting(profile(), key, value);
}

void TabContents::ClearInspectorSettings() {
  RenderViewHostDelegateHelper::ClearInspectorSettings(profile());
}

void TabContents::Close(RenderViewHost* rvh) {
  // The UI may be in an event-tracking loop, such as between the
  // mouse-down and mouse-up in text selection or a button click.
  // Defer the close until after tracking is complete, so that we
  // don't free objects out from under the UI.
  // TODO(shess): This could probably be integrated with the
  // IsDoingDrag() test below.  Punting for now because I need more
  // research to understand how this impacts platforms other than Mac.
  // TODO(shess): This could get more fine-grained.  For instance,
  // closing a tab in another window while selecting text in the
  // current window's Omnibox should be just fine.
  if (view()->IsEventTracking()) {
    view()->CloseTabAfterEventTracking();
    return;
  }

  // If we close the tab while we're in the middle of a drag, we'll crash.
  // Instead, cancel the drag and close it as soon as the drag ends.
  if (view()->IsDoingDrag()) {
    view()->CancelDragAndCloseTab();
    return;
  }

  // Ignore this if it comes from a RenderViewHost that we aren't showing.
  if (delegate() && rvh == render_view_host())
    delegate()->CloseContents(this);
}

void TabContents::RequestMove(const gfx::Rect& new_bounds) {
  if (delegate() && delegate()->IsPopupOrPanel(this))
    delegate()->MoveContents(this, new_bounds);
}

void TabContents::DidStartLoading() {
  SetIsLoading(true, NULL);

  if (delegate() && content_restrictions_) {
      content_restrictions_ = 0;
      delegate()->ContentRestrictionsChanged(this);
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, DidStartLoading());
}

void TabContents::DidStopLoading() {
  scoped_ptr<LoadNotificationDetails> details;

  NavigationEntry* entry = controller_.GetActiveEntry();
  // An entry may not exist for a stop when loading an initial blank page or
  // if an iframe injected by script into a blank page finishes loading.
  if (entry) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - current_load_start_;

    details.reset(new LoadNotificationDetails(
        entry->virtual_url(),
        entry->transition_type(),
        elapsed,
        &controller_,
        controller_.GetCurrentEntryIndex()));
  }

  SetIsLoading(false, details.get());

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, DidStopLoading());
}

void TabContents::DidCancelLoading() {
  controller_.DiscardNonCommittedEntries();

  // Update the URL display.
  NotifyNavigationStateChanged(TabContents::INVALIDATE_URL);
}

void TabContents::DidChangeLoadProgress(double progress) {
  if (delegate())
    delegate()->LoadProgressChanged(progress);
}

void TabContents::DocumentOnLoadCompletedInMainFrame(
    RenderViewHost* render_view_host,
    int32 page_id) {
  NotificationService::current()->Notify(
      NotificationType::LOAD_COMPLETED_MAIN_FRAME,
      Source<TabContents>(this),
      Details<int>(&page_id));
}

void TabContents::RequestOpenURL(const GURL& url, const GURL& referrer,
                                 WindowOpenDisposition disposition) {
  if (render_manager_.web_ui()) {
    // When we're a Web UI, it will provide a page transition type for us (this
    // is so the new tab page can specify AUTO_BOOKMARK for automatically
    // generated suggestions).
    //
    // Note also that we hide the referrer for Web UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    OpenURL(url, GURL(), disposition,
            render_manager_.web_ui()->link_transition_type());
  } else {
    OpenURL(url, referrer, disposition, PageTransition::LINK);
  }
}

void TabContents::RunJavaScriptMessage(
    const RenderViewHost* rvh,
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Suppress javascript messages when requested and when inside a constrained
  // popup window (because that activates them and breaks them out of the
  // constrained window jail).
  // Also suppress messages when showing an interstitial. The interstitial is
  // shown over the previous page, we don't want the hidden page dialogs to
  // interfere with the interstitial.
  bool suppress_this_message =
      rvh->is_swapped_out() ||
      suppress_javascript_messages_ ||
      showing_interstitial_page() ||
      (delegate() && delegate()->ShouldSuppressDialogs());
  if (delegate())
    suppress_this_message |=
        (delegate()->GetConstrainingContents(this) != this);

  *did_suppress_message = suppress_this_message;

  if (!suppress_this_message) {
    base::TimeDelta time_since_last_message(
        base::TimeTicks::Now() - last_javascript_message_dismissal_);
    bool show_suppress_checkbox = false;
    // Show a checkbox offering to suppress further messages if this message is
    // being displayed within kJavascriptMessageExpectedDelay of the last one.
    if (time_since_last_message <
        base::TimeDelta::FromMilliseconds(
            chrome::kJavascriptMessageExpectedDelay))
      show_suppress_checkbox = true;

    RunJavascriptMessageBox(profile(), this, frame_url, flags, message,
                            default_prompt, show_suppress_checkbox, reply_msg);
  } else {
    // If we are suppressing messages, just reply as is if the user immediately
    // pressed "Cancel".
    OnMessageBoxClosed(reply_msg, false, std::wstring());
  }
}

void TabContents::RunBeforeUnloadConfirm(const RenderViewHost* rvh,
                                         const std::wstring& message,
                                         IPC::Message* reply_msg) {
  if (delegate())
    delegate()->WillRunBeforeUnloadConfirm();
  bool suppress_this_message = rvh->is_swapped_out() ||
      (delegate() && delegate()->ShouldSuppressDialogs());
  if (suppress_this_message) {
    render_view_host()->JavaScriptMessageBoxClosed(reply_msg, true,
                                                   std::wstring());
    return;
  }
  is_showing_before_unload_dialog_ = true;
  RunBeforeUnloadDialog(this, message, reply_msg);
}

GURL TabContents::GetAlternateErrorPageURL() const {
  return content::GetContentClient()->browser()->GetAlternateErrorPageURL(this);
}

WebPreferences TabContents::GetWebkitPrefs() {
  Profile* profile = render_view_host()->process()->profile();
  bool is_web_ui = false;
  WebPreferences web_prefs =
      RenderViewHostDelegateHelper::GetWebkitPrefs(profile, is_web_ui);

  // Force accelerated compositing and 2d canvas off for chrome: and
  // chrome-extension: pages.
  if (GetURL().SchemeIs(chrome::kChromeDevToolsScheme) ||
      GetURL().SchemeIs(chrome::kChromeUIScheme)) {
    web_prefs.accelerated_compositing_enabled = false;
    web_prefs.accelerated_2d_canvas_enabled = false;
  }

#if defined(OS_MACOSX)
  // Disable accelerated compositing if IOSurface's are not supported,
  // as is the case in 10.5.
  if (!IOSurfaceSupport::Initialize())
      web_prefs.accelerated_compositing_enabled = false;
#endif

  return web_prefs;
}

void TabContents::OnUserGesture() {
  // Notify observers.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, DidGetUserGesture());
  ExternalProtocolHandler::PermitLaunchUrl();
}

void TabContents::OnIgnoredUIEvent() {
  if (constrained_window_count()) {
    ConstrainedWindow* window = *constrained_window_begin();
    window->FocusConstrainedWindow();
  }
}

void TabContents::OnCrossSiteResponse(int new_render_process_host_id,
                                      int new_request_id) {
  // Allows the TabContents to react when a cross-site response is ready to be
  // delivered to a pending RenderViewHost.  We must first run the onunload
  // handler of the old RenderViewHost before we can allow it to proceed.
  render_manager_.OnCrossSiteResponse(new_render_process_host_id,
                                      new_request_id);
}

void TabContents::RendererUnresponsive(RenderViewHost* rvh,
                                       bool is_during_unload) {
  // Don't show hung renderer dialog for a swapped out RVH.
  if (rvh != render_view_host())
    return;

  if (is_during_unload) {
    // Hang occurred while firing the beforeunload/unload handler.
    // Pretend the handler fired so tab closing continues as if it had.
    rvh->set_sudden_termination_allowed(true);

    if (!render_manager_.ShouldCloseTabOnUnresponsiveRenderer())
      return;

    // If the tab hangs in the beforeunload/unload handler there's really
    // nothing we can do to recover. Pretend the unload listeners have
    // all fired and close the tab. If the hang is in the beforeunload handler
    // then the user will not have the option of cancelling the close.
    Close(rvh);
    return;
  }

  if (render_view_host() && render_view_host()->IsRenderViewLive() &&
      (!delegate() || delegate()->ShouldShowHungRendererDialog())) {
    browser::ShowHungRendererDialog(this);
  }
}

void TabContents::RendererResponsive(RenderViewHost* render_view_host) {
  browser::HideHungRendererDialog(this);
}

void TabContents::LoadStateChanged(const GURL& url,
                                   net::LoadState load_state,
                                   uint64 upload_position,
                                   uint64 upload_size) {
  load_state_ = load_state;
  upload_position_ = upload_position;
  upload_size_ = upload_size;
  load_state_host_ = net::IDNToUnicode(url.host(),
      profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  if (load_state_ == net::LOAD_STATE_READING_RESPONSE)
    SetNotWaitingForResponse();
  if (is_loading())
    NotifyNavigationStateChanged(INVALIDATE_LOAD | INVALIDATE_TAB);
}

bool TabContents::IsExternalTabContainer() const {
  if (!delegate())
    return false;

  return delegate()->IsExternalTabContainer();
}

void TabContents::WorkerCrashed() {
  if (delegate())
    delegate()->WorkerCrashed();
}

void TabContents::RequestDesktopNotificationPermission(
    const GURL& source_origin, int callback_context) {
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile());
  service->RequestPermission(
      source_origin, GetRenderProcessHost()->id(),
      render_view_host()->routing_id(), callback_context, this);
}

void TabContents::BeforeUnloadFiredFromRenderManager(
    bool proceed,
    bool* proceed_to_fire_unload) {
  if (delegate())
    delegate()->BeforeUnloadFired(this, proceed, proceed_to_fire_unload);
}

void TabContents::DidStartLoadingFromRenderManager(
      RenderViewHost* render_view_host) {
  DidStartLoading();
}

void TabContents::RenderViewGoneFromRenderManager(
    RenderViewHost* render_view_host) {
  DCHECK(crashed_status_ != base::TERMINATION_STATUS_STILL_RUNNING);
  RenderViewGone(render_view_host, crashed_status_, crashed_error_code_);
}

void TabContents::UpdateRenderViewSizeForRenderManager() {
  // TODO(brettw) this is a hack. See TabContentsView::SizeContents.
  gfx::Size size = view_->GetContainerSize();
  // 0x0 isn't a valid window size (minimal window size is 1x1) but it may be
  // here during container initialization and normal window size will be set
  // later. In case of tab duplication this resizing to 0x0 prevents setting
  // normal size later so just ignore it.
  if (!size.IsEmpty())
    view_->SizeContents(size);
}

void TabContents::NotifySwappedFromRenderManager() {
  NotifySwapped();
}

NavigationController& TabContents::GetControllerForRenderManager() {
  return controller();
}

WebUI* TabContents::CreateWebUIForRenderManager(const GURL& url) {
  return content::WebUIFactory::Get()->CreateWebUIForURL(this, url);
}

NavigationEntry*
TabContents::GetLastCommittedNavigationEntryForRenderManager() {
  return controller_.GetLastCommittedEntry();
}

bool TabContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  RenderWidgetHostView* rwh_view = view_->CreateViewForWidget(render_view_host);

  // Now that the RenderView has been created, we need to tell it its size.
  rwh_view->SetSize(view_->GetContainerSize());

  if (!render_view_host->CreateRenderView(string16()))
    return false;

  UpdateMaxPageIDIfNecessary(render_view_host->site_instance(),
                             render_view_host);
  return true;
}

void TabContents::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PREF_CHANGED: {
      std::string* pref_name_in = Details<std::string>(details).ptr();
      DCHECK(Source<PrefService>(source).ptr() == profile()->GetPrefs());
      if (*pref_name_in == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL();
      } else if ((*pref_name_in == prefs::kDefaultCharset) ||
                 StartsWithASCII(*pref_name_in, "webkit.webprefs.", true)) {
        UpdateWebPreferences();
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
        UpdateZoomLevel();
      } else if (*pref_name_in == prefs::kEnableReferrers) {
        renderer_preferences_util::UpdateFromSystemSettings(
            &renderer_preferences_, profile());
        render_view_host()->SyncRendererPrefs();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    case NotificationType::RENDER_WIDGET_HOST_DESTROYED:
      view_->RenderWidgetHostDestroyed(Source<RenderWidgetHost>(source).ptr());
      break;

#if defined(OS_LINUX)
    case NotificationType::BROWSER_THEME_CHANGED: {
      renderer_preferences_util::UpdateFromSystemSettings(
          &renderer_preferences_, profile());
      render_view_host()->SyncRendererPrefs();
      break;
    }
#endif

    case NotificationType::USER_STYLE_SHEET_UPDATED:
      UpdateWebPreferences();
      break;

    case NotificationType::GOOGLE_URL_UPDATED:
      UpdateAlternateErrorPageURL();
      break;

    default:
      NOTREACHED();
  }
}

gfx::NativeWindow TabContents::GetMessageBoxRootWindow() {
  return view_->GetTopLevelNativeWindow();
}

void TabContents::OnMessageBoxClosed(IPC::Message* reply_msg,
                                     bool success,
                                     const std::wstring& prompt) {
  last_javascript_message_dismissal_ = base::TimeTicks::Now();
  if (is_showing_before_unload_dialog_ && !success) {
    // If a beforeunload dialog is canceled, we need to stop the throbber from
    // spinning, since we forced it to start spinning in Navigate.
    DidStopLoading();

    tab_close_start_time_ = base::TimeTicks();
  }
  is_showing_before_unload_dialog_ = false;
  render_view_host()->JavaScriptMessageBoxClosed(reply_msg, success, prompt);
}

void TabContents::SetSuppressMessageBoxes(bool suppress_message_boxes) {
  set_suppress_javascript_messages(suppress_message_boxes);
}

TabContents* TabContents::AsTabContents() {
  return this;
}

ExtensionHost* TabContents::AsExtensionHost() {
  return NULL;
}

void TabContents::set_encoding(const std::string& encoding) {
  encoding_ = content::GetContentClient()->browser()->
      GetCanonicalEncodingNameByAliasName(encoding);
}

void TabContents::SwapInRenderViewHost(RenderViewHost* rvh) {
  render_manager_.SwapInRenderViewHost(rvh);
}

void TabContents::CreateViewAndSetSizeForRVH(RenderViewHost* rvh) {
  RenderWidgetHostView* rwh_view = view()->CreateViewForWidget(rvh);
  rwh_view->SetSize(view()->GetContainerSize());
}

void TabContents::OnOnlineStateChanged(bool online) {
  render_view_host()->Send(new ViewMsg_NetworkStateChanged(
      render_view_host()->routing_id(), online));
}
