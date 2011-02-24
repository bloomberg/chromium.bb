// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_contents.h"

#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/view_types.h"
#include "content/browser/browsing_instance.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mach_broker_mac.h"
#endif

namespace prerender {

class PrerenderContentsFactoryImpl : public PrerenderContents::Factory {
 public:
  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
      const std::vector<GURL>& alias_urls, const GURL& referrer) {
    return new PrerenderContents(prerender_manager, profile, url, alias_urls,
                                 referrer);
  }
};

PrerenderContents::PrerenderContents(PrerenderManager* prerender_manager,
                                     Profile* profile,
                                     const GURL& url,
                                     const std::vector<GURL>& alias_urls,
                                     const GURL& referrer)
    : prerender_manager_(prerender_manager),
      render_view_host_(NULL),
      prerender_url_(url),
      referrer_(referrer),
      profile_(profile),
      page_id_(0),
      has_stopped_loading_(false),
      final_status_(FINAL_STATUS_MAX) {
  DCHECK(prerender_manager != NULL);
  if (!AddAliasURL(prerender_url_))
    LOG(DFATAL) << "PrerenderContents given invalid URL " << prerender_url_;
  for (std::vector<GURL>::const_iterator it = alias_urls.begin();
       it != alias_urls.end();
       ++it) {
    if (!AddAliasURL(*it))
      LOG(DFATAL) << "PrerenderContents given invalid URL " << prerender_url_;
  }
}

// static
PrerenderContents::Factory* PrerenderContents::CreateFactory() {
  return new PrerenderContentsFactoryImpl();
}

void PrerenderContents::StartPrerendering() {
  DCHECK(profile_ != NULL);
  SiteInstance* site_instance = SiteInstance::CreateSiteInstance(profile_);
  render_view_host_ = new RenderViewHost(site_instance, this, MSG_ROUTING_NONE,
                                         NULL);
  // Hide the RVH, so that we will run at a lower CPU priority.
  // Once the RVH is being swapped into a tab, we will Restore it again.
  render_view_host_->WasHidden();
  render_view_host_->AllowScriptToClose(true);

  // Close ourselves when the application is shutting down.
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());

  // Register for our parent profile to shutdown, so we can shut ourselves down
  // as well (should only be called for OTR profiles, as we should receive
  // APP_TERMINATING before non-OTR profiles are destroyed).
  // TODO(tburkard): figure out if this is needed.
  registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                 Source<Profile>(profile_));
  render_view_host_->CreateRenderView(string16());

  // Register to cancel if Authentication is required.
  registrar_.Add(this, NotificationType::AUTH_NEEDED,
                 NotificationService::AllSources());

  registrar_.Add(this, NotificationType::AUTH_CANCELLED,
                 NotificationService::AllSources());

  // Register all responses to see if we should cancel.
  registrar_.Add(this, NotificationType::DOWNLOAD_INITIATED,
                 NotificationService::AllSources());

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  ViewMsg_Navigate_Params params;
  params.url = prerender_url_;
  params.transition = PageTransition::LINK;
  params.navigation_type = ViewMsg_Navigate_Params::PRERENDER;
  params.referrer = referrer_;
  render_view_host_->Navigate(params);
}

void PrerenderContents::set_final_status(FinalStatus final_status) {
  DCHECK(final_status >= FINAL_STATUS_USED && final_status < FINAL_STATUS_MAX);
  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);

  final_status_ = final_status;
}

FinalStatus PrerenderContents::final_status() const {
  return final_status_;
}

PrerenderContents::~PrerenderContents() {
  DCHECK(final_status_ != FINAL_STATUS_MAX);
  RecordFinalStatus(final_status_);

  if (!render_view_host_)   // Will be null for unit tests.
    return;

  render_view_host_->Shutdown();  // deletes render_view_host
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
  // We only care when the outer frame changes.
  if (!PageTransition::IsMainFrame(params.transition))
    return;

  // Store the navigation params.
  ViewHostMsg_FrameNavigate_Params* p = new ViewHostMsg_FrameNavigate_Params();
  *p = params;
  navigate_params_.reset(p);

  if (!AddAliasURL(params.url)) {
    Destroy(FINAL_STATUS_HTTPS);
    return;
  }

  url_ = params.url;
}

void PrerenderContents::UpdateTitle(RenderViewHost* render_view_host,
                                    int32 page_id,
                                    const std::wstring& title) {
  if (title.empty()) {
    return;
  }

  title_ = WideToUTF16Hack(title);
  page_id_ = page_id;
}

void PrerenderContents::RunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Always suppress JavaScript messages if they're triggered by a page being
  // prerendered.
  *did_suppress_message = true;
  // We still want to show the user the message when they navigate to this
  // page, so cancel this prerender.
  Destroy(FINAL_STATUS_JAVASCRIPT_ALERT);
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
      // Prerendered pages have a NULL controller and the login handler should
      // be referencing us as the render view host delegate.
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      LoginNotificationDetails* details_ptr =
          Details<LoginNotificationDetails>(details).ptr();
      LoginHandler* handler = details_ptr->handler();
      DCHECK(handler != NULL);
      RenderViewHostDelegate* delegate = handler->GetRenderViewHostDelegate();
      if (controller == NULL && delegate == this) {
        Destroy(FINAL_STATUS_AUTH_NEEDED);
        return;
      }
      break;
    }

    case NotificationType::DOWNLOAD_INITIATED: {
      // If the download is started from a RenderViewHost that we are
      // delegating, kill the prerender. This cancels any pending requests
      // though the download never actually started thanks to the
      // DownloadRequestLimiter.
      DCHECK(NotificationService::NoDetails() == details);
      RenderViewHost* rvh = Source<RenderViewHost>(source).ptr();
      CHECK(rvh != NULL);
      if (rvh->delegate() == this) {
        Destroy(FINAL_STATUS_DOWNLOAD);
        return;
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

void PrerenderContents::ProcessWebUIMessage(
    const ViewHostMsg_DomMessage_Params& params) {
  render_view_host_->BlockExtensionRequest(params.request_id);
}

void PrerenderContents::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
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

bool PrerenderContents::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PrerenderContents, message, message_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidRedirectProvisionalLoad,
                        OnDidRedirectProvisionalLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateFavIconURL, OnUpdateFavIconURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void PrerenderContents::OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                                          bool is_main_frame,
                                                          const GURL& url) {
  if (is_main_frame) {
    if (!AddAliasURL(url)) {
      Destroy(FINAL_STATUS_HTTPS);
      return;
    }
  }
}

void PrerenderContents::OnDidRedirectProvisionalLoad(int32 page_id,
                                                     const GURL& source_url,
                                                     const GURL& target_url) {
  if (!AddAliasURL(target_url))
    Destroy(FINAL_STATUS_HTTPS);
}

void PrerenderContents::OnUpdateFavIconURL(int32 page_id,
                                           const GURL& icon_url) {
  icon_url_ = icon_url;
}

bool PrerenderContents::AddAliasURL(const GURL& url) {
  if (!url.SchemeIs("http"))
    return false;
  alias_urls_.push_back(url);
  return true;
}

bool PrerenderContents::MatchesURL(const GURL& url) const {
  return std::find(alias_urls_.begin(), alias_urls_.end(), url)
      != alias_urls_.end();
}

void PrerenderContents::DidStopLoading() {
  has_stopped_loading_ = true;
}

void PrerenderContents::Destroy(FinalStatus final_status) {
  prerender_manager_->RemoveEntry(this);
  set_final_status(final_status);
  delete this;
}

void PrerenderContents::OnJSOutOfMemory() {
  Destroy(FINAL_STATUS_JS_OUT_OF_MEMORY);
}

void PrerenderContents::RendererUnresponsive(RenderViewHost* render_view_host,
                                             bool is_during_unload) {
  Destroy(FINAL_STATUS_RENDERER_UNRESPONSIVE);
}


base::ProcessMetrics* PrerenderContents::MaybeGetProcessMetrics() {
  if (process_metrics_.get() == NULL) {
    base::ProcessHandle handle = render_view_host_->process()->GetHandle();
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
    if (private_bytes > kMaxPrerenderPrivateMB * 1024 * 1024)
      Destroy(FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);
  }
}

}  // namespace prerender
