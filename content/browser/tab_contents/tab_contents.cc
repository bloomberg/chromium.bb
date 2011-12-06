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
#include "content/browser/browser_context.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/debugger/devtools_manager_impl.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/intents/intents_host_impl.h"
#include "content/browser/load_from_memory_cache_details.h"
#include "content/browser/load_notification_details.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_request_details.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/provisional_load_details.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/tab_contents/title_updated_details.h"
#include "content/browser/user_metrics.h"
#include "content/browser/webui/web_ui_factory.h"
#include "content/common/intents_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/codec/png_codec.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/web_intent_data.h"
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
//   it creates a pending_render_view_host_.
// - The pending RVH is "suspended," so that no navigation messages are sent to
//   its renderer until the onbeforeunload JavaScript handler has a chance to
//   run in the current RVH.
// - The pending RVH tells CrossSiteRequestManager (a thread-safe singleton)
//   that it has a pending cross-site request.  ResourceDispatcherHost will
//   check for this when the response arrives.
// - The current RVH runs its onbeforeunload handler.  If it returns false, we
//   cancel all the pending logic.  Otherwise we allow the pending RVH to send
//   the navigation request to its renderer.
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread for the
//   main resource load on the pending RVH. It checks CrossSiteRequestManager
//   to see that it is a cross-site request, and installs a
//   CrossSiteResourceHandler.
// - When RDH receives a response, the BufferedResourceHandler determines
//   whether it is a download.  If so, it sends a message to the new renderer
//   causing it to cancel the request, and the download proceeds. For now, the
//   pending RVH remains until the next DidNavigate event for this TabContents.
//   This isn't ideal, but it doesn't affect any functionality.
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
//   pending RVH.
// - The previous renderer is kept swapped out in RenderViewHostManager in case
//   the user goes back.  The process only stays live if another tab is using
//   it, but if so, the existing frame relationships will be maintained.
//
// It is possible that we trigger a new navigation after we have received
// a SwapOut_ACK message but before the FrameNavigation has been confirmed.
// In this case the old RVH has been swapped out but the new one has not
// replaced it, yet. Therefore, we cancel the pending RVH and skip the unloading
// of the old RVH.

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsManagerImpl;

namespace {

// Amount of time we wait between when a key event is received and the renderer
// is queried for its state and pushed to the NavigationEntry.
const int kQueryStateDelay = 5000;

const int kSyncWaitDelay = 40;

static const char kDotGoogleDotCom[] = ".google.com";

#if defined(OS_WIN)

BOOL CALLBACK InvalidateWindow(HWND hwnd, LPARAM lparam) {
  // Note: erase is required to properly paint some widgets borders. This can
  // be seen with textfields.
  InvalidateRect(hwnd, NULL, TRUE);
  return TRUE;
}
#endif

ViewMsg_Navigate_Type::Value GetNavigationType(
    content::BrowserContext* browser_context, const NavigationEntry& entry,
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
      browser_context->DidLastSessionExitCleanly())
    return ViewMsg_Navigate_Type::RESTORE;

  return ViewMsg_Navigate_Type::NORMAL;
}

void MakeNavigateParams(const NavigationEntry& entry,
                        const NavigationController& controller,
                        TabContentsDelegate* delegate,
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
      GetNavigationType(controller.browser_context(), entry, reload_type);
  params->request_time = base::Time::Now();
  params->extra_headers = entry.extra_headers();
  params->transferred_request_child_id =
      entry.transferred_global_request_id().child_id;
  params->transferred_request_request_id =
      entry.transferred_global_request_id().request_id;

  if (delegate)
    delegate->AddNavigationHeaders(params->url, &params->extra_headers);
}

}  // namespace


// TabContents ----------------------------------------------------------------

TabContents::TabContents(content::BrowserContext* browser_context,
                         SiteInstance* site_instance,
                         int routing_id,
                         const TabContents* base_tab_contents,
                         SessionStorageNamespace* session_storage_namespace)
    : delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(controller_(
          this, browser_context, session_storage_namespace)),
      ALLOW_THIS_IN_INITIALIZER_LIST(view_(
          content::GetContentClient()->browser()->CreateTabContentsView(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(render_manager_(this, this)),
      is_loading_(false),
      crashed_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      crashed_error_code_(0),
      waiting_for_response_(false),
      max_page_id_(-1),
      load_state_(net::LOAD_STATE_IDLE, string16()),
      upload_size_(0),
      upload_position_(0),
      displayed_insecure_content_(false),
      capturing_contents_(false),
      is_being_destroyed_(false),
      notify_disconnection_(false),
      dialog_creator_(NULL),
#if defined(OS_WIN)
      message_box_active_(CreateEvent(NULL, TRUE, FALSE, NULL)),
#endif
      is_showing_before_unload_dialog_(false),
      opener_web_ui_type_(WebUI::kNoWebUI),
      closed_by_user_gesture_(false),
      minimum_zoom_percent_(
          static_cast<int>(content::kMinimumZoomFactor * 100)),
      maximum_zoom_percent_(
          static_cast<int>(content::kMaximumZoomFactor * 100)),
      temporary_zoom_settings_(false),
      content_restrictions_(0),
      view_type_(content::VIEW_TYPE_TAB_CONTENTS) {
  render_manager_.Init(browser_context, site_instance, routing_id);

  // We have the initial size of the view be based on the size of the passed in
  // tab contents (normally a tab from the same window).
  view_->CreateView(base_tab_contents ?
      base_tab_contents->view()->GetContainerSize() : gfx::Size());

#if defined(ENABLE_JAVA_BRIDGE)
  java_bridge_dispatcher_host_manager_.reset(
      new JavaBridgeDispatcherHostManager(this));
#endif
}

TabContents::~TabContents() {
  is_being_destroyed_ = true;

  // Clear out any JavaScript state.
  if (dialog_creator_)
    dialog_creator_->ResetJavaScriptState(this);

  NotifyDisconnected();

  // Notify any observer that have a reference on this tab contents.
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
      content::Source<TabContents>(this),
      content::NotificationService::NoDetails());

  // TODO(brettw) this should be moved to the view.
#if defined(OS_WIN) && !defined(USE_AURA)
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

  set_delegate(NULL);
}

// TODO(cbentzel): Either remove the debugging code, or rename to SetDelegate.
void TabContents::set_delegate(TabContentsDelegate* delegate) {
  if (delegate == delegate_)
    return;
  if (delegate_)
    delegate_->Detach(this);
  delegate_ = delegate;
  if (delegate_)
    delegate_->Attach(this);
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
    IPC_MESSAGE_HANDLER(IntentsHostMsg_RegisterIntentService,
                        OnRegisterIntentService)
    IPC_MESSAGE_HANDLER(IntentsHostMsg_WebIntentDispatch,
                        OnWebIntentDispatch)
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_SaveURLAs, OnSaveURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_EnumerateDirectory, OnEnumerateDirectory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_JSOutOfMemory, OnJSOutOfMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterProtocolHandler,
                        OnRegisterProtocolHandler)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Find_Reply, OnFindReply)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CrashedPlugin, OnCrashedPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AppCacheAccessed, OnAppCacheAccessed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!message_is_ok) {
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RVD"));
    GetRenderProcessHost()->ReceivedBadMessage();
  }

  return handled;
}

void TabContents::RunFileChooser(
    RenderViewHost* render_view_host,
    const content::FileChooserParams& params) {
  delegate()->RunFileChooser(this, params);
}

content::RenderProcessHost* TabContents::GetRenderProcessHost() const {
  if (render_manager_.current_host())
    return render_manager_.current_host()->process();
  else
    return NULL;
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
  std::string accept_languages =
      content::GetContentClient()->browser()->GetAcceptLangs(
          this->browser_context());
  if (entry) {
    return entry->GetTitleForDisplay(accept_languages);
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
    return entry->GetTitleForDisplay(accept_languages);
  }

  // |page_title_when_no_navigation_entry_| is finally used
  // if no title cannot be retrieved.
  return page_title_when_no_navigation_entry_;
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

SiteInstance* TabContents::GetPendingSiteInstance() const {
  RenderViewHost* dest_rvh = render_manager_.pending_render_view_host() ?
      render_manager_.pending_render_view_host() :
      render_manager_.current_host();
  return dest_rvh->site_instance();
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

  last_selected_time_ = base::TimeTicks::Now();

  FOR_EACH_OBSERVER(TabContentsObserver, observers_, DidBecomeSelected());
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

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_HIDDEN,
      content::Source<TabContents>(this),
      content::NotificationService::NoDetails());
}

void TabContents::Activate() {
  if (delegate_)
    delegate_->ActivateContents(this);
}

void TabContents::Deactivate() {
  if (delegate_)
    delegate_->DeactivateContents(this);
}

void TabContents::LostCapture() {
  if (delegate_)
    delegate_->LostCapture();
}

bool TabContents::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                         bool* is_keyboard_shortcut) {
  return delegate_ &&
      delegate_->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void TabContents::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (delegate_)
    delegate_->HandleKeyboardEvent(event);
}

void TabContents::HandleMouseDown() {
  if (delegate_)
    delegate_->HandleMouseDown();
}

void TabContents::HandleMouseUp() {
  if (delegate_)
    delegate_->HandleMouseUp();
}

void TabContents::HandleMouseActivate() {
  if (delegate_)
    delegate_->HandleMouseActivate();
}

void TabContents::ToggleFullscreenMode(bool enter_fullscreen) {
  if (delegate_)
    delegate_->ToggleFullscreenModeForTab(this, enter_fullscreen);
}

bool TabContents::IsFullscreenForCurrentTab() const {
  return delegate_ ? delegate_->IsFullscreenForTab(this) : false;
}

void TabContents::RequestToLockMouse() {
  if (delegate_) {
    delegate_->RequestToLockMouse(this);
  } else {
    GotResponseToLockMouseRequest(false);
  }
}

void TabContents::LostMouseLock() {
  if (delegate_)
    delegate_->LostMouseLock();
}

void TabContents::UpdatePreferredSize(const gfx::Size& pref_size) {
  if (delegate_)
    delegate_->UpdatePreferredSize(this, pref_size);
}

void TabContents::WebUISend(RenderViewHost* render_view_host,
                            const GURL& source_url,
                            const std::string& name,
                            const base::ListValue& args) {
  if (delegate_)
    delegate_->WebUISend(this, source_url, name, args);
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

// TODO(adriansc): Remove this method once refactoring changed all call sites.
TabContents* TabContents::OpenURL(const GURL& url,
                                  const GURL& referrer,
                                  WindowOpenDisposition disposition,
                                  content::PageTransition transition) {
  // For specifying a referrer, use the version of OpenURL taking OpenURLParams.
  DCHECK(referrer.is_empty());
  return OpenURL(OpenURLParams(url, content::Referrer(), disposition,
                               transition, false));
}

TabContents* TabContents::OpenURL(const OpenURLParams& params) {
  if (delegate_) {
    TabContents* new_contents = delegate_->OpenURLFromTab(this, params);
    // Notify observers.
    FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                      DidOpenURL(params.url, params.referrer,
                                 params.disposition, params.transition));
    return new_contents;
  }
  return NULL;
}

bool TabContents::NavigateToPendingEntry(
    NavigationController::ReloadType reload_type) {
  return NavigateToEntry(*controller_.pending_entry(), reload_type);
}

bool TabContents::NavigateToEntry(
    const NavigationEntry& entry,
    NavigationController::ReloadType reload_type) {
  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (entry.url().spec().size() > content::kMaxURLChars)
    return false;

  RenderViewHost* dest_render_view_host = render_manager_.Navigate(entry);
  if (!dest_render_view_host)
    return false;  // Unable to create the desired render view host.

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  int enabled_bindings = dest_render_view_host->enabled_bindings();
  bool is_allowed_in_web_ui_renderer = content::GetContentClient()->
      browser()->GetWebUIFactory()->IsURLAcceptableForWebUI(browser_context(),
                                                            entry.url());
#if defined(OS_CHROMEOS)
  is_allowed_in_web_ui_renderer |= entry.url().SchemeIs(chrome::kDataScheme);
#endif
  CHECK(!(enabled_bindings & content::BINDINGS_POLICY_WEB_UI) ||
        is_allowed_in_web_ui_renderer);

  // Tell DevTools agent that it is attached prior to the navigation.
  DevToolsManagerImpl::GetInstance()->OnNavigatingToPendingEntry(
      render_view_host(),
      dest_render_view_host,
      entry.url());

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderViewHost.
  ViewMsg_Navigate_Params navigate_params;
  MakeNavigateParams(entry, controller_, delegate_, reload_type,
                     &navigate_params);
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

  if (delegate_)
    delegate_->DidNavigateToPendingEntry(this);

  return true;
}

void TabContents::SetHistoryLengthAndPrune(const SiteInstance* site_instance,
                                           int history_length,
                                           int32 minimum_page_id) {
  // SetHistoryLengthAndPrune doesn't work when there are pending cross-site
  // navigations. Callers should ensure that this is the case.
  if (render_manager_.pending_render_view_host()) {
    NOTREACHED();
    return;
  }
  RenderViewHost* rvh = render_view_host();
  if (!rvh) {
    NOTREACHED();
    return;
  }
  if (site_instance && rvh->site_instance() != site_instance) {
    NOTREACHED();
    return;
  }
  rvh->Send(new ViewMsg_SetHistoryLengthAndPrune(rvh->routing_id(),
                                                 history_length,
                                                 minimum_page_id));
}

void TabContents::Stop() {
  render_manager_.Stop();
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, StopNavigation());
}

TabContents* TabContents::Clone() {
  // We create a new SiteInstance so that the new tab won't share processes
  // with the old one. This can be changed in the future if we need it to share
  // processes for some reason.
  TabContents* tc = new TabContents(
      browser_context(),
      SiteInstance::CreateSiteInstance(browser_context()),
      MSG_ROUTING_NONE, this, NULL);
  tc->controller().CopyStateFrom(controller_);
  return tc;
}

void TabContents::ShowPageInfo(const GURL& url,
                               const NavigationEntry::SSLStatus& ssl,
                               bool show_history) {
  if (!delegate_)
    return;

  delegate_->ShowPageInfo(browser_context(), url, ssl, show_history);
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

bool TabContents::CanDownload(int request_id) {
  TabContentsDelegate* d = delegate();
  if (d)
    return d->CanDownload(this, request_id);
  return true;
}

void TabContents::OnStartDownload(DownloadItem* download) {
  TabContentsDelegate* d = delegate();
  if (d)
    d->OnStartDownload(this, download);
}

void TabContents::OnSavePage() {
  // If we can not save the page, try to download it.
  if (!SavePackage::IsSavableContents(contents_mime_type())) {
    DownloadManager* dlm = browser_context()->GetDownloadManager();
    const GURL& current_page_url = GetURL();
    if (dlm && current_page_url.is_valid()) {
      dlm->DownloadUrl(current_page_url, GURL(), "", this);
      download_stats::RecordDownloadCount(
          download_stats::INITIATED_BY_SAVE_PACKAGE_FAILURE_COUNT);
      return;
    }
  }

  Stop();

  // Create the save package and possibly prompt the user for the name to save
  // the page as. The user prompt is an asynchronous operation that runs on
  // another thread.
  save_package_ = new SavePackage(this);
  save_package_->GetSaveInfo();
}

// Used in automated testing to bypass prompting the user for file names.
// Instead, the names and paths are hard coded rather than running them through
// file name sanitation and extension / mime checking.
bool TabContents::SavePage(const FilePath& main_file, const FilePath& dir_path,
                           SavePackage::SavePackageType save_type) {
  // Stop the page from navigating.
  Stop();

  save_package_ = new SavePackage(this, save_type, main_file, dir_path);
  return save_package_->Init();
}

void TabContents::OnSaveURL(const GURL& url) {
  DownloadManager* dlm = browser_context()->GetDownloadManager();
  dlm->DownloadUrl(url, GetURL(), "", this);
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

double TabContents::GetZoomLevel() const {
  HostZoomMap* zoom_map = browser_context()->GetHostZoomMap();
  if (!zoom_map)
    return 0;

  double zoom_level;
  if (temporary_zoom_settings_) {
    zoom_level = zoom_map->GetTemporaryZoomLevel(
        GetRenderProcessHost()->GetID(), render_view_host()->routing_id());
  } else {
    GURL url;
    NavigationEntry* active_entry = controller().GetActiveEntry();
    // Since zoom map is updated using rewritten URL, use rewritten URL
    // to get the zoom level.
    url = active_entry ? active_entry->url() : GURL::EmptyGURL();
    zoom_level = zoom_map->GetZoomLevel(net::GetHostOrSpecFromURL(url));
  }
  return zoom_level;
}

int TabContents::GetZoomPercent(bool* enable_increment,
                                bool* enable_decrement) {
  *enable_decrement = *enable_increment = false;
  // Calculate the zoom percent from the factor. Round up to the nearest whole
  // number.
  int percent = static_cast<int>(
      WebKit::WebView::zoomLevelToZoomFactor(GetZoomLevel()) * 100 + 0.5);
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

void TabContents::OnRegisterIntentService(const string16& action,
                                          const string16& type,
                                          const string16& href,
                                          const string16& title,
                                          const string16& disposition) {
  delegate()->RegisterIntentHandler(
      this, action, type, href, title, disposition);
}

void TabContents::OnWebIntentDispatch(const IPC::Message& message,
                                      const webkit_glue::WebIntentData& intent,
                                      int intent_id) {
  IntentsHostImpl* intents_host = new IntentsHostImpl(this, intent, intent_id);
  delegate()->WebIntentDispatch(this, intents_host);
}

void TabContents::OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                                    bool is_main_frame,
                                                    const GURL& opener_url,
                                                    const GURL& url) {
  bool is_error_page = (url.spec() == chrome::kUnreachableWebDataURL);
  GURL validated_url(url);
  render_view_host()->FilterURL(ChildProcessSecurityPolicy::GetInstance(),
      GetRenderProcessHost()->GetID(), &validated_url);

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
                      ProvisionalChangeToMainFrameUrl(url, opener_url));
  }
}

void TabContents::OnDidRedirectProvisionalLoad(int32 page_id,
                                               const GURL& opener_url,
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
                    ProvisionalChangeToMainFrameUrl(target_url,
                                                    opener_url));
}

void TabContents::OnDidFailProvisionalLoadWithError(
    const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  VLOG(1) << "Failed Provisional Load: " << params.url.possibly_invalid_spec()
          << ", error_code: " << params.error_code
          << ", error_description: " << params.error_description
          << ", is_main_frame: " << params.is_main_frame
          << ", showing_repost_interstitial: " <<
            params.showing_repost_interstitial
          << ", frame_id: " << params.frame_id;
  GURL validated_url(params.url);
  render_view_host()->FilterURL(ChildProcessSecurityPolicy::GetInstance(),
      GetRenderProcessHost()->GetID(), &validated_url);

  if (net::ERR_ABORTED == params.error_code) {
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

    // Discard our pending entry if the load canceled (e.g. if we decided to
    // download the file instead of load it).  We do not verify that the URL
    // being canceled matches the pending entry's URL because they will not
    // match if a redirect occurred (in which case we do not want to leave a
    // stale redirect URL showing).  This means that we also cancel the pending
    // entry if the user started a new navigation.  As a result, the navigation
    // controller may not remember that a load is in progress, but the
    // navigation will still commit even if there is no pending entry.
    NavigationEntry* pending_entry = controller_.pending_entry();
    if (pending_entry)
      DidCancelLoading();

    render_manager_.RendererAbortedProvisionalLoad(render_view_host());
  }

  // Send out a notification that we failed a provisional load with an error.
  ProvisionalLoadDetails details(
      params.is_main_frame,
      controller_.IsURLInPageNavigation(validated_url),
      validated_url,
      std::string(),
      false,
      params.frame_id);
  details.set_error_code(params.error_code);

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_FAIL_PROVISIONAL_LOAD_WITH_ERROR,
      content::Source<NavigationController>(&controller_),
      content::Details<ProvisionalLoadDetails>(&details));

  FOR_EACH_OBSERVER(TabContentsObserver,
                    observers_,
                    DidFailProvisionalLoad(params.frame_id,
                                           params.is_main_frame,
                                           validated_url,
                                           params.error_code,
                                           params.error_description));
}

void TabContents::OnDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info,
    const std::string& http_method,
    ResourceType::Type resource_type) {
  base::StatsCounter cache("WebKit.CacheHit");
  cache.Increment();

  // Send out a notification that we loaded a resource from our memory cache.
  int cert_id = 0;
  net::CertStatus cert_status = 0;
  int security_bits = -1;
  int connection_status = 0;
  SSLManager::DeserializeSecurityInfo(security_info,
                                      &cert_id, &cert_status,
                                      &security_bits,
                                      &connection_status);
  LoadFromMemoryCacheDetails details(url, GetRenderProcessHost()->GetID(),
                                     cert_id, cert_status);

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE,
      content::Source<NavigationController>(&controller_),
      content::Details<LoadFromMemoryCacheDetails>(&details));
}

void TabContents::OnDidDisplayInsecureContent() {
  UserMetrics::RecordAction(UserMetricsAction("SSL.DisplayedInsecureContent"));
  displayed_insecure_content_ = true;
  SSLManager::NotifySSLInternalStateChanged(&controller());
}

void TabContents::OnDidRunInsecureContent(
    const std::string& security_origin, const GURL& target_url) {
  LOG(INFO) << security_origin << " ran insecure content from "
            << target_url.possibly_invalid_spec();
  UserMetrics::RecordAction(UserMetricsAction("SSL.RanInsecureContent"));
  if (EndsWith(security_origin, kDotGoogleDotCom, false)) {
    UserMetrics::RecordAction(
        UserMetricsAction("SSL.RanInsecureContentGoogle"));
  }
  controller_.ssl_manager()->DidRunInsecureContent(security_origin);
  displayed_insecure_content_ = true;
  SSLManager::NotifySSLInternalStateChanged(&controller());
}

void TabContents::OnDocumentLoadedInFrame(int64 frame_id) {
  controller_.DocumentLoadedInFrame();
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DocumentLoadedInFrame(frame_id));
}

void TabContents::OnDidFinishLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame) {
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidFinishLoad(frame_id, validated_url, is_main_frame));
}

void TabContents::OnDidFailLoadWithError(int64 frame_id,
                                         const GURL& validated_url,
                                         bool is_main_frame,
                                         int error_code,
                                         const string16& error_description) {
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidFailLoad(frame_id, validated_url, is_main_frame,
                                error_code, error_description));
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
    entry->set_transition_type(
        content::PageTransitionFromInt(
            entry->transition_type() |
            content::PAGE_TRANSITION_FORWARD_BACK));
    NavigateToEntry(*entry, NavigationController::NO_RELOAD);

    // If the entry is being restored and doesn't have a SiteInstance yet, fill
    // it in now that we know. This allows us to find the entry when it commits.
    if (!entry->site_instance() &&
        entry->restore_type() != NavigationEntry::RESTORE_NONE) {
      entry->set_site_instance(GetPendingSiteInstance());
    }
  }
}

void TabContents::OnUpdateZoomLimits(int minimum_percent,
                                     int maximum_percent,
                                     bool remember) {
  minimum_zoom_percent_ = minimum_percent;
  maximum_zoom_percent_ = maximum_percent;
  temporary_zoom_settings_ = !remember;
}

void TabContents::OnEnumerateDirectory(int request_id,
                                       const FilePath& path) {
  delegate()->EnumerateDirectory(this, request_id, path);
}

void TabContents::OnJSOutOfMemory() {
  delegate()->JSOutOfMemory(this);
}

void TabContents::OnRegisterProtocolHandler(const std::string& protocol,
                                            const GURL& url,
                                            const string16& title) {
  delegate()->RegisterProtocolHandler(this, protocol, url, title);
}

void TabContents::OnFindReply(int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update) {
  delegate()->FindReply(this, request_id, number_of_matches, selection_rect,
                        active_match_ordinal, final_update);
  // Send a notification to the renderer that we are ready to receive more
  // results from the scoping effort of the Find operation. The FindInPage
  // scoping is asynchronous and periodically sends results back up to the
  // browser using IPC. In an effort to not spam the browser we have the
  // browser send an ACK for each FindReply message and have the renderer
  // queue up the latest status message while waiting for this ACK.
  render_view_host()->Send(
      new ViewMsg_FindReplyACK(render_view_host()->routing_id()));
}

void TabContents::OnCrashedPlugin(const FilePath& plugin_path) {
  delegate()->CrashedPlugin(this, plugin_path);
}

void TabContents::OnAppCacheAccessed(const GURL& manifest_url,
                                     bool blocked_by_policy) {
  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    AppCacheAccessed(manifest_url, blocked_by_policy));
}

// Notifies the RenderWidgetHost instance about the fact that the page is
// loading, or done loading and calls the base implementation.
void TabContents::SetIsLoading(bool is_loading,
                               LoadNotificationDetails* details) {
  if (is_loading == is_loading_)
    return;

  if (!is_loading) {
    load_state_ = net::LoadStateWithParam(net::LOAD_STATE_IDLE, string16());
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

  int type = is_loading ? content::NOTIFICATION_LOAD_START :
      content::NOTIFICATION_LOAD_STOP;
  content::NotificationDetails det = content::NotificationService::NoDetails();
  if (details)
      det = content::Details<LoadNotificationDetails>(details);
  content::NotificationService::current()->Notify(type,
      content::Source<NavigationController>(&controller_),
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
  return content::WebUIFactory::Get()->GetWebUIType(browser_context(),
                                                    GetURL());
}

void TabContents::DidNavigateMainFramePostCommit(
    const content::LoadCommittedDetails& details,
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

  if (details.is_navigation_to_different_page()) {
    // Clear the status bubble. This is a workaround for a bug where WebKit
    // doesn't let us know that the cursor left an element during a
    // transition (this is also why the mouse cursor remains as a hand after
    // clicking on a link); see bugs 1184641 and 980803. We don't want to
    // clear the bubble when a user navigates to a named anchor in the same
    // page.
    UpdateTargetURL(details.entry->page_id(), GURL());
  }

  if (!details.is_in_page) {
    // Once the main frame is navigated, we're no longer considered to have
    // displayed insecure content.
    displayed_insecure_content_ = false;
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidNavigateMainFrame(details, params));
}

void TabContents::DidNavigateAnyFramePostCommit(
    RenderViewHost* render_view_host,
    const content::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // If we navigate, reset JavaScript state. This does nothing to prevent
  // a malicious script from spamming messages, since the script could just
  // reload the page to stop blocking.
  if (dialog_creator_) {
    dialog_creator_->ResetJavaScriptState(this);
    dialog_creator_ = NULL;
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DidNavigateAnyFrame(details, params));
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

bool TabContents::UpdateTitleForEntry(NavigationEntry* entry,
                                      const string16& title) {
  // For file URLs without a title, use the pathname instead. In the case of a
  // synthesized title, we don't want the update to count toward the "one set
  // per page of the title to history."
  string16 final_title;
  bool explicit_set;
  if (entry && entry->url().SchemeIsFile() && title.empty()) {
    final_title = UTF8ToUTF16(entry->url().ExtractFileName());
    explicit_set = false;  // Don't count synthetic titles toward the set limit.
  } else {
    TrimWhitespace(title, TRIM_ALL, &final_title);
    explicit_set = true;
  }

  // If a page is created via window.open and never navigated,
  // there will be no navigation entry. In this situation,
  // |page_title_when_no_navigaiton_entry_| will be used for page title.
  if (entry) {
    if (final_title == entry->title())
      return false;  // Nothing changed, don't bother.

    entry->set_title(final_title);
  } else {
    if (page_title_when_no_navigation_entry_ == final_title)
      return false;  // Nothing changed, don't bother.

    page_title_when_no_navigation_entry_ = final_title;
  }

  // Lastly, set the title for the view.
  view_->SetPageTitle(final_title);

  TitleUpdatedDetails details(entry, explicit_set);

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_TITLE_UPDATED,
      content::Source<TabContents>(this),
      content::Details<TitleUpdatedDetails>(&details));

  return true;
}

void TabContents::NotifySwapped() {
  // After sending out a swap notification, we need to send a disconnect
  // notification so that clients that pick up a pointer to |this| can NULL the
  // pointer.  See Bug 1230284.
  notify_disconnection_ = true;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_SWAPPED,
      content::Source<TabContents>(this),
      content::NotificationService::NoDetails());
}

void TabContents::NotifyConnected() {
  notify_disconnection_ = true;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_CONNECTED,
      content::Source<TabContents>(this),
      content::NotificationService::NoDetails());
}

void TabContents::NotifyDisconnected() {
  if (!notify_disconnection_)
    return;

  notify_disconnection_ = false;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED,
      content::Source<TabContents>(this),
      content::NotificationService::NoDetails());
}

RenderViewHostDelegate::View* TabContents::GetViewDelegate() {
  return view_.get();
}

RenderViewHostDelegate::RendererManagement*
TabContents::GetRendererManagementDelegate() {
  return &render_manager_;
}

content::RendererPreferences TabContents::GetRendererPrefs(
    content::BrowserContext* browser_context) const {
  return renderer_preferences_;
}

TabContents* TabContents::GetAsTabContents() {
  return this;
}

content::ViewType TabContents::GetRenderViewType() const {
  return view_type_;
}

void TabContents::RenderViewCreated(RenderViewHost* render_view_host) {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
      content::Source<TabContents>(this),
      content::Details<RenderViewHost>(render_view_host));
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

  FOR_EACH_OBSERVER(
      TabContentsObserver, observers_, RenderViewCreated(render_view_host));
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

  FOR_EACH_OBSERVER(TabContentsObserver, observers_, RenderViewReady());
}

void TabContents::RenderViewGone(RenderViewHost* rvh,
                                 base::TerminationStatus status,
                                 int error_code) {
  if (rvh != render_view_host()) {
    // The pending page's RenderViewHost is gone.
    return;
  }

  SetIsLoading(false, NULL);
  NotifyDisconnected();
  SetIsCrashed(status, error_code);
  view()->OnTabCrashed(crashed_status(), crashed_error_code());

  FOR_EACH_OBSERVER(TabContentsObserver,
                    observers_,
                    RenderViewGone(crashed_status()));
}

void TabContents::RenderViewDeleted(RenderViewHost* rvh) {
  render_manager_.RenderViewDeleted(rvh);
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, RenderViewDeleted(rvh));
}

void TabContents::DidNavigate(RenderViewHost* rvh,
                              const ViewHostMsg_FrameNavigate_Params& params) {
  if (content::PageTransitionIsMainFrame(params.transition))
    render_manager_.DidNavigateMainFrame(rvh);

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
  if (content::PageTransitionIsMainFrame(params.transition))
    contents_mime_type_ = params.contents_mime_type;

  content::LoadCommittedDetails details;
  bool did_navigate = controller_.RendererDidNavigate(params, &details);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != content::NAVIGATION_TYPE_NAV_IGNORE) {
    // For AUTO_SUBFRAME navigations, an event for the main frame is generated
    // that is not recorded in the navigation history. For the purpose of
    // tracking navigation events, we treat this event as a sub frame navigation
    // event.
    bool is_main_frame = did_navigate ? details.is_main_frame : false;
    content::PageTransition transition_type = params.transition;
    // Whether or not a page transition was triggered by going backward or
    // forward in the history is only stored in the navigation controller's
    // entry list.
    if (did_navigate &&
        (controller_.GetActiveEntry()->transition_type() &
            content::PAGE_TRANSITION_FORWARD_BACK)) {
      transition_type = content::PageTransitionFromInt(
          params.transition | content::PAGE_TRANSITION_FORWARD_BACK);
    }
    // Notify observers about the commit of the provisional load.
    FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                      DidCommitProvisionalLoadForFrame(params.frame_id,
                      is_main_frame, params.url, transition_type));
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // Run post-commit tasks.
  if (details.is_main_frame) {
    DidNavigateMainFramePostCommit(details, params);
    if (delegate())
      delegate()->DidNavigateMainFramePostCommit(this);
  }
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
                              int32 page_id,
                              const string16& title,
                              base::i18n::TextDirection title_direction) {
  // If we have a title, that's a pretty good indication that we've started
  // getting useful data.
  SetNotWaitingForResponse();

  DCHECK(rvh == render_view_host());
  NavigationEntry* entry = controller_.GetEntryWithPageID(rvh->site_instance(),
                                                          page_id);

  // TODO(evan): make use of title_direction.
  // http://code.google.com/p/chromium/issues/detail?id=27094
  if (!UpdateTitleForEntry(entry, title))
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
    delegate()->UpdateTargetURL(this, page_id, url);
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

void TabContents::SwappedOut(RenderViewHost* rvh) {
  if (delegate() && rvh == render_view_host())
    delegate()->SwappedOut(this);
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

void TabContents::DocumentAvailableInMainFrame(
    RenderViewHost* render_view_host) {
  FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                    DocumentAvailableInMainFrame());
}

void TabContents::DocumentOnLoadCompletedInMainFrame(
    RenderViewHost* render_view_host,
    int32 page_id) {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<TabContents>(this),
      content::Details<int>(&page_id));
}

void TabContents::RequestOpenURL(const GURL& url,
                                 const content::Referrer& referrer,
                                 WindowOpenDisposition disposition,
                                 int64 source_frame_id) {
  // Delegate to RequestTransferURL because this is just the generic
  // case where |old_request_id| is empty.
  RequestTransferURL(url, referrer, disposition, source_frame_id,
                     GlobalRequestID());
}

void TabContents::RequestTransferURL(const GURL& url,
                                     const content::Referrer& referrer,
                                     WindowOpenDisposition disposition,
                                     int64 source_frame_id,
                                     const GlobalRequestID& old_request_id) {
  TabContents* new_contents = NULL;
  content::PageTransition transition_type = content::PAGE_TRANSITION_LINK;
  if (render_manager_.web_ui()) {
    // When we're a Web UI, it will provide a page transition type for us (this
    // is so the new tab page can specify AUTO_BOOKMARK for automatically
    // generated suggestions).
    //
    // Note also that we hide the referrer for Web UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    OpenURLParams params(url, content::Referrer(), disposition,
        render_manager_.web_ui()->link_transition_type(),
        false /* is_renderer_initiated */);
    params.transferred_global_request_id = old_request_id;
    new_contents = OpenURL(params);
    transition_type = render_manager_.web_ui()->link_transition_type();
  } else {
    OpenURLParams params(url, referrer, disposition,
        content::PAGE_TRANSITION_LINK, true /* is_renderer_initiated */);
    params.transferred_global_request_id = old_request_id;
    new_contents = OpenURL(params);
  }
  if (new_contents) {
    // Notify observers.
    FOR_EACH_OBSERVER(TabContentsObserver, observers_,
                      DidOpenRequestedURL(new_contents,
                                          url,
                                          referrer,
                                          disposition,
                                          transition_type,
                                          source_frame_id));
  }
}

void TabContents::RunJavaScriptMessage(
    const RenderViewHost* rvh,
    const string16& message,
    const string16& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Suppress JavaScript dialogs when requested. Also suppress messages when
  // showing an interstitial as it's shown over the previous page and we don't
  // want the hidden page's dialogs to interfere with the interstitial.
  bool suppress_this_message =
      rvh->is_swapped_out() ||
      showing_interstitial_page() ||
      !delegate_ ||
      delegate_->ShouldSuppressDialogs();

  if (!suppress_this_message) {
    content::JavaScriptDialogCreator::TitleType title_type;
    string16 title;

    if (!frame_url.has_host()) {
      title_type = content::JavaScriptDialogCreator::DIALOG_TITLE_NONE;
    } else {
      title_type = content::JavaScriptDialogCreator::DIALOG_TITLE_FORMATTED_URL;
      title = net::FormatUrl(
          frame_url.GetOrigin(),
          content::GetContentClient()->browser()->GetAcceptLangs(
              this->browser_context()));
    }

    dialog_creator_ = delegate_->GetJavaScriptDialogCreator();
    dialog_creator_->RunJavaScriptDialog(this,
                                         title_type,
                                         title,
                                         flags,
                                         message,
                                         default_prompt,
                                         reply_msg,
                                         &suppress_this_message);
  }

  if (suppress_this_message) {
    // If we are suppressing messages, just reply as if the user immediately
    // pressed "Cancel".
    OnDialogClosed(reply_msg, false, string16());
  }

  *did_suppress_message = suppress_this_message;
}

void TabContents::RunBeforeUnloadConfirm(const RenderViewHost* rvh,
                                         const string16& message,
                                         IPC::Message* reply_msg) {
  if (delegate())
    delegate()->WillRunBeforeUnloadConfirm();

  bool suppress_this_message =
      rvh->is_swapped_out() ||
      !delegate_ ||
      delegate_->ShouldSuppressDialogs();
  if (suppress_this_message) {
    render_view_host()->JavaScriptDialogClosed(reply_msg, true, string16());
    return;
  }

  is_showing_before_unload_dialog_ = true;
  dialog_creator_ = delegate_->GetJavaScriptDialogCreator();
  dialog_creator_->RunBeforeUnloadDialog(this,
                                         message,
                                         reply_msg);
}

WebPreferences TabContents::GetWebkitPrefs() {
  WebPreferences web_prefs =
      content::GetContentClient()->browser()->GetWebkitPrefs(
          render_view_host());

  // Force accelerated compositing and 2d canvas off for chrome:, about: and
  // chrome-devtools: pages (unless it's specifically allowed).
  if ((GetURL().SchemeIs(chrome::kChromeDevToolsScheme) ||
      // Allow accelerated compositing for keyboard and log in screen.
      GetURL().SchemeIs(chrome::kChromeUIScheme) ||
      (GetURL().SchemeIs(chrome::kAboutScheme) &&
       GetURL().spec() != chrome::kAboutBlankURL)) &&
      !web_prefs.allow_webui_compositing) {
    web_prefs.accelerated_compositing_enabled = false;
    web_prefs.accelerated_2d_canvas_enabled = false;
  }

  return web_prefs;
}

void TabContents::OnUserGesture() {
  // Notify observers.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, DidGetUserGesture());

  ResourceDispatcherHost* rdh =
      content::GetContentClient()->browser()->GetResourceDispatcherHost();
  if (rdh)  // NULL in unittests.
    rdh->OnUserGesture(this);
}

void TabContents::OnIgnoredUIEvent() {
  // Notify observers.
  FOR_EACH_OBSERVER(TabContentsObserver, observers_, DidGetIgnoredUIEvent());
}

void TabContents::RendererUnresponsive(RenderViewHost* rvh,
                                       bool is_during_unload) {
  // Don't show hung renderer dialog for a swapped out RVH.
  if (rvh != render_view_host())
    return;

  // Ignore renderer unresponsive event if debugger is attached to the tab
  // since the event may be a result of the renderer sitting on a breakpoint.
  // See http://crbug.com/65458
  DevToolsAgentHost* agent =
      content::DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh);
  if (agent &&
      DevToolsManagerImpl::GetInstance()->GetDevToolsClientHostFor(agent))
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

  if (!render_view_host() || !render_view_host()->IsRenderViewLive())
    return;

  if (delegate())
    delegate()->RendererUnresponsive(this);
}

void TabContents::RendererResponsive(RenderViewHost* render_view_host) {
  if (delegate())
    delegate()->RendererResponsive(this);
}

void TabContents::LoadStateChanged(const GURL& url,
                                   const net::LoadStateWithParam& load_state,
                                   uint64 upload_position,
                                   uint64 upload_size) {
  load_state_ = load_state;
  upload_position_ = upload_position;
  upload_size_ = upload_size;
  load_state_host_ = net::IDNToUnicode(url.host(),
      content::GetContentClient()->browser()->GetAcceptLangs(
          this->browser_context()));
  if (load_state_.state == net::LOAD_STATE_READING_RESPONSE)
    SetNotWaitingForResponse();
  if (IsLoading())
    NotifyNavigationStateChanged(INVALIDATE_LOAD | INVALIDATE_TAB);
}

void TabContents::WorkerCrashed() {
  if (delegate())
    delegate()->WorkerCrashed(this);
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
  // Can be NULL during tests.
  RenderWidgetHostView* rwh_view = view_->CreateViewForWidget(render_view_host);

  // Now that the RenderView has been created, we need to tell it its size.
  if (rwh_view)
    rwh_view->SetSize(view_->GetContainerSize());

  if (!render_view_host->CreateRenderView(string16()))
    return false;

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Force a ViewMsg_Resize to be sent, needed to make plugins show up on
  // linux. See crbug.com/83941.
  if (rwh_view) {
    if (RenderWidgetHost* render_widget_host = rwh_view->GetRenderWidgetHost())
      render_widget_host->WasResized();
  }
#endif

  UpdateMaxPageIDIfNecessary(render_view_host->site_instance(),
                             render_view_host);
  return true;
}

void TabContents::OnDialogClosed(IPC::Message* reply_msg,
                                 bool success,
                                 const string16& user_input) {
  if (is_showing_before_unload_dialog_ && !success) {
    // If a beforeunload dialog is canceled, we need to stop the throbber from
    // spinning, since we forced it to start spinning in Navigate.
    DidStopLoading();

    tab_close_start_time_ = base::TimeTicks();
  }
  is_showing_before_unload_dialog_ = false;
  render_view_host()->JavaScriptDialogClosed(reply_msg,
                                             success,
                                             user_input);
}

gfx::NativeWindow TabContents::GetDialogRootWindow() const {
  return view_->GetTopLevelNativeWindow();
}

void TabContents::OnDialogShown() {
  Activate();
}

void TabContents::set_encoding(const std::string& encoding) {
  encoding_ = content::GetContentClient()->browser()->
      GetCanonicalEncodingNameByAliasName(encoding);
}

void TabContents::CreateViewAndSetSizeForRVH(RenderViewHost* rvh) {
  RenderWidgetHostView* rwh_view = view()->CreateViewForWidget(rvh);
  // Can be NULL during tests.
  if (rwh_view)
    rwh_view->SetSize(view()->GetContainerSize());
}

bool TabContents::GotResponseToLockMouseRequest(bool allowed) {
  return render_view_host() ?
      render_view_host()->GotResponseToLockMouseRequest(allowed) : false;
}
