// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_view/web_view_internal_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/url_constants.h"

using base::UserMetricsAction;
using content::RenderFrameHost;
using content::ResourceType;
using content::WebContents;

namespace extensions {

namespace {

std::string WindowOpenDispositionToString(
  WindowOpenDisposition window_open_disposition) {
  switch (window_open_disposition) {
    case IGNORE_ACTION:
      return "ignore";
    case SAVE_TO_DISK:
      return "save_to_disk";
    case CURRENT_TAB:
      return "current_tab";
    case NEW_BACKGROUND_TAB:
      return "new_background_tab";
    case NEW_FOREGROUND_TAB:
      return "new_foreground_tab";
    case NEW_WINDOW:
      return "new_window";
    case NEW_POPUP:
      return "new_popup";
    default:
      NOTREACHED() << "Unknown Window Open Disposition";
      return "ignore";
  }
}

static std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "abnormal";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "crashed";
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

std::string GetStoragePartitionIdFromSiteURL(const GURL& site_url) {
  const std::string& partition_id = site_url.query();
  bool persist_storage = site_url.path().find("persist") != std::string::npos;
  return (persist_storage ? webview::kPersistPrefix : "") + partition_id;
}

void ParsePartitionParam(const base::DictionaryValue& create_params,
                         std::string* storage_partition_id,
                         bool* persist_storage) {
  std::string partition_str;
  if (!create_params.GetString(webview::kStoragePartitionId, &partition_str)) {
    return;
  }

  // Since the "persist:" prefix is in ASCII, StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (StartsWithASCII(partition_str, "persist:", true)) {
    size_t index = partition_str.find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    *storage_partition_id = partition_str.substr(index + 1);

    if (storage_partition_id->empty()) {
      // TODO(lazyboy): Better way to deal with this error.
      return;
    }
    *persist_storage = true;
  } else {
    *storage_partition_id = partition_str;
    *persist_storage = false;
  }
}

void RemoveWebViewEventListenersOnIOThread(
    void* profile,
    const std::string& extension_id,
    int embedder_process_id,
    int view_instance_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  ExtensionWebRequestEventRouter::GetInstance()->RemoveWebViewEventListeners(
      profile,
      extension_id,
      embedder_process_id,
      view_instance_id);
}

}  // namespace

// static
GuestViewBase* WebViewGuest::Create(content::WebContents* owner_web_contents) {
  return new WebViewGuest(owner_web_contents);
}

// static
bool WebViewGuest::GetGuestPartitionConfigForSite(
    const GURL& site,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  if (!site.SchemeIs(content::kGuestScheme))
    return false;

  // Since guest URLs are only used for packaged apps, there must be an app
  // id in the URL.
  CHECK(site.has_host());
  *partition_domain = site.host();
  // Since persistence is optional, the path must either be empty or the
  // literal string.
  *in_memory = (site.path() != "/persist");
  // The partition name is user supplied value, which we have encoded when the
  // URL was created, so it needs to be decoded.
  *partition_name =
      net::UnescapeURLComponent(site.query(), net::UnescapeRule::NORMAL);
  return true;
}

// static
const char WebViewGuest::Type[] = "webview";

using WebViewKey = std::pair<int, int>;
using WebViewKeyToIDMap = std::map<WebViewKey, int>;
static base::LazyInstance<WebViewKeyToIDMap> web_view_key_to_id_map =
    LAZY_INSTANCE_INITIALIZER;

// static
int WebViewGuest::GetOrGenerateRulesRegistryID(
    int embedder_process_id,
    int webview_instance_id) {
  bool is_web_view = embedder_process_id && webview_instance_id;
  if (!is_web_view)
    return RulesRegistryService::kDefaultRulesRegistryID;

  WebViewKey key = std::make_pair(embedder_process_id, webview_instance_id);
  auto it = web_view_key_to_id_map.Get().find(key);
  if (it != web_view_key_to_id_map.Get().end())
    return it->second;

  auto rph = content::RenderProcessHost::FromID(embedder_process_id);
  int rules_registry_id =
      RulesRegistryService::Get(rph->GetBrowserContext())->
          GetNextRulesRegistryID();
  web_view_key_to_id_map.Get()[key] = rules_registry_id;
  return rules_registry_id;
}

// static
int WebViewGuest::GetViewInstanceId(WebContents* contents) {
  auto guest = FromWebContents(contents);
  if (!guest)
    return guestview::kInstanceIDNone;

  return guest->view_instance_id();
}

bool WebViewGuest::CanRunInDetachedState() const {
  return true;
}

void WebViewGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  content::RenderProcessHost* owner_render_process_host =
      owner_web_contents()->GetRenderProcessHost();
  std::string storage_partition_id;
  bool persist_storage = false;
  ParsePartitionParam(create_params, &storage_partition_id, &persist_storage);
  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!base::IsStringUTF8(storage_partition_id)) {
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    owner_render_process_host->Shutdown(content::RESULT_CODE_KILLED_BAD_MESSAGE,
                                        false);
    callback.Run(nullptr);
    return;
  }
  std::string url_encoded_partition = net::EscapeQueryParamValue(
      storage_partition_id, false);
  std::string partition_domain = GetOwnerSiteURL().host();
  GURL guest_site(base::StringPrintf("%s://%s/%s?%s",
                                     content::kGuestScheme,
                                     partition_domain.c_str(),
                                     persist_storage ? "persist" : "",
                                     url_encoded_partition.c_str()));

  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  auto guest_view_manager = GuestViewManager::FromBrowserContext(
      owner_render_process_host->GetBrowserContext());
  content::SiteInstance* guest_site_instance =
      guest_view_manager->GetGuestSiteInstance(guest_site);
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that webview tags are also not allowed to send messages across
    // different partitions.
    guest_site_instance = content::SiteInstance::CreateForURL(
        owner_render_process_host->GetBrowserContext(), guest_site);
  }
  WebContents::CreateParams params(
      owner_render_process_host->GetBrowserContext(),
      guest_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void WebViewGuest::DidAttachToEmbedder() {
  ApplyAttributes(*attach_params());
}

void WebViewGuest::DidDropLink(const GURL& url) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventDropLink, args.Pass()));
}

void WebViewGuest::DidInitialize(const base::DictionaryValue& create_params) {
  script_executor_.reset(
      new ScriptExecutor(web_contents(), &script_observers_));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                              content::Source<WebContents>(web_contents()));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                              content::Source<WebContents>(web_contents()));

  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnDidInitialize();
  AttachWebViewHelpers(web_contents());

  rules_registry_id_ = GetOrGenerateRulesRegistryID(
      owner_web_contents()->GetRenderProcessHost()->GetID(),
      view_instance_id());

  // We must install the mapping from guests to WebViews prior to resuming
  // suspended resource loads so that the WebRequest API will catch resource
  // requests.
  PushWebViewStateToIOThread();

  ApplyAttributes(create_params);
}

void WebViewGuest::AttachWebViewHelpers(WebContents* contents) {
  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnAttachWebViewHelpers(contents);
  web_view_permission_helper_.reset(new WebViewPermissionHelper(this));
}

void WebViewGuest::DidStopLoading() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventLoadStop, args.Pass()));
}

void WebViewGuest::EmbedderWillBeDestroyed() {
  // Clean up rules registries for the webview.
  RulesRegistryService::Get(browser_context())
      ->RemoveRulesRegistriesByID(rules_registry_id_);
  WebViewKey key(owner_web_contents()->GetRenderProcessHost()->GetID(),
                 view_instance_id());
  web_view_key_to_id_map.Get().erase(key);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          browser_context(),
          owner_extension_id(),
          owner_web_contents()->GetRenderProcessHost()->GetID(),
          view_instance_id()));
}

const char* WebViewGuest::GetAPINamespace() const {
  return webview::kAPINamespace;
}

int WebViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_WEBVIEW_TAG_PREFIX;
}

void WebViewGuest::GuestDestroyed() {
  // Clean up custom context menu items for this guest.
  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnGuestDestroyed();
  RemoveWebViewStateFromIOThread(web_contents());
}

void WebViewGuest::GuestReady() {
  // The guest RenderView should always live in an isolated guest process.
  CHECK(web_contents()->GetRenderProcessHost()->IsIsolatedGuest());
  Send(new ExtensionMsg_SetFrameName(web_contents()->GetRoutingID(), name_));

  // We don't want to accidentally set the opacity of an interstitial page.
  // WebContents::GetRenderWidgetHostView will return the RWHV of an
  // interstitial page if one is showing at this time. We only want opacity
  // to apply to web pages.
  if (guest_opaque_) {
    web_contents()
        ->GetRenderViewHost()
        ->GetView()
        ->SetBackgroundColorToDefault();
  } else {
    web_contents()->GetRenderViewHost()->GetView()->SetBackgroundColor(
        SK_ColorTRANSPARENT);
  }
}

void WebViewGuest::GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                                 const gfx::Size& new_size) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kOldHeight, old_size.height());
  args->SetInteger(webview::kOldWidth, old_size.width());
  args->SetInteger(webview::kNewHeight, new_size.height());
  args->SetInteger(webview::kNewWidth, new_size.width());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventSizeChanged, args.Pass()));
}

bool WebViewGuest::IsAutoSizeSupported() const {
  return true;
}

bool WebViewGuest::IsDragAndDropEnabled() const {
  return true;
}

void WebViewGuest::WillDestroy() {
  if (!attached() && GetOpener())
    GetOpener()->pending_new_windows_.erase(this);
}

bool WebViewGuest::AddMessageToConsole(WebContents* source,
                                       int32 level,
                                       const base::string16& message,
                                       int32 line_no,
                                       const base::string16& source_id) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  // Log levels are from base/logging.h: LogSeverity.
  args->SetInteger(webview::kLevel, level);
  args->SetString(webview::kMessage, message);
  args->SetInteger(webview::kLine, line_no);
  args->SetString(webview::kSourceId, source_id);
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventConsoleMessage, args.Pass()));
  return true;
}

void WebViewGuest::CloseContents(WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventClose, args.Pass()));
}

void WebViewGuest::FindReply(WebContents* source,
                             int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) {
  find_helper_.FindReply(request_id,
                         number_of_matches,
                         selection_rect,
                         active_match_ordinal,
                         final_update);
}

bool WebViewGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (!web_view_guest_delegate_)
    return false;
  return web_view_guest_delegate_->HandleContextMenu(params);
}

void WebViewGuest::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (HandleKeyboardShortcuts(event))
    return;

  GuestViewBase::HandleKeyboardEvent(source, event);
}

bool WebViewGuest::PreHandleGestureEvent(content::WebContents* source,
                                         const blink::WebGestureEvent& event) {
  return !allow_scaling_ && GuestViewBase::PreHandleGestureEvent(source, event);
}

void WebViewGuest::LoadProgressChanged(content::WebContents* source,
                                       double progress) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventLoadProgress, args.Pass()));
}

void WebViewGuest::LoadAbort(bool is_top_level,
                             const GURL& url,
                             const std::string& error_type) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(guestview::kUrl, url.possibly_invalid_spec());
  args->SetString(guestview::kReason, error_type);
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventLoadAbort, args.Pass()));
}

void WebViewGuest::CreateNewGuestWebViewWindow(
    const content::OpenURLParams& params) {
  GuestViewManager* guest_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  // Set the attach params to use the same partition as the opener.
  // We pull the partition information from the site's URL, which is of the
  // form guest://site/{persist}?{partition_name}.
  const GURL& site_url = web_contents()->GetSiteInstance()->GetSiteURL();
  const std::string storage_partition_id =
      GetStoragePartitionIdFromSiteURL(site_url);
  base::DictionaryValue create_params;
  create_params.SetString(webview::kStoragePartitionId, storage_partition_id);

  guest_manager->CreateGuest(WebViewGuest::Type,
                             embedder_web_contents(),
                             create_params,
                             base::Bind(&WebViewGuest::NewGuestWebViewCallback,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        params));
}

void WebViewGuest::NewGuestWebViewCallback(
    const content::OpenURLParams& params,
    content::WebContents* guest_web_contents) {
  WebViewGuest* new_guest = WebViewGuest::FromWebContents(guest_web_contents);
  new_guest->SetOpener(this);

  // Take ownership of |new_guest|.
  pending_new_windows_.insert(
      std::make_pair(new_guest, NewWindowInfo(params.url, std::string())));

  // Request permission to show the new window.
  RequestNewWindowPermission(params.disposition,
                             gfx::Rect(),
                             params.user_gesture,
                             new_guest->web_contents());
}

// TODO(fsamuel): Find a reliable way to test the 'responsive' and
// 'unresponsive' events.
void WebViewGuest::RendererResponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventResponsive, args.Pass()));
}

void WebViewGuest::RendererUnresponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventUnresponsive, args.Pass()));
}

void WebViewGuest::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(), web_contents());
      if (content::Source<WebContents>(source).ptr() == web_contents())
        LoadHandlerCalled();
      break;
    }
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(), web_contents());
      content::ResourceRedirectDetails* resource_redirect_details =
          content::Details<content::ResourceRedirectDetails>(details).ptr();
      bool is_top_level = resource_redirect_details->resource_type ==
                          content::RESOURCE_TYPE_MAIN_FRAME;
      LoadRedirect(resource_redirect_details->url,
                   resource_redirect_details->new_url,
                   is_top_level);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void WebViewGuest::StartFindInternal(
    const base::string16& search_text,
    const blink::WebFindOptions& options,
    scoped_refptr<WebViewInternalFindFunction> find_function) {
  find_helper_.Find(web_contents(), search_text, options, find_function);
}

void WebViewGuest::StopFindingInternal(content::StopFindAction action) {
  find_helper_.CancelAllFindSessions();
  web_contents()->StopFinding(action);
}

bool WebViewGuest::Go(int relative_index) {
  content::NavigationController& controller = web_contents()->GetController();
  if (!controller.CanGoToOffset(relative_index))
    return false;

  controller.GoToOffset(relative_index);
  return true;
}

void WebViewGuest::Reload() {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  web_contents()->GetController().Reload(false);
}

void WebViewGuest::SetUserAgentOverride(
    const std::string& user_agent_override) {
  is_overriding_user_agent_ = !user_agent_override.empty();
  if (is_overriding_user_agent_) {
    content::RecordAction(UserMetricsAction("WebView.Guest.OverrideUA"));
  }
  web_contents()->SetUserAgentOverride(user_agent_override);
}

void WebViewGuest::Stop() {
  web_contents()->Stop();
}

void WebViewGuest::Terminate() {
  content::RecordAction(UserMetricsAction("WebView.Guest.Terminate"));
  base::ProcessHandle process_handle =
      web_contents()->GetRenderProcessHost()->GetHandle();
  if (process_handle)
    web_contents()->GetRenderProcessHost()->Shutdown(
        content::RESULT_CODE_KILLED, false);
}

bool WebViewGuest::ClearData(const base::Time remove_since,
                             uint32 removal_mask,
                             const base::Closure& callback) {
  content::RecordAction(UserMetricsAction("WebView.Guest.ClearData"));
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents()->GetBrowserContext(),
          web_contents()->GetSiteInstance());

  if (!partition)
    return false;

  partition->ClearData(
      removal_mask,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(),
      content::StoragePartition::OriginMatcherFunction(),
      remove_since,
      base::Time::Now(),
      callback);
  return true;
}

WebViewGuest::WebViewGuest(content::WebContents* owner_web_contents)
    : GuestView<WebViewGuest>(owner_web_contents),
      rules_registry_id_(RulesRegistryService::kInvalidRulesRegistryID),
      find_helper_(this),
      is_overriding_user_agent_(false),
      guest_opaque_(true),
      javascript_dialog_helper_(this),
      current_zoom_factor_(1.0),
      allow_scaling_(false),
      weak_ptr_factory_(this) {
  web_view_guest_delegate_.reset(
      ExtensionsAPIClient::Get()->CreateWebViewGuestDelegate(this));
}

WebViewGuest::~WebViewGuest() {
}

void WebViewGuest::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (!render_frame_host->GetParent())
    src_ = url;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, !render_frame_host->GetParent());
  args->SetString(webview::kInternalBaseURLForDataURL,
                  web_contents()
                      ->GetController()
                      .GetLastCommittedEntry()
                      ->GetBaseURLForDataURL()
                      .spec());
  args->SetInteger(webview::kInternalCurrentEntryIndex,
                   web_contents()->GetController().GetCurrentEntryIndex());
  args->SetInteger(webview::kInternalEntryCount,
                   web_contents()->GetController().GetEntryCount());
  args->SetInteger(webview::kInternalProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventLoadCommit, args.Pass()));

  find_helper_.CancelAllFindSessions();

  // Update the current zoom factor for the new page.
  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents());
  DCHECK(zoom_controller);
  current_zoom_factor_ =
      content::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());

  if (web_view_guest_delegate_) {
    web_view_guest_delegate_->OnDidCommitProvisionalLoadForFrame(
        !render_frame_host->GetParent());
  }
}

void WebViewGuest::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  LoadAbort(!render_frame_host->GetParent(), validated_url,
            net::ErrorToShortString(error_code));
}

void WebViewGuest::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, validated_url.spec());
  args->SetBoolean(guestview::kIsTopLevel, !render_frame_host->GetParent());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventLoadStart, args.Pass()));
}

void WebViewGuest::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnDocumentLoadedInFrame(render_frame_host);
}

void WebViewGuest::RenderProcessGone(base::TerminationStatus status) {
  // Cancel all find sessions in progress.
  find_helper_.CancelAllFindSessions();

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  args->SetString(webview::kReason, TerminationStatusToString(status));
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventExit, args.Pass()));
}

void WebViewGuest::UserAgentOverrideSet(const std::string& user_agent) {
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry)
    return;
  entry->SetIsOverridingUserAgent(!user_agent.empty());
  web_contents()->GetController().Reload(false);
}

void WebViewGuest::FrameNameChanged(RenderFrameHost* render_frame_host,
                                    const std::string& name) {
  if (render_frame_host->GetParent())
    return;

  if (name_ == name)
    return;

  ReportFrameNameChange(name);
}

void WebViewGuest::ReportFrameNameChange(const std::string& name) {
  name_ = name;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kName, name);
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventFrameNameChanged, args.Pass()));
}

void WebViewGuest::LoadHandlerCalled() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventContentLoad, args.Pass()));
}

void WebViewGuest::LoadRedirect(const GURL& old_url,
                                const GURL& new_url,
                                bool is_top_level) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(webview::kNewURL, new_url.spec());
  args->SetString(webview::kOldURL, old_url.spec());
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventLoadRedirect, args.Pass()));
}

void WebViewGuest::PushWebViewStateToIOThread() {
  const GURL& site_url = web_contents()->GetSiteInstance()->GetSiteURL();
  std::string partition_domain;
  std::string partition_id;
  bool in_memory;
  if (!GetGuestPartitionConfigForSite(
          site_url, &partition_domain, &partition_id, &in_memory)) {
    NOTREACHED();
    return;
  }

  WebViewRendererState::WebViewInfo web_view_info;
  web_view_info.embedder_process_id =
      owner_web_contents()->GetRenderProcessHost()->GetID();
  web_view_info.instance_id = view_instance_id();
  web_view_info.partition_id = partition_id;
  web_view_info.owner_extension_id = owner_extension_id();
  web_view_info.rules_registry_id = rules_registry_id_;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebViewRendererState::AddGuest,
                 base::Unretained(WebViewRendererState::GetInstance()),
                 web_contents()->GetRenderProcessHost()->GetID(),
                 web_contents()->GetRoutingID(),
                 web_view_info));
}

// static
void WebViewGuest::RemoveWebViewStateFromIOThread(
    WebContents* web_contents) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &WebViewRendererState::RemoveGuest,
          base::Unretained(WebViewRendererState::GetInstance()),
          web_contents->GetRenderProcessHost()->GetID(),
          web_contents->GetRoutingID()));
}

void WebViewGuest::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  web_view_permission_helper_->RequestMediaAccessPermission(source,
                                                            request,
                                                            callback);
}

bool WebViewGuest::CheckMediaAccessPermission(content::WebContents* source,
                                              const GURL& security_origin,
                                              content::MediaStreamType type) {
  return web_view_permission_helper_->CheckMediaAccessPermission(
      source, security_origin, type);
}

void WebViewGuest::CanDownload(
    content::RenderViewHost* render_view_host,
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  web_view_permission_helper_->CanDownload(render_view_host,
                                           url,
                                           request_method,
                                           callback);
}

void WebViewGuest::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    const base::Callback<void(bool)>& callback) {
  web_view_permission_helper_->RequestPointerLockPermission(
      user_gesture,
      last_unlocked_by_target,
      callback);
}

void WebViewGuest::WillAttachToEmbedder() {
  rules_registry_id_ = GetOrGenerateRulesRegistryID(
      owner_web_contents()->GetRenderProcessHost()->GetID(),
      view_instance_id());

  // We must install the mapping from guests to WebViews prior to resuming
  // suspended resource loads so that the WebRequest API will catch resource
  // requests.
  PushWebViewStateToIOThread();
}

content::JavaScriptDialogManager* WebViewGuest::GetJavaScriptDialogManager(
    WebContents* source) {
  return &javascript_dialog_helper_;
}

void WebViewGuest::NavigateGuest(const std::string& src,
                                 bool force_navigation) {
  if (src.empty())
    return;

  GURL url = ResolveURL(src);

  LoadURLWithParams(url, content::Referrer(),
                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                    force_navigation);
}

bool WebViewGuest::HandleKeyboardShortcuts(
    const content::NativeWebKeyboardEvent& event) {
  // <webview> outside of Chrome Apps do not handle keyboard shortcuts.
  if (!in_extension())
    return false;

  if (event.type != blink::WebInputEvent::RawKeyDown)
    return false;

  // If the user hits the escape key without any modifiers then unlock the
  // mouse if necessary.
  if ((event.windowsKeyCode == ui::VKEY_ESCAPE) &&
      !(event.modifiers & blink::WebInputEvent::InputModifiers)) {
    return web_contents()->GotResponseToLockMouseRequest(false);
  }

#if defined(OS_MACOSX)
  if (event.modifiers != blink::WebInputEvent::MetaKey)
    return false;

  if (event.windowsKeyCode == ui::VKEY_OEM_4) {
    Go(-1);
    return true;
  }

  if (event.windowsKeyCode == ui::VKEY_OEM_6) {
    Go(1);
    return true;
  }
#else
  if (event.windowsKeyCode == ui::VKEY_BROWSER_BACK) {
    Go(-1);
    return true;
  }

  if (event.windowsKeyCode == ui::VKEY_BROWSER_FORWARD) {
    Go(1);
    return true;
  }
#endif

  return false;
}

void WebViewGuest::ApplyAttributes(const base::DictionaryValue& params) {
  std::string name;
  if (params.GetString(webview::kAttributeName, &name)) {
    // If the guest window's name is empty, then the WebView tag's name is
    // assigned. Otherwise, the guest window's name takes precedence over the
    // WebView tag's name.
    if (name_.empty())
      SetName(name);
  }
  if (attached())
    ReportFrameNameChange(name_);

  std::string user_agent_override;
  params.GetString(webview::kParameterUserAgentOverride, &user_agent_override);
  SetUserAgentOverride(user_agent_override);

  bool allow_transparency = false;
  params.GetBoolean(webview::kAttributeAllowTransparency, &allow_transparency);
  // We need to set the background opaque flag after navigation to ensure that
  // there is a RenderWidgetHostView available.
  SetAllowTransparency(allow_transparency);

  bool allow_scaling = false;
  params.GetBoolean(webview::kAttributeAllowScaling, &allow_scaling);
  SetAllowScaling(allow_scaling);

  bool is_pending_new_window = false;
  if (GetOpener()) {
    // We need to do a navigation here if the target URL has changed between
    // the time the WebContents was created and the time it was attached.
    // We also need to do an initial navigation if a RenderView was never
    // created for the new window in cases where there is no referrer.
    auto it = GetOpener()->pending_new_windows_.find(this);
    if (it != GetOpener()->pending_new_windows_.end()) {
      const NewWindowInfo& new_window_info = it->second;
      if (new_window_info.changed || !web_contents()->HasOpener())
        NavigateGuest(new_window_info.url.spec(), false /* force_navigation */);

      // Once a new guest is attached to the DOM of the embedder page, then the
      // lifetime of the new guest is no longer managed by the opener guest.
      GetOpener()->pending_new_windows_.erase(this);

      is_pending_new_window = true;
    }
  }

  // Only read the src attribute if this is not a New Window API flow.
  if (!is_pending_new_window) {
    std::string src;
    params.GetString(webview::kAttributeSrc, &src);
    NavigateGuest(src, false /* force_navigation */);
  }
}

void WebViewGuest::ShowContextMenu(
    int request_id,
    const WebViewGuestDelegate::MenuItemVector* items) {
  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnShowContextMenu(request_id, items);
}

void WebViewGuest::SetName(const std::string& name) {
  if (name_ == name)
    return;
  name_ = name;

  Send(new ExtensionMsg_SetFrameName(routing_id(), name_));
}

void WebViewGuest::SetZoom(double zoom_factor) {
  auto zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents());
  DCHECK(zoom_controller);
  double zoom_level = content::ZoomFactorToZoomLevel(zoom_factor);
  zoom_controller->SetZoomLevel(zoom_level);

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetDouble(webview::kOldZoomFactor, current_zoom_factor_);
  args->SetDouble(webview::kNewZoomFactor, zoom_factor);
  DispatchEventToView(
      new GuestViewBase::Event(webview::kEventZoomChange, args.Pass()));
  current_zoom_factor_ = zoom_factor;
}

void WebViewGuest::SetAllowTransparency(bool allow) {
  if (guest_opaque_ != allow)
    return;

  guest_opaque_ = !allow;
  if (!web_contents()->GetRenderViewHost()->GetView())
    return;

  if (guest_opaque_) {
    web_contents()
        ->GetRenderViewHost()
        ->GetView()
        ->SetBackgroundColorToDefault();
  } else {
    web_contents()->GetRenderViewHost()->GetView()->SetBackgroundColor(
        SK_ColorTRANSPARENT);
  }
}

void WebViewGuest::SetAllowScaling(bool allow) {
  allow_scaling_ = allow;
}

bool WebViewGuest::LoadDataWithBaseURL(const std::string& data_url,
                                       const std::string& base_url,
                                       const std::string& virtual_url,
                                       std::string* error) {
  // Make GURLs from URLs.
  const GURL data_gurl = GURL(data_url);
  const GURL base_gurl = GURL(base_url);
  const GURL virtual_gurl = GURL(virtual_url);

  // Check that the provided URLs are valid.
  // |data_url| must be a valid data URL.
  if (!data_gurl.is_valid() || !data_gurl.SchemeIs(url::kDataScheme)) {
    base::SStringPrintf(
        error, webview::kAPILoadDataInvalidDataURL, data_url.c_str());
    return false;
  }
  // |base_url| must be a valid URL.
  if (!base_gurl.is_valid()) {
    base::SStringPrintf(
        error, webview::kAPILoadDataInvalidBaseURL, base_url.c_str());
    return false;
  }
  // |virtual_url| must be a valid URL.
  if (!virtual_gurl.is_valid()) {
    base::SStringPrintf(
        error, webview::kAPILoadDataInvalidVirtualURL, virtual_url.c_str());
    return false;
  }

  // Set up the parameters to load |data_url| with the specified |base_url|.
  content::NavigationController::LoadURLParams load_params(data_gurl);
  load_params.load_type = content::NavigationController::LOAD_TYPE_DATA;
  load_params.base_url_for_data_url = base_gurl;
  load_params.virtual_url_for_data_url = virtual_gurl;
  load_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_INHERIT;

  // Navigate to the data URL.
  GuestViewBase::LoadURLWithParams(load_params);

  return true;
}

void WebViewGuest::AddNewContents(content::WebContents* source,
                                  content::WebContents* new_contents,
                                  WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_rect,
                                  bool user_gesture,
                                  bool* was_blocked) {
  if (was_blocked)
    *was_blocked = false;
  RequestNewWindowPermission(disposition,
                             initial_rect,
                             user_gesture,
                             new_contents);
}

content::WebContents* WebViewGuest::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // There are two use cases to consider from a security perspective:
  // 1.) Renderer-initiated navigation to chrome:// must always be blocked even
  //     if the <webview> is in WebUI. This is handled by
  //     WebViewGuest::LoadURLWithParams. WebViewGuest::NavigateGuest will also
  //     call LoadURLWithParams. CreateNewGuestWebViewWindow creates a new
  //     WebViewGuest which will call NavigateGuest in DidInitialize.
  // 2.) The Language Settings context menu item should always work, both in
  //     Chrome Apps and WebUI. This is a browser initiated request and so
  //     we pass it along to the embedder's WebContentsDelegate to get the
  //     browser to perform the action for the <webview>.
  if (!params.is_renderer_initiated) {
    if (!owner_web_contents()->GetDelegate())
      return nullptr;
    return owner_web_contents()->GetDelegate()->OpenURLFromTab(
        owner_web_contents(), params);
  }

  // If the guest wishes to navigate away prior to attachment then we save the
  // navigation to perform upon attachment. Navigation initializes a lot of
  // state that assumes an embedder exists, such as RenderWidgetHostViewGuest.
  // Navigation also resumes resource loading which we don't want to allow
  // until attachment.
  if (!attached()) {
    WebViewGuest* opener = GetOpener();
    auto it = opener->pending_new_windows_.find(this);
    if (it == opener->pending_new_windows_.end())
      return nullptr;
    const NewWindowInfo& info = it->second;
    NewWindowInfo new_window_info(params.url, info.name);
    new_window_info.changed = new_window_info.url != info.url;
    it->second = new_window_info;
    return nullptr;
  }

  // This code path is taken if RenderFrameImpl::DecidePolicyForNavigation
  // decides that a fork should happen. At the time of writing this comment,
  // the only way a well behaving guest could hit this code path is if it
  // navigates to a URL that's associated with the default search engine.
  // This list of URLs is generated by chrome::GetSearchURLs. Validity checks
  // are performed inside LoadURLWithParams such that if the guest attempts
  // to navigate to a URL that it is not allowed to navigate to, a 'loadabort'
  // event will fire in the embedder, and the guest will be navigated to
  // about:blank.
  if (params.disposition == CURRENT_TAB) {
    LoadURLWithParams(params.url, params.referrer, params.transition,
                      true /* force_navigation */);
    return web_contents();
  }

  // This code path is taken if Ctrl+Click, middle click or any of the
  // keyboard/mouse combinations are used to open a link in a new tab/window.
  // This code path is also taken on client-side redirects from about:blank.
  CreateNewGuestWebViewWindow(params);
  return nullptr;
}

void WebViewGuest::WebContentsCreated(WebContents* source_contents,
                                      int opener_render_frame_id,
                                      const base::string16& frame_name,
                                      const GURL& target_url,
                                      content::WebContents* new_contents) {
  auto guest = WebViewGuest::FromWebContents(new_contents);
  CHECK(guest);
  guest->SetOpener(this);
  std::string guest_name = base::UTF16ToUTF8(frame_name);
  guest->name_ = guest_name;
  pending_new_windows_.insert(
      std::make_pair(guest, NewWindowInfo(target_url, guest_name)));
}

void WebViewGuest::LoadURLWithParams(const GURL& url,
                                     const content::Referrer& referrer,
                                     ui::PageTransition transition_type,
                                     bool force_navigation) {
  // Do not allow navigating a guest to schemes other than known safe schemes.
  // This will block the embedder trying to load unwanted schemes, e.g.
  // chrome://.
  bool scheme_is_blocked =
      (!content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
           url.scheme()) &&
       !url.SchemeIs(url::kAboutScheme)) ||
      url.SchemeIs(url::kJavaScriptScheme);
  if (scheme_is_blocked || !url.is_valid()) {
    LoadAbort(true /* is_top_level */, url,
              net::ErrorToShortString(net::ERR_ABORTED));
    NavigateGuest(url::kAboutBlankURL, true /* force_navigation */);
    return;
  }

  if (!force_navigation && (src_ == url))
    return;

  GURL validated_url(url);
  web_contents()->GetRenderProcessHost()->FilterURL(false, &validated_url);
  // As guests do not swap processes on navigation, only navigations to
  // normal web URLs are supported.  No protocol handlers are installed for
  // other schemes (e.g., WebUI or extensions), and no permissions or bindings
  // can be granted to the guest process.
  content::NavigationController::LoadURLParams load_url_params(validated_url);
  load_url_params.referrer = referrer;
  load_url_params.transition_type = transition_type;
  load_url_params.extra_headers = std::string();
  if (is_overriding_user_agent_) {
    load_url_params.override_user_agent =
        content::NavigationController::UA_OVERRIDE_TRUE;
  }
  GuestViewBase::LoadURLWithParams(load_url_params);

  src_ = validated_url;
}

void WebViewGuest::RequestNewWindowPermission(
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_bounds,
    bool user_gesture,
    content::WebContents* new_contents) {
  auto guest = WebViewGuest::FromWebContents(new_contents);
  if (!guest)
    return;
  auto it = pending_new_windows_.find(guest);
  if (it == pending_new_windows_.end())
    return;
  const NewWindowInfo& new_window_info = it->second;

  // Retrieve the opener partition info if we have it.
  const GURL& site_url = new_contents->GetSiteInstance()->GetSiteURL();
  std::string storage_partition_id = GetStoragePartitionIdFromSiteURL(site_url);

  base::DictionaryValue request_info;
  request_info.SetInteger(webview::kInitialHeight, initial_bounds.height());
  request_info.SetInteger(webview::kInitialWidth, initial_bounds.width());
  request_info.Set(webview::kTargetURL,
                   new base::StringValue(new_window_info.url.spec()));
  request_info.Set(webview::kName, new base::StringValue(new_window_info.name));
  request_info.SetInteger(webview::kWindowID, guest->guest_instance_id());
  // We pass in partition info so that window-s created through newwindow
  // API can use it to set their partition attribute.
  request_info.Set(webview::kStoragePartitionId,
                   new base::StringValue(storage_partition_id));
  request_info.Set(
      webview::kWindowOpenDisposition,
      new base::StringValue(WindowOpenDispositionToString(disposition)));

  web_view_permission_helper_->
      RequestPermission(WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW,
                        request_info,
                        base::Bind(&WebViewGuest::OnWebViewNewWindowResponse,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   guest->guest_instance_id()),
                                   false /* allowed_by_default */);
}

GURL WebViewGuest::ResolveURL(const std::string& src) {
  if (!in_extension())
    return GURL(src);

  GURL default_url(base::StringPrintf("%s://%s/",
                                      kExtensionScheme,
                                      owner_extension_id().c_str()));
  return default_url.Resolve(src);
}

void WebViewGuest::OnWebViewNewWindowResponse(
    int new_window_instance_id,
    bool allow,
    const std::string& user_input) {
  auto guest =
      WebViewGuest::From(owner_web_contents()->GetRenderProcessHost()->GetID(),
                         new_window_instance_id);
  if (!guest)
    return;

  if (!allow)
    guest->Destroy();
}

}  // namespace extensions
