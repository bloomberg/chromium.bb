// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/web_view_guest.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/api/web_view/web_view_internal_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/guest_view/guest_view_manager.h"
#include "chrome/browser/guest_view/web_view/web_view_constants.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_types.h"
#include "chrome/browser/guest_view/web_view/web_view_renderer_state.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/pdf/pdf_tab_helper.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/render_messages.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
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
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_constants.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/base/models/simple_menu_model.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#endif

using base::UserMetricsAction;
using content::RenderFrameHost;
using content::ResourceType;
using content::WebContents;

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

}  // namespace

// static
GuestViewBase* WebViewGuest::Create(content::BrowserContext* browser_context,
                                    int guest_instance_id) {
  return new WebViewGuest(browser_context, guest_instance_id);
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

// static
int WebViewGuest::GetViewInstanceId(WebContents* contents) {
  WebViewGuest* guest = FromWebContents(contents);
  if (!guest)
    return guestview::kInstanceIDNone;

  return guest->view_instance_id();
}

// static
scoped_ptr<base::ListValue> WebViewGuest::MenuModelToValue(
    const ui::SimpleMenuModel& menu_model) {
  scoped_ptr<base::ListValue> items(new base::ListValue());
  for (int i = 0; i < menu_model.GetItemCount(); ++i) {
    base::DictionaryValue* item_value = new base::DictionaryValue();
    // TODO(lazyboy): We need to expose some kind of enum equivalent of
    // |command_id| instead of plain integers.
    item_value->SetInteger(webview::kMenuItemCommandId,
                           menu_model.GetCommandIdAt(i));
    item_value->SetString(webview::kMenuItemLabel, menu_model.GetLabelAt(i));
    items->Append(item_value);
  }
  return items.Pass();
}

const char* WebViewGuest::GetAPINamespace() {
  return webview::kAPINamespace;
}

void WebViewGuest::CreateWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  content::RenderProcessHost* embedder_render_process_host =
      content::RenderProcessHost::FromID(embedder_render_process_id);
  std::string storage_partition_id;
  bool persist_storage = false;
  std::string storage_partition_string;
  ParsePartitionParam(create_params, &storage_partition_id, &persist_storage);
  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!base::IsStringUTF8(storage_partition_id)) {
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        embedder_render_process_host->GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    callback.Run(NULL);
    return;
  }
  std::string url_encoded_partition = net::EscapeQueryParamValue(
      storage_partition_id, false);
  // The SiteInstance of a given webview tag is based on the fact that it's
  // a guest process in addition to which platform application the tag
  // belongs to and what storage partition is in use, rather than the URL
  // that the tag is being navigated to.
  GURL guest_site(base::StringPrintf("%s://%s/%s?%s",
                                     content::kGuestScheme,
                                     embedder_extension_id.c_str(),
                                     persist_storage ? "persist" : "",
                                     url_encoded_partition.c_str()));

  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(
          embedder_render_process_host->GetBrowserContext());
  content::SiteInstance* guest_site_instance =
      guest_view_manager->GetGuestSiteInstance(guest_site);
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that webview tags are also not allowed to send messages across
    // different partitions.
    guest_site_instance = content::SiteInstance::CreateForURL(
        embedder_render_process_host->GetBrowserContext(), guest_site);
  }
  WebContents::CreateParams params(
      embedder_render_process_host->GetBrowserContext(),
      guest_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void WebViewGuest::DidAttachToEmbedder() {
  SetUpAutoSize();

  std::string name;
  if (extra_params()->GetString(webview::kName, &name)) {
    // If the guest window's name is empty, then the WebView tag's name is
    // assigned. Otherwise, the guest window's name takes precedence over the
    // WebView tag's name.
    if (name_.empty())
      name_ = name;
  }
  ReportFrameNameChange(name_);

  std::string user_agent_override;
  if (extra_params()->GetString(webview::kParameterUserAgentOverride,
                                &user_agent_override)) {
    SetUserAgentOverride(user_agent_override);
  } else {
    SetUserAgentOverride("");
  }

  std::string src;
  if (extra_params()->GetString("src", &src) && !src.empty())
    NavigateGuest(src);

  if (GetOpener()) {
    // We need to do a navigation here if the target URL has changed between
    // the time the WebContents was created and the time it was attached.
    // We also need to do an initial navigation if a RenderView was never
    // created for the new window in cases where there is no referrer.
    PendingWindowMap::iterator it =
        GetOpener()->pending_new_windows_.find(this);
    if (it != GetOpener()->pending_new_windows_.end()) {
      const NewWindowInfo& new_window_info = it->second;
      if (new_window_info.changed || !guest_web_contents()->HasOpener())
        NavigateGuest(new_window_info.url.spec());
    } else {
      NOTREACHED();
    }

    // Once a new guest is attached to the DOM of the embedder page, then the
    // lifetime of the new guest is no longer managed by the opener guest.
    GetOpener()->pending_new_windows_.erase(this);
  }
}

void WebViewGuest::DidInitialize() {
  script_executor_.reset(new extensions::ScriptExecutor(guest_web_contents(),
                                                        &script_observers_));

  notification_registrar_.Add(
      this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<WebContents>(guest_web_contents()));

  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<WebContents>(guest_web_contents()));

#if defined(OS_CHROMEOS)
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&WebViewGuest::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
#endif

  AttachWebViewHelpers(guest_web_contents());
}

void WebViewGuest::AttachWebViewHelpers(WebContents* contents) {
  // Create a zoom controller for the guest contents give it access to
  // GetZoomLevel() and and SetZoomLevel() in WebViewGuest.
  // TODO(wjmaclean) This currently uses the same HostZoomMap as the browser
  // context, but we eventually want to isolate the guest contents from zoom
  // changes outside the guest (e.g. in the main browser), so we should
  // create a separate HostZoomMap for the guest.
  ZoomController::CreateForWebContents(contents);

  FaviconTabHelper::CreateForWebContents(contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)
  PDFTabHelper::CreateForWebContents(contents);
  web_view_permission_helper_.reset(new WebViewPermissionHelper(this));
}

void WebViewGuest::DidStopLoading() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventLoadStop, args.Pass()));
}

void WebViewGuest::EmbedderDestroyed() {
  // TODO(fsamuel): WebRequest event listeners for <webview> should survive
  // reparenting of a <webview> within a single embedder. Right now, we keep
  // around the browser state for the listener for the lifetime of the embedder.
  // Ideally, the lifetime of the listeners should match the lifetime of the
  // <webview> DOM node. Once http://crbug.com/156219 is resolved we can move
  // the call to RemoveWebViewEventListenersOnIOThread back to
  // WebViewGuest::WebContentsDestroyed.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          browser_context(), embedder_extension_id(),
          embedder_render_process_id(),
          view_instance_id()));
}

void WebViewGuest::GuestDestroyed() {
  // Clean up custom context menu items for this guest.
  extensions::MenuManager* menu_manager = extensions::MenuManager::Get(
      Profile::FromBrowserContext(browser_context()));
  menu_manager->RemoveAllContextItems(extensions::MenuItem::ExtensionKey(
      embedder_extension_id(), view_instance_id()));

  RemoveWebViewStateFromIOThread(web_contents());
}

void WebViewGuest::GuestReady() {
  // The guest RenderView should always live in an isolated guest process.
  CHECK(guest_web_contents()->GetRenderProcessHost()->IsIsolatedGuest());
  Send(new ChromeViewMsg_SetName(guest_web_contents()->GetRoutingID(), name_));
}

void WebViewGuest::GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                                 const gfx::Size& new_size) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kOldHeight, old_size.height());
  args->SetInteger(webview::kOldWidth, old_size.width());
  args->SetInteger(webview::kNewHeight, new_size.height());
  args->SetInteger(webview::kNewWidth, new_size.width());
  DispatchEventToEmbedder(
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
  DestroyUnattachedWindows();
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
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventConsoleMessage, args.Pass()));
  return true;
}

void WebViewGuest::CloseContents(WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventClose, args.Pass()));
}

void WebViewGuest::FindReply(WebContents* source,
                             int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) {
  find_helper_.FindReply(request_id, number_of_matches, selection_rect,
                         active_match_ordinal, final_update);
}

bool WebViewGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  DCHECK(menu_delegate);

  pending_menu_ = menu_delegate->BuildMenu(guest_web_contents(), params);

  // Pass it to embedder.
  int request_id = ++pending_context_menu_request_id_;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  scoped_ptr<base::ListValue> items =
      MenuModelToValue(pending_menu_->menu_model());
  args->Set(webview::kContextMenuItems, items.release());
  args->SetInteger(webview::kRequestId, request_id);
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventContextMenu, args.Pass()));
  return true;
}

void WebViewGuest::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (!attached())
    return;

  if (HandleKeyboardShortcuts(event))
    return;

  // Send the unhandled keyboard events back to the embedder to reprocess them.
  // TODO(fsamuel): This introduces the possibility of out-of-order keyboard
  // events because the guest may be arbitrarily delayed when responding to
  // keyboard events. In that time, the embedder may have received and processed
  // additional key events. This needs to be fixed as soon as possible.
  // See http://crbug.com/229882.
  embedder_web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents(), event);
}

void WebViewGuest::LoadProgressChanged(content::WebContents* source,
                                       double progress) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, guest_web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventLoadProgress, args.Pass()));
}

void WebViewGuest::LoadAbort(bool is_top_level,
                             const GURL& url,
                             const std::string& error_type) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(guestview::kUrl, url.possibly_invalid_spec());
  args->SetString(guestview::kReason, error_type);
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventLoadAbort, args.Pass()));
}

void WebViewGuest::OnUpdateFrameName(bool is_top_level,
                                     const std::string& name) {
  if (!is_top_level)
    return;

  if (name_ == name)
    return;

  ReportFrameNameChange(name);
}

void WebViewGuest::CreateNewGuestWebViewWindow(
    const content::OpenURLParams& params) {
  GuestViewManager* guest_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  // Set the attach params to use the same partition as the opener.
  // We pull the partition information from the site's URL, which is of the
  // form guest://site/{persist}?{partition_name}.
  const GURL& site_url = guest_web_contents()->GetSiteInstance()->GetSiteURL();
  const std::string storage_partition_id =
      GetStoragePartitionIdFromSiteURL(site_url);
  base::DictionaryValue create_params;
  create_params.SetString(webview::kStoragePartitionId, storage_partition_id);

  guest_manager->CreateGuest(WebViewGuest::Type,
                             embedder_extension_id(),
                             embedder_web_contents(),
                             create_params,
                             base::Bind(&WebViewGuest::NewGuestWebViewCallback,
                                        base::Unretained(this),
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
  RequestNewWindowPermission(params.disposition, gfx::Rect(),
                             params.user_gesture,
                             new_guest->guest_web_contents());
}

// TODO(fsamuel): Find a reliable way to test the 'responsive' and
// 'unresponsive' events.
void WebViewGuest::RendererResponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventResponsive, args.Pass()));
}

void WebViewGuest::RendererUnresponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventUnresponsive, args.Pass()));
}

void WebViewGuest::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                guest_web_contents());
      if (content::Source<WebContents>(source).ptr() == guest_web_contents())
        LoadHandlerCalled();
      break;
    }
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                guest_web_contents());
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

double WebViewGuest::GetZoom() {
  return current_zoom_factor_;
}

void WebViewGuest::Find(
    const base::string16& search_text,
    const blink::WebFindOptions& options,
    scoped_refptr<extensions::WebViewInternalFindFunction> find_function) {
  find_helper_.Find(guest_web_contents(), search_text, options, find_function);
}

void WebViewGuest::StopFinding(content::StopFindAction action) {
  find_helper_.CancelAllFindSessions();
  guest_web_contents()->StopFinding(action);
}

void WebViewGuest::Go(int relative_index) {
  guest_web_contents()->GetController().GoToOffset(relative_index);
}

void WebViewGuest::Reload() {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  guest_web_contents()->GetController().Reload(false);
}

void WebViewGuest::SetUserAgentOverride(
    const std::string& user_agent_override) {
  if (!attached())
    return;
  is_overriding_user_agent_ = !user_agent_override.empty();
  if (is_overriding_user_agent_) {
    content::RecordAction(UserMetricsAction("WebView.Guest.OverrideUA"));
  }
  guest_web_contents()->SetUserAgentOverride(user_agent_override);
}

void WebViewGuest::Stop() {
  guest_web_contents()->Stop();
}

void WebViewGuest::Terminate() {
  content::RecordAction(UserMetricsAction("WebView.Guest.Terminate"));
  base::ProcessHandle process_handle =
      guest_web_contents()->GetRenderProcessHost()->GetHandle();
  if (process_handle)
    base::KillProcess(process_handle, content::RESULT_CODE_KILLED, false);
}

bool WebViewGuest::ClearData(const base::Time remove_since,
                             uint32 removal_mask,
                             const base::Closure& callback) {
  content::RecordAction(UserMetricsAction("WebView.Guest.ClearData"));
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          guest_web_contents()->GetBrowserContext(),
          guest_web_contents()->GetSiteInstance());

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

WebViewGuest::WebViewGuest(content::BrowserContext* browser_context,
                           int guest_instance_id)
    : GuestView<WebViewGuest>(browser_context, guest_instance_id),
      pending_context_menu_request_id_(0),
      is_overriding_user_agent_(false),
      chromevox_injected_(false),
      current_zoom_factor_(1.0),
      find_helper_(this),
      javascript_dialog_helper_(this) {
}

WebViewGuest::~WebViewGuest() {
}

void WebViewGuest::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    content::PageTransition transition_type) {
  find_helper_.CancelAllFindSessions();

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, !render_frame_host->GetParent());
  args->SetInteger(webview::kInternalCurrentEntryIndex,
      guest_web_contents()->GetController().GetCurrentEntryIndex());
  args->SetInteger(webview::kInternalEntryCount,
      guest_web_contents()->GetController().GetEntryCount());
  args->SetInteger(webview::kInternalProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventLoadCommit, args.Pass()));

  // Update the current zoom factor for the new page.
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(guest_web_contents());
  DCHECK(zoom_controller);
  current_zoom_factor_ = zoom_controller->GetZoomLevel();

  if (!render_frame_host->GetParent())
    chromevox_injected_ = false;
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
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventLoadStart, args.Pass()));
}

void WebViewGuest::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent())
    InjectChromeVoxIfNeeded(render_frame_host->GetRenderViewHost());
}

bool WebViewGuest::OnMessageReceived(const IPC::Message& message,
                                     RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebViewGuest, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_UpdateFrameName, OnUpdateFrameName)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebViewGuest::RenderProcessGone(base::TerminationStatus status) {
  // Cancel all find sessions in progress.
  find_helper_.CancelAllFindSessions();

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   guest_web_contents()->GetRenderProcessHost()->GetID());
  args->SetString(webview::kReason, TerminationStatusToString(status));
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventExit, args.Pass()));
}

void WebViewGuest::UserAgentOverrideSet(const std::string& user_agent) {
  if (!attached())
    return;
  content::NavigationController& controller =
      guest_web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry)
    return;
  entry->SetIsOverridingUserAgent(!user_agent.empty());
  guest_web_contents()->GetController().Reload(false);
}

void WebViewGuest::ReportFrameNameChange(const std::string& name) {
  name_ = name;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kName, name);
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventFrameNameChanged, args.Pass()));
}

void WebViewGuest::LoadHandlerCalled() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventContentLoad, args.Pass()));
}

void WebViewGuest::LoadRedirect(const GURL& old_url,
                                const GURL& new_url,
                                bool is_top_level) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(webview::kNewURL, new_url.spec());
  args->SetString(webview::kOldURL, old_url.spec());
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventLoadRedirect, args.Pass()));
}

void WebViewGuest::PushWebViewStateToIOThread() {
  const GURL& site_url = guest_web_contents()->GetSiteInstance()->GetSiteURL();
  std::string partition_domain;
  std::string partition_id;
  bool in_memory;
  if (!GetGuestPartitionConfigForSite(
          site_url, &partition_domain, &partition_id, &in_memory)) {
    NOTREACHED();
    return;
  }
  DCHECK(embedder_extension_id() == partition_domain);

  WebViewRendererState::WebViewInfo web_view_info;
  web_view_info.embedder_process_id = embedder_render_process_id();
  web_view_info.instance_id = view_instance_id();
  web_view_info.partition_id = partition_id;
  web_view_info.embedder_extension_id = embedder_extension_id();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebViewRendererState::AddGuest,
                 base::Unretained(WebViewRendererState::GetInstance()),
                 guest_web_contents()->GetRenderProcessHost()->GetID(),
                 guest_web_contents()->GetRoutingID(),
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

content::WebContents* WebViewGuest::CreateNewGuestWindow(
    const content::WebContents::CreateParams& create_params) {
  GuestViewManager* guest_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  return guest_manager->CreateGuestWithWebContentsParams(
      WebViewGuest::Type,
      embedder_extension_id(),
      embedder_web_contents()->GetRenderProcessHost()->GetID(),
      create_params);
}

void WebViewGuest::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  web_view_permission_helper_->RequestMediaAccessPermission(source,
                                                            request,
                                                            callback);
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
  // We must install the mapping from guests to WebViews prior to resuming
  // suspended resource loads so that the WebRequest API will catch resource
  // requests.
  PushWebViewStateToIOThread();
}

content::JavaScriptDialogManager*
    WebViewGuest::GetJavaScriptDialogManager() {
  return &javascript_dialog_helper_;
}

content::ColorChooser* WebViewGuest::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return NULL;
  return embedder_web_contents()->GetDelegate()->OpenColorChooser(
      web_contents, color, suggestions);
}

void WebViewGuest::RunFileChooser(WebContents* web_contents,
                                  const content::FileChooserParams& params) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->RunFileChooser(web_contents, params);
}

void WebViewGuest::NavigateGuest(const std::string& src) {
  GURL url = ResolveURL(src);

  // Do not allow navigating a guest to schemes other than known safe schemes.
  // This will block the embedder trying to load unwanted schemes, e.g.
  // chrome://settings.
  bool scheme_is_blocked =
      (!content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
           url.scheme()) &&
       !url.SchemeIs(url::kAboutScheme)) ||
      url.SchemeIs(url::kJavaScriptScheme);
  if (scheme_is_blocked || !url.is_valid()) {
    LoadAbort(true /* is_top_level */, url,
              net::ErrorToShortString(net::ERR_ABORTED));
    return;
  }

  GURL validated_url(url);
  guest_web_contents()->GetRenderProcessHost()->
      FilterURL(false, &validated_url);
  // As guests do not swap processes on navigation, only navigations to
  // normal web URLs are supported.  No protocol handlers are installed for
  // other schemes (e.g., WebUI or extensions), and no permissions or bindings
  // can be granted to the guest process.
  LoadURLWithParams(validated_url,
                    content::Referrer(),
                    content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                    guest_web_contents());
}

#if defined(OS_CHROMEOS)
void WebViewGuest::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& details) {
  if (details.notification_type == chromeos::ACCESSIBILITY_MANAGER_SHUTDOWN) {
    accessibility_subscription_.reset();
  } else if (details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    if (details.enabled)
      InjectChromeVoxIfNeeded(guest_web_contents()->GetRenderViewHost());
    else
      chromevox_injected_ = false;
  }
}
#endif

void WebViewGuest::InjectChromeVoxIfNeeded(
    content::RenderViewHost* render_view_host) {
#if defined(OS_CHROMEOS)
  if (!chromevox_injected_) {
    chromeos::AccessibilityManager* manager =
        chromeos::AccessibilityManager::Get();
    if (manager && manager->IsSpokenFeedbackEnabled()) {
      manager->InjectChromeVox(render_view_host);
      chromevox_injected_ = true;
    }
  }
#endif
}

bool WebViewGuest::HandleKeyboardShortcuts(
    const content::NativeWebKeyboardEvent& event) {
  if (event.type != blink::WebInputEvent::RawKeyDown)
    return false;

  // If the user hits the escape key without any modifiers then unlock the
  // mouse if necessary.
  if ((event.windowsKeyCode == ui::VKEY_ESCAPE) &&
      !(event.modifiers & blink::WebInputEvent::InputModifiers)) {
    return guest_web_contents()->GotResponseToLockMouseRequest(false);
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

void WebViewGuest::SetUpAutoSize() {
  // Read the autosize parameters passed in from the embedder.
  bool auto_size_enabled = false;
  extra_params()->GetBoolean(webview::kAttributeAutoSize, &auto_size_enabled);

  int max_height = 0;
  int max_width = 0;
  extra_params()->GetInteger(webview::kAttributeMaxHeight, &max_height);
  extra_params()->GetInteger(webview::kAttributeMaxWidth, &max_width);

  int min_height = 0;
  int min_width = 0;
  extra_params()->GetInteger(webview::kAttributeMinHeight, &min_height);
  extra_params()->GetInteger(webview::kAttributeMinWidth, &min_width);

  // Call SetAutoSize to apply all the appropriate validation and clipping of
  // values.
  SetAutoSize(auto_size_enabled,
              gfx::Size(min_width, min_height),
              gfx::Size(max_width, max_height));
}

void WebViewGuest::ShowContextMenu(int request_id,
                                   const MenuItemVector* items) {
  if (!pending_menu_.get())
    return;

  // Make sure this was the correct request.
  if (request_id != pending_context_menu_request_id_)
    return;

  // TODO(lazyboy): Implement.
  DCHECK(!items);

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  menu_delegate->ShowMenu(pending_menu_.Pass());
}

void WebViewGuest::SetName(const std::string& name) {
  if (name_ == name)
    return;
  name_ = name;

  Send(new ChromeViewMsg_SetName(routing_id(), name_));
}

void WebViewGuest::SetZoom(double zoom_factor) {
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(guest_web_contents());
  DCHECK(zoom_controller);
  double zoom_level = content::ZoomFactorToZoomLevel(zoom_factor);
  zoom_controller->SetZoomLevel(zoom_level);

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetDouble(webview::kOldZoomFactor, current_zoom_factor_);
  args->SetDouble(webview::kNewZoomFactor, zoom_factor);
  DispatchEventToEmbedder(
      new GuestViewBase::Event(webview::kEventZoomChange, args.Pass()));

  current_zoom_factor_ = zoom_factor;
}

void WebViewGuest::AddNewContents(content::WebContents* source,
                                  content::WebContents* new_contents,
                                  WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_pos,
                                  bool user_gesture,
                                  bool* was_blocked) {
  if (was_blocked)
    *was_blocked = false;
  RequestNewWindowPermission(disposition,
                             initial_pos,
                             user_gesture,
                             new_contents);
}

content::WebContents* WebViewGuest::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // If the guest wishes to navigate away prior to attachment then we save the
  // navigation to perform upon attachment. Navigation initializes a lot of
  // state that assumes an embedder exists, such as RenderWidgetHostViewGuest.
  // Navigation also resumes resource loading which we don't want to allow
  // until attachment.
  if (!attached()) {
    WebViewGuest* opener = GetOpener();
    PendingWindowMap::iterator it =
        opener->pending_new_windows_.find(this);
    if (it == opener->pending_new_windows_.end())
      return NULL;
    const NewWindowInfo& info = it->second;
    NewWindowInfo new_window_info(params.url, info.name);
    new_window_info.changed = new_window_info.url != info.url;
    it->second = new_window_info;
    return NULL;
  }
  if (params.disposition == CURRENT_TAB) {
    // This can happen for cross-site redirects.
    LoadURLWithParams(params.url, params.referrer, params.transition, source);
    return source;
  }

  CreateNewGuestWebViewWindow(params);
  return NULL;
}

void WebViewGuest::WebContentsCreated(WebContents* source_contents,
                                      int opener_render_frame_id,
                                      const base::string16& frame_name,
                                      const GURL& target_url,
                                      content::WebContents* new_contents) {
  WebViewGuest* guest = WebViewGuest::FromWebContents(new_contents);
  CHECK(guest);
  guest->SetOpener(this);
  std::string guest_name = base::UTF16ToUTF8(frame_name);
  guest->name_ = guest_name;
  pending_new_windows_.insert(
      std::make_pair(guest, NewWindowInfo(target_url, guest_name)));
}

void WebViewGuest::LoadURLWithParams(const GURL& url,
                                     const content::Referrer& referrer,
                                     content::PageTransition transition_type,
                                     content::WebContents* web_contents) {
  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = referrer;
  load_url_params.transition_type = transition_type;
  load_url_params.extra_headers = std::string();
  if (is_overriding_user_agent_) {
    load_url_params.override_user_agent =
        content::NavigationController::UA_OVERRIDE_TRUE;
  }
  web_contents->GetController().LoadURLWithParams(load_url_params);
}

void WebViewGuest::RequestNewWindowPermission(
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_bounds,
    bool user_gesture,
    content::WebContents* new_contents) {
  WebViewGuest* guest = WebViewGuest::FromWebContents(new_contents);
  if (!guest)
    return;
  PendingWindowMap::iterator it = pending_new_windows_.find(guest);
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
  request_info.SetInteger(webview::kWindowID, guest->GetGuestInstanceID());
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
                                   base::Unretained(this),
                                   guest->GetGuestInstanceID()),
                                   false /* allowed_by_default */);
}

void WebViewGuest::DestroyUnattachedWindows() {
  // Destroy() reaches in and removes the WebViewGuest from its opener's
  // pending_new_windows_ set. To avoid mutating the set while iterating, we
  // create a copy of the pending new windows set and iterate over the copy.
  PendingWindowMap pending_new_windows(pending_new_windows_);
  // Clean up unattached new windows opened by this guest.
  for (PendingWindowMap::const_iterator it = pending_new_windows.begin();
       it != pending_new_windows.end(); ++it) {
    it->first->Destroy();
  }
  // All pending windows should be removed from the set after Destroy() is
  // called on all of them.
  DCHECK(pending_new_windows_.empty());
}

GURL WebViewGuest::ResolveURL(const std::string& src) {
  if (!in_extension()) {
    NOTREACHED();
    return GURL(src);
  }

  GURL default_url(base::StringPrintf("%s://%s/",
                                      extensions::kExtensionScheme,
                                      embedder_extension_id().c_str()));
  return default_url.Resolve(src);
}

void WebViewGuest::OnWebViewNewWindowResponse(
    int new_window_instance_id,
    bool allow,
    const std::string& user_input) {
  WebViewGuest* guest =
      WebViewGuest::From(embedder_render_process_id(), new_window_instance_id);
  if (!guest)
    return;

  if (!allow)
    guest->Destroy();
}
