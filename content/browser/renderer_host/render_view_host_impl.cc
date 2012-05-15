// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/power_save_blocker.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/accessibility_messages.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/speech_recognition_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/selected_file_info.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebScreenInfoFactory.h"
#endif
#include "ui/gfx/native_widget_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webdropdata.h"

using base::TimeDelta;
using WebKit::WebConsoleMessage;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;
using WebKit::WebMediaPlayerAction;
using WebKit::WebPluginAction;

namespace {

// Delay to wait on closing the WebContents for a beforeunload/unload handler to
// fire.
const int kUnloadTimeoutMS = 1000;

// Translate a WebKit text direction into a base::i18n one.
base::i18n::TextDirection WebTextDirectionToChromeTextDirection(
    WebKit::WebTextDirection dir) {
  switch (dir) {
    case WebKit::WebTextDirectionLeftToRight:
      return base::i18n::LEFT_TO_RIGHT;
    case WebKit::WebTextDirectionRightToLeft:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
      return base::i18n::UNKNOWN_DIRECTION;
  }
}

}  // namespace

namespace content {

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id);
  if (!process)
    return NULL;
  RenderWidgetHost* widget = process->GetRenderWidgetHostByID(render_view_id);
  if (!widget || !widget->IsRenderView())
    return NULL;
  return static_cast<RenderViewHostImpl*>(RenderWidgetHostImpl::From(widget));
}

// static
RenderViewHost* RenderViewHost::From(RenderWidgetHost* rwh) {
  return static_cast<RenderViewHostImpl*>(RenderWidgetHostImpl::From(rwh));
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, public:

// static
RenderViewHostImpl* RenderViewHostImpl::FromID(int render_process_id,
                                               int render_view_id) {
  return static_cast<RenderViewHostImpl*>(
      RenderViewHost::FromID(render_process_id, render_view_id));
}

RenderViewHostImpl::RenderViewHostImpl(SiteInstance* instance,
                                       RenderViewHostDelegate* delegate,
                                       int routing_id,
                                       bool swapped_out,
                                       SessionStorageNamespace* session_storage)
    : RenderWidgetHostImpl(instance->GetProcess(), routing_id),
      delegate_(delegate),
      instance_(static_cast<SiteInstanceImpl*>(instance)),
      waiting_for_drag_context_response_(false),
      enabled_bindings_(0),
      guest_(false),
      pending_request_id_(-1),
      navigations_suspended_(false),
      suspended_nav_message_(NULL),
      is_swapped_out_(swapped_out),
      run_modal_reply_msg_(NULL),
      run_modal_opener_id_(MSG_ROUTING_NONE),
      is_waiting_for_beforeunload_ack_(false),
      is_waiting_for_unload_ack_(false),
      unload_ack_is_for_cross_site_transition_(false),
      are_javascript_messages_suppressed_(false),
      sudden_termination_allowed_(false),
      session_storage_namespace_(
          static_cast<SessionStorageNamespaceImpl*>(session_storage)),
      save_accessibility_tree_for_testing_(false),
      send_accessibility_updated_notifications_(false),
      render_view_termination_status_(base::TERMINATION_STATUS_STILL_RUNNING) {
  if (!session_storage_namespace_) {
    DOMStorageContext* dom_storage_context =
        BrowserContext::GetDOMStorageContext(GetProcess()->GetBrowserContext());
    session_storage_namespace_ = new SessionStorageNamespaceImpl(
        static_cast<DOMStorageContextImpl*>(dom_storage_context));
  }

  DCHECK(instance_);
  CHECK(delegate_);  // http://crbug.com/82827

  GetProcess()->EnableSendQueue();

  content::GetContentClient()->browser()->RenderViewHostCreated(this);

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
      content::Source<RenderViewHost>(this),
      content::NotificationService::NoDetails());
}

RenderViewHostImpl::~RenderViewHostImpl() {
  FOR_EACH_OBSERVER(
      content::RenderViewHostObserver, observers_, RenderViewHostDestruction());

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
      content::Source<RenderViewHost>(this),
      content::NotificationService::NoDetails());

  ClearPowerSaveBlockers();

  GetDelegate()->RenderViewDeleted(this);

  // Be sure to clean up any leftover state from cross-site requests.
  CrossSiteRequestManager::GetInstance()->SetHasPendingCrossSiteRequest(
      GetProcess()->GetID(), GetRoutingID(), false);
}

content::RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() const {
  return delegate_;
}

content::SiteInstance* RenderViewHostImpl::GetSiteInstance() const {
  return instance_;
}

bool RenderViewHostImpl::CreateRenderView(const string16& frame_name,
                                          int opener_route_id,
                                          int32 max_page_id) {
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;
  DCHECK(GetProcess()->HasConnection());
  DCHECK(GetProcess()->GetBrowserContext());

  renderer_initialized_ = true;

  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id(), GetCompositingSurface());

  // Ensure the RenderView starts with a next_page_id larger than any existing
  // page ID it might be asked to render.
  int32 next_page_id = 1;
  if (max_page_id > -1)
    next_page_id = max_page_id + 1;

  ViewMsg_New_Params params;
  params.parent_window = GetNativeViewId();
  params.renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
  params.web_preferences = delegate_->GetWebkitPrefs();
  params.view_id = GetRoutingID();
  params.surface_id = surface_id();
  params.session_storage_namespace_id = session_storage_namespace_->id();
  params.frame_name = frame_name;
  // Ensure the RenderView sets its opener correctly.
  params.opener_route_id = opener_route_id;
  params.swapped_out = is_swapped_out_;
  params.next_page_id = next_page_id;
#if defined(OS_POSIX) || defined(USE_AURA)
  if (GetView()) {
    static_cast<content::RenderWidgetHostViewPort*>(
        GetView())->GetScreenInfo(&params.screen_info);
  } else {
    content::RenderWidgetHostViewPort::GetDefaultScreenInfo(
        &params.screen_info);
  }
#else
  params.screen_info =
      WebKit::WebScreenInfoFactory::screenInfo(
          gfx::NativeViewFromId(GetNativeViewId()));
#endif
  params.guest = guest_;
  params.accessibility_mode =
      BrowserAccessibilityState::GetInstance()->IsAccessibleBrowser() ?
          AccessibilityModeComplete :
          AccessibilityModeOff;

#if defined(OS_WIN)
  // On Windows 8, always enable accessibility for editable text controls
  // so we can show the virtual keyboard when one is enabled.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      params.accessibility_mode == AccessibilityModeOff) {
    params.accessibility_mode = AccessibilityModeEditableTextOnly;
  }
#endif

  Send(new ViewMsg_New(params));

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
  // Let our delegate know that we created a RenderView.
  delegate_->RenderViewCreated(this);

  // Invert the color scheme if a flag is set.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInvertWebContent))
    Send(new ViewMsg_InvertWebContent(GetRoutingID(), true));

  FOR_EACH_OBSERVER(
      content::RenderViewHostObserver, observers_, RenderViewHostInitialized());

  return true;
}

bool RenderViewHostImpl::IsRenderViewLive() const {
  return GetProcess()->HasConnection() && renderer_initialized_;
}

void RenderViewHostImpl::SyncRendererPrefs() {
  Send(new ViewMsg_SetRendererPrefs(GetRoutingID(),
                                    delegate_->GetRendererPrefs(
                                        GetProcess()->GetBrowserContext())));
}

void RenderViewHostImpl::Navigate(const ViewMsg_Navigate_Params& params) {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
      GetProcess()->GetID(), params.url);

  ViewMsg_Navigate* nav_message = new ViewMsg_Navigate(GetRoutingID(), params);

  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (navigations_suspended_) {
    // Shouldn't be possible to have a second navigation while suspended, since
    // navigations will only be suspended during a cross-site request.  If a
    // second navigation occurs, WebContentsImpl will cancel this pending RVH
    // create a new pending RVH.
    DCHECK(!suspended_nav_message_.get());
    suspended_nav_message_.reset(nav_message);
  } else {
    // Get back to a clean state, in case we start a new navigation without
    // completing a RVH swap or unload handler.
    SetSwappedOut(false);

    Send(nav_message);
  }

  // Force the throbber to start. We do this because WebKit's "started
  // loading" message will be received asynchronously from the UI of the
  // browser. But we want to keep the throbber in sync with what's happening
  // in the UI. For example, we want to start throbbing immediately when the
  // user naivgates even if the renderer is delayed. There is also an issue
  // with the throbber starting because the WebUI (which controls whether the
  // favicon is displayed) happens synchronously. If the start loading
  // messages was asynchronous, then the default favicon would flash in.
  //
  // WebKit doesn't send throb notifications for JavaScript URLs, so we
  // don't want to either.
  if (!params.url.SchemeIs(chrome::kJavaScriptScheme))
    delegate_->DidStartLoading();

  FOR_EACH_OBSERVER(content::RenderViewHostObserver,
                    observers_, Navigate(params.url));
}

void RenderViewHostImpl::NavigateToURL(const GURL& url) {
  ViewMsg_Navigate_Params params;
  params.page_id = -1;
  params.pending_history_list_offset = -1;
  params.current_history_list_offset = -1;
  params.current_history_list_length = 0;
  params.url = url;
  params.transition = content::PAGE_TRANSITION_LINK;
  params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  Navigate(params);
}

void RenderViewHostImpl::SetNavigationsSuspended(bool suspend) {
  // This should only be called to toggle the state.
  DCHECK(navigations_suspended_ != suspend);

  navigations_suspended_ = suspend;
  if (!suspend && suspended_nav_message_.get()) {
    // There's a navigation message waiting to be sent.  Now that we're not
    // suspended anymore, resume navigation by sending it.  If we were swapped
    // out, we should also stop filtering out the IPC messages now.
    SetSwappedOut(false);

    Send(suspended_nav_message_.release());
  }
}

void RenderViewHostImpl::CancelSuspendedNavigations() {
  // Clear any state if a pending navigation is canceled or pre-empted.
  if (suspended_nav_message_.get())
    suspended_nav_message_.reset();
  navigations_suspended_ = false;
}

void RenderViewHostImpl::SetNavigationStartTime(
    const base::TimeTicks& navigation_start) {
  Send(new ViewMsg_SetNavigationStartTime(GetRoutingID(), navigation_start));
}

void RenderViewHostImpl::FirePageBeforeUnload(bool for_cross_site_transition) {
  if (!IsRenderViewLive()) {
    // This RenderViewHostImpl doesn't have a live renderer, so just
    // skip running the onbeforeunload handler.
    is_waiting_for_beforeunload_ack_ = true;  // Checked by OnMsgShouldCloseACK.
    unload_ack_is_for_cross_site_transition_ = for_cross_site_transition;
    base::TimeTicks now = base::TimeTicks::Now();
    OnMsgShouldCloseACK(true, now, now);
    return;
  }

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if she clicks the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_ack_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_cross_site_transition_ =
        unload_ack_is_for_cross_site_transition_ && for_cross_site_transition;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_ack_ = true;
    unload_ack_is_for_cross_site_transition_ = for_cross_site_transition;
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));
    send_should_close_start_time_ = base::TimeTicks::Now();
    Send(new ViewMsg_ShouldClose(GetRoutingID()));
  }
}

void RenderViewHostImpl::SwapOut(int new_render_process_host_id,
                                 int new_request_id) {
  // This will be set back to false in OnSwapOutACK, just before we replace
  // this RVH with the pending RVH.
  is_waiting_for_unload_ack_ = true;
  // Start the hang monitor in case the renderer hangs in the unload handler.
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  ViewMsg_SwapOut_Params params;
  params.closing_process_id = GetProcess()->GetID();
  params.closing_route_id = GetRoutingID();
  params.new_render_process_host_id = new_render_process_host_id;
  params.new_request_id = new_request_id;
  if (IsRenderViewLive()) {
    Send(new ViewMsg_SwapOut(GetRoutingID(), params));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the unload
    // event.  We must notify the ResourceDispatcherHost on the IO thread,
    // which we will do through the RenderProcessHost's widget helper.
    GetProcess()->CrossSiteSwapOutACK(params);
  }
}

void RenderViewHostImpl::OnSwapOutACK() {
  // Stop the hang monitor now that the unload handler has finished.
  StopHangMonitorTimeout();
  is_waiting_for_unload_ack_ = false;
  delegate_->SwappedOut(this);
}

void RenderViewHostImpl::WasSwappedOut() {
  // Don't bother reporting hung state anymore.
  StopHangMonitorTimeout();

  // Now that we're no longer the active RVH in the tab, start filtering out
  // most IPC messages.  Usually the renderer will have stopped sending
  // messages as of OnSwapOutACK.  However, we may have timed out waiting
  // for that message, and additional IPC messages may keep streaming in.
  // We filter them out, as long as that won't cause problems (e.g., we
  // still allow synchronous messages through).
  SetSwappedOut(true);

  // Inform the renderer that it can exit if no one else is using it.
  Send(new ViewMsg_WasSwappedOut(GetRoutingID()));
}

void RenderViewHostImpl::ClosePage() {
  // Start the hang monitor in case the renderer hangs in the unload handler.
  is_waiting_for_unload_ack_ = true;
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  if (IsRenderViewLive()) {
    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        content::Source<RenderViewHost>(this),
        content::NotificationService::NoDetails());

    Send(new ViewMsg_ClosePage(GetRoutingID()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the unload
    // event and close the page.
    ClosePageIgnoringUnloadEvents();
  }
}

void RenderViewHostImpl::ClosePageIgnoringUnloadEvents() {
  StopHangMonitorTimeout();
  is_waiting_for_beforeunload_ack_ = false;
  is_waiting_for_unload_ack_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

void RenderViewHostImpl::SetHasPendingCrossSiteRequest(bool has_pending_request,
                                                       int request_id) {
  CrossSiteRequestManager::GetInstance()->SetHasPendingCrossSiteRequest(
      GetProcess()->GetID(), GetRoutingID(), has_pending_request);
  pending_request_id_ = request_id;
}

int RenderViewHostImpl::GetPendingRequestId() {
  return pending_request_id_;
}

void RenderViewHostImpl::DragTargetDragEnter(
    const WebDropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed) {
  const int renderer_id = GetProcess()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // The URL could have been cobbled together from any highlighted text string,
  // and can't be interpreted as a capability.
  WebDropData filtered_data(drop_data);
  FilterURL(policy, renderer_id, true, &filtered_data.url);

  // The filenames vector, on the other hand, does represent a capability to
  // access the given files.
  std::set<FilePath> filesets;
  for (std::vector<WebDropData::FileInfo>::const_iterator iter(
           filtered_data.filenames.begin());
       iter != filtered_data.filenames.end(); ++iter) {
    FilePath path = FilePath::FromUTF8Unsafe(UTF16ToUTF8(iter->path));
    policy->GrantRequestURL(renderer_id, net::FilePathToFileURL(path));

    // If the renderer already has permission to read these paths, we don't need
    // to re-grant them. This prevents problems with DnD for files in the CrOS
    // file manager--the file manager already had read/write access to those
    // directories, but dragging a file would cause the read/write access to be
    // overwritten with read-only access, making them impossible to delete or
    // rename until the renderer was killed.
    if (!policy->CanReadFile(renderer_id, path)) {
      policy->GrantReadFile(renderer_id, path);
      // Allow dragged directories to be enumerated by the child process.
      // Note that we can't tell a file from a directory at this point.
      policy->GrantReadDirectory(renderer_id, path);
    }

    filesets.insert(path);
  }

  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  std::string filesystem_id = isolated_context->RegisterIsolatedFileSystem(
      filesets);
  policy->GrantAccessFileSystem(renderer_id, filesystem_id);
  filtered_data.filesystem_id = UTF8ToUTF16(filesystem_id);

  Send(new DragMsg_TargetDragEnter(GetRoutingID(), filtered_data, client_pt,
                                   screen_pt, operations_allowed));
}

void RenderViewHostImpl::DragTargetDragOver(
    const gfx::Point& client_pt, const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed) {
  Send(new DragMsg_TargetDragOver(GetRoutingID(), client_pt, screen_pt,
                                  operations_allowed));
}

void RenderViewHostImpl::DragTargetDragLeave() {
  Send(new DragMsg_TargetDragLeave(GetRoutingID()));
}

void RenderViewHostImpl::DragTargetDrop(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  Send(new DragMsg_TargetDrop(GetRoutingID(), client_pt, screen_pt));
}

void RenderViewHostImpl::DesktopNotificationPermissionRequestDone(
    int callback_context) {
  Send(new DesktopNotificationMsg_PermissionRequestDone(
      GetRoutingID(), callback_context));
}

void RenderViewHostImpl::DesktopNotificationPostDisplay(int callback_context) {
  Send(new DesktopNotificationMsg_PostDisplay(GetRoutingID(),
                                              callback_context));
}

void RenderViewHostImpl::DesktopNotificationPostError(int notification_id,
                                                      const string16& message) {
  Send(new DesktopNotificationMsg_PostError(
      GetRoutingID(), notification_id, message));
}

void RenderViewHostImpl::DesktopNotificationPostClose(int notification_id,
                                                      bool by_user) {
  Send(new DesktopNotificationMsg_PostClose(
      GetRoutingID(), notification_id, by_user));
}

void RenderViewHostImpl::DesktopNotificationPostClick(int notification_id) {
  Send(new DesktopNotificationMsg_PostClick(GetRoutingID(), notification_id));
}

void RenderViewHostImpl::ExecuteJavascriptInWebFrame(
    const string16& frame_xpath,
    const string16& jscript) {
  Send(new ViewMsg_ScriptEvalRequest(GetRoutingID(), frame_xpath, jscript,
                                     0, false));
}

int RenderViewHostImpl::ExecuteJavascriptInWebFrameNotifyResult(
    const string16& frame_xpath,
    const string16& jscript) {
  static int next_id = 1;
  Send(new ViewMsg_ScriptEvalRequest(GetRoutingID(), frame_xpath, jscript,
                                     next_id, true));
  return next_id++;
}

typedef std::pair<int, Value*> ExecuteDetailType;

ExecuteNotificationObserver::ExecuteNotificationObserver(int id)
: id_(id) {
}

ExecuteNotificationObserver::~ExecuteNotificationObserver() {
}

void ExecuteNotificationObserver::Observe(int type,
                     const content::NotificationSource& source,
                     const content::NotificationDetails& details) {
  content::Details<ExecuteDetailType> execute_details =
      static_cast<content::Details<ExecuteDetailType> >(details);
  int id = execute_details->first;
  if (id != id_)
    return;
  Value* value = execute_details->second;
  if (value)
    value_.reset(value->DeepCopy());
  MessageLoop::current()->Quit();
}

Value* RenderViewHostImpl::ExecuteJavascriptAndGetValue(
    const string16& frame_xpath,
    const string16& jscript) {
  int id = ExecuteJavascriptInWebFrameNotifyResult(frame_xpath, jscript);
  ExecuteNotificationObserver observer(id);
  content::NotificationRegistrar notification_registrar;
  notification_registrar.Add(
      &observer, content::NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,
      content::Source<RenderViewHost>(this));
  MessageLoop* loop = MessageLoop::current();
  loop->Run();
  return observer.value()->DeepCopy();
}

void RenderViewHostImpl::JavaScriptDialogClosed(IPC::Message* reply_msg,
                                                bool success,
                                                const string16& user_input) {
  GetProcess()->SetIgnoreInputEvents(false);
  bool is_waiting =
      is_waiting_for_beforeunload_ack_ || is_waiting_for_unload_ack_;
  if (is_waiting)
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  ViewHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg,
                                                     success, user_input);
  Send(reply_msg);

  // If we are waiting for an unload or beforeunload ack and the user has
  // suppressed messages, kill the tab immediately; a page that's spamming
  // alerts in onbeforeunload is presumably malicious, so there's no point in
  // continuing to run its script and dragging out the process.
  // This must be done after sending the reply since RenderView can't close
  // correctly while waiting for a response.
  if (is_waiting && are_javascript_messages_suppressed_)
    delegate_->RendererUnresponsive(this, is_waiting);
}

void RenderViewHostImpl::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y,
    WebDragOperation operation) {
  Send(new DragMsg_SourceEndedOrMoved(
      GetRoutingID(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      true, operation));
}

void RenderViewHostImpl::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new DragMsg_SourceEndedOrMoved(
      GetRoutingID(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      false, WebDragOperationNone));
}

void RenderViewHostImpl::DragSourceSystemDragEnded() {
  Send(new DragMsg_SourceSystemDragEnded(GetRoutingID()));
}

void RenderViewHostImpl::AllowBindings(int bindings_flags) {
  // Ensure we aren't granting WebUI bindings to a process that has already
  // been used for non-privileged views.
  if (bindings_flags & content::BINDINGS_POLICY_WEB_UI &&
      GetProcess()->HasConnection() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have more
    // than this single view.
    RenderProcessHostImpl::RenderWidgetHostsIterator iter(
        GetProcess()->GetRenderWidgetHostsIterator());
    iter.Advance();
    if (!iter.IsAtEnd())
      return;
  }

  if (bindings_flags & content::BINDINGS_POLICY_WEB_UI) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID());
  }

  enabled_bindings_ |= bindings_flags;
  if (renderer_initialized_)
    Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
}

int RenderViewHostImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderViewHostImpl::SetWebUIProperty(const std::string& name,
                                          const std::string& value) {
  // This is just a sanity check before telling the renderer to enable the
  // property.  It could lie and send the corresponding IPC messages anyway,
  // but we will not act on them if enabled_bindings_ doesn't agree.
  if (enabled_bindings_ & content::BINDINGS_POLICY_WEB_UI)
    Send(new ViewMsg_SetWebUIProperty(GetRoutingID(), name, value));
  else
    NOTREACHED() << "WebUI bindings not enabled.";
}

void RenderViewHostImpl::GotFocus() {
  RenderWidgetHostImpl::GotFocus();  // Notifies the renderer it got focus.

  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->GotFocus();
}

void RenderViewHostImpl::LostCapture() {
  RenderWidgetHostImpl::LostCapture();
  delegate_->LostCapture();
}

void RenderViewHostImpl::LostMouseLock() {
  RenderWidgetHostImpl::LostMouseLock();
  delegate_->LostMouseLock();
}

void RenderViewHostImpl::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(GetRoutingID(), reverse));
}

void RenderViewHostImpl::FilesSelectedInChooser(
    const std::vector<SelectedFileInfo>& files,
    int permissions) {
  // Grant the security access requested to the given files.
  for (size_t i = 0; i < files.size(); ++i) {
    const SelectedFileInfo& file = files[i];
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantPermissionsForFile(
        GetProcess()->GetID(), file.path, permissions);
  }
  Send(new ViewMsg_RunFileChooserResponse(GetRoutingID(), files));
}

void RenderViewHostImpl::DirectoryEnumerationFinished(
    int request_id,
    const std::vector<FilePath>& files) {
  // Grant the security access requested to the given files.
  for (std::vector<FilePath>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        GetProcess()->GetID(), *file);
  }
  Send(new ViewMsg_EnumerateDirectoryResponse(GetRoutingID(),
                                              request_id,
                                              files));
}

void RenderViewHostImpl::LoadStateChanged(
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64 upload_position,
    uint64 upload_size) {
  delegate_->LoadStateChanged(url, load_state, upload_position, upload_size);
}

bool RenderViewHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_ ||
      GetProcess()->SuddenTerminationAllowed();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
  if (!BrowserMessageFilter::CheckCanDispatchOnUI(msg, this))
    return true;

  // Filter out most IPC messages if this renderer is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  if (is_swapped_out_) {
    if (!content::SwappedOutMessages::CanHandleWhileSwappedOut(msg)) {
      // If this is a synchronous message and we decided not to handle it,
      // we must send an error reply, or else the renderer will be stuck
      // and won't respond to future requests.
      if (msg.is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
        reply->set_reply_error();
        Send(reply);
      }
      // Don't continue looking for someone to handle it.
      return true;
    }
  }

  ObserverListBase<content::RenderViewHostObserver>::Iterator it(observers_);
  content::RenderViewHostObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnMessageReceived(msg))
      return true;
  }

  if (delegate_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderViewHostImpl, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnMsgShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnMsgShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowFullscreenWidget,
                        OnMsgShowFullscreenWidget)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunModal, OnMsgRunModal)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnMsgRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnMsgRenderViewGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                        OnMsgDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidRedirectProvisionalLoad,
                        OnMsgDidRedirectProvisionalLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFailProvisionalLoadWithError,
                        OnMsgDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_FrameNavigate, OnMsgNavigate(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnMsgUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTitle, OnMsgUpdateTitle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateEncoding, OnMsgUpdateEncoding)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnMsgUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateInspectorSetting,
                        OnUpdateInspectorSetting)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnMsgClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnMsgRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartLoading, OnMsgDidStartLoading)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStopLoading, OnMsgDidStopLoading)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeLoadProgress,
                        OnMsgDidChangeLoadProgress)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentAvailableInMainFrame,
                        OnMsgDocumentAvailableInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentOnLoadCompletedInMainFrame,
                        OnMsgDocumentOnLoadCompletedInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ContextMenu, OnMsgContextMenu)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ToggleFullscreen,
                        OnMsgToggleFullscreen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenURL, OnMsgOpenURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredSizeChange,
                        OnMsgDidContentsPreferredSizeChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeScrollbarsForMainFrame,
                        OnMsgDidChangeScrollbarsForMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeScrollOffsetPinningForMainFrame,
                        OnMsgDidChangeScrollOffsetPinningForMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeNumWheelEvents,
                        OnMsgDidChangeNumWheelEvents)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteCloseEvent,
                        OnMsgRouteCloseEvent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteMessageEvent, OnMsgRouteMessageEvent)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunJavaScriptMessage,
                                    OnMsgRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunBeforeUnloadConfirm,
                                    OnMsgRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER(DragHostMsg_StartDragging, OnMsgStartDragging)
    IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(DragHostMsg_TargetDrop_ACK, OnTargetDropACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShouldClose_ACK, OnMsgShouldCloseACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnMsgClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionChanged, OnMsgSelectionChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionBoundsChanged,
                        OnMsgSelectionBoundsChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ScriptEvalResponse, OnScriptEvalResponse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaNotification, OnMediaNotification)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_RequestPermission,
                        OnRequestDesktopNotificationPermission)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_Show,
                        OnShowDesktopNotification)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_Cancel,
                        OnCancelDesktopNotification)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnMsgShowPopup)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunFileChooser, OnRunFileChooser)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DomOperationResponse,
                        OnDomOperationResponse)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_Notifications,
                        OnAccessibilityNotifications)
    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(
        handled = RenderWidgetHostImpl::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message had a handler, but its de-serialization failed.
    // Kill the renderer.
    content::RecordAction(UserMetricsAction("BadMessageTerminate_RVH"));
    GetProcess()->ReceivedBadMessage();
  }

  return handled;
}

void RenderViewHostImpl::Shutdown() {
  // If we are being run modally (see RunModal), then we need to cleanup.
  if (run_modal_reply_msg_) {
    Send(run_modal_reply_msg_);
    run_modal_reply_msg_ = NULL;
    RenderViewHostImpl* opener =
        RenderViewHostImpl::FromID(GetProcess()->GetID(), run_modal_opener_id_);
    if (opener) {
      opener->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(
          hung_renderer_delay_ms_));
      // Balance out the decrement when we got created.
      opener->increment_in_flight_event_count();
    }
    run_modal_opener_id_ = MSG_ROUTING_NONE;
  }

  RenderWidgetHostImpl::Shutdown();
}

bool RenderViewHostImpl::IsRenderView() const {
  return true;
}

void RenderViewHostImpl::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  view->CreateNewWindow(route_id, params);
}

void RenderViewHostImpl::CreateNewWidget(int route_id,
                                     WebKit::WebPopupType popup_type) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->CreateNewWidget(route_id, popup_type);
}

void RenderViewHostImpl::CreateNewFullscreenWidget(int route_id) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->CreateNewFullscreenWidget(route_id);
}

void RenderViewHostImpl::OnMsgShowView(int route_id,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    if (!is_swapped_out_)
      view->ShowCreatedWindow(route_id, disposition, initial_pos, user_gesture);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHostImpl::OnMsgShowWidget(int route_id,
                                         const gfx::Rect& initial_pos) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    if (!is_swapped_out_)
      view->ShowCreatedWidget(route_id, initial_pos);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHostImpl::OnMsgShowFullscreenWidget(int route_id) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    if (!is_swapped_out_)
      view->ShowCreatedFullscreenWidget(route_id);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHostImpl::OnMsgRunModal(int opener_id, IPC::Message* reply_msg) {
  DCHECK(!run_modal_reply_msg_);
  run_modal_reply_msg_ = reply_msg;
  run_modal_opener_id_ = opener_id;

  content::RecordAction(UserMetricsAction("ShowModalDialog"));

  RenderViewHostImpl* opener =
      RenderViewHostImpl::FromID(GetProcess()->GetID(), run_modal_opener_id_);
  opener->StopHangMonitorTimeout();
  // The ack for the mouse down won't come until the dialog closes, so fake it
  // so that we don't get a timeout.
  opener->decrement_in_flight_event_count();

  // TODO(darin): Bug 1107929: Need to inform our delegate to show this view in
  // an app-modal fashion.
}

void RenderViewHostImpl::OnMsgRenderViewReady() {
  render_view_termination_status_ = base::TERMINATION_STATUS_STILL_RUNNING;
  WasResized();
  delegate_->RenderViewReady(this);
}

void RenderViewHostImpl::OnMsgRenderViewGone(int status, int exit_code) {
  // Keep the termination status so we can get at it later when we
  // need to know why it died.
  render_view_termination_status_ =
      static_cast<base::TerminationStatus>(status);

  // Reset state.
  ClearPowerSaveBlockers();

  // Our base class RenderWidgetHost needs to reset some stuff.
  RendererExited(render_view_termination_status_, exit_code);

  delegate_->RenderViewGone(this,
                            static_cast<base::TerminationStatus>(status),
                            exit_code);
}

void RenderViewHostImpl::OnMsgDidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& opener_url,
    const GURL& url) {
  delegate_->DidStartProvisionalLoadForFrame(
      this, frame_id, is_main_frame, opener_url, url);
}

void RenderViewHostImpl::OnMsgDidRedirectProvisionalLoad(
    int32 page_id,
    const GURL& opener_url,
    const GURL& source_url,
    const GURL& target_url) {
  delegate_->DidRedirectProvisionalLoad(
      this, page_id, opener_url, source_url, target_url);
}

void RenderViewHostImpl::OnMsgDidFailProvisionalLoadWithError(
    const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  delegate_->DidFailProvisionalLoadWithError(this, params);
}

// Called when the renderer navigates.  For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
//
// Subframes are identified by the page transition type.  For subframes loaded
// as part of a wider page load, the page_id will be the same as for the top
// level frame.  If the user explicitly requests a subframe navigation, we will
// get a new page_id because we need to create a new navigation entry for that
// action.
void RenderViewHostImpl::OnMsgNavigate(const IPC::Message& msg) {
  // Read the parameters out of the IPC message directly to avoid making another
  // copy when we filter the URLs.
  PickleIterator iter(msg);
  ViewHostMsg_FrameNavigate_Params validated_params;
  if (!IPC::ParamTraits<ViewHostMsg_FrameNavigate_Params>::
      Read(&msg, &iter, &validated_params))
    return;

  // If we're waiting for a cross-site beforeunload ack from this renderer and
  // we receive a Navigate message from the main frame, then the renderer was
  // navigating already and sent it before hearing the ViewMsg_Stop message.
  // We do not want to cancel the pending navigation in this case, since the
  // old page will soon be stopped.  Instead, treat this as a beforeunload ack
  // to allow the pending navigation to continue.
  if (is_waiting_for_beforeunload_ack_ &&
      unload_ack_is_for_cross_site_transition_ &&
      content::PageTransitionIsMainFrame(validated_params.transition)) {
    OnMsgShouldCloseACK(true, send_should_close_start_time_,
                        base::TimeTicks::Now());
    return;
  }

  // If we're waiting for an unload ack from this renderer and we receive a
  // Navigate message, then the renderer was navigating before it received the
  // unload request.  It will either respond to the unload request soon or our
  // timer will expire.  Either way, we should ignore this message, because we
  // have already committed to closing this renderer.
  if (is_waiting_for_unload_ack_)
    return;

  const int renderer_id = GetProcess()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  FilterURL(policy, renderer_id, false, &validated_params.url);
  FilterURL(policy, renderer_id, true, &validated_params.referrer.url);
  for (std::vector<GURL>::iterator it(validated_params.redirects.begin());
      it != validated_params.redirects.end(); ++it) {
    FilterURL(policy, renderer_id, false, &(*it));
  }
  FilterURL(policy, renderer_id, true, &validated_params.searchable_form_url);
  FilterURL(policy, renderer_id, true, &validated_params.password_form.origin);
  FilterURL(policy, renderer_id, true, &validated_params.password_form.action);

  delegate_->DidNavigate(this, validated_params);
}

void RenderViewHostImpl::OnMsgUpdateState(int32 page_id,
                                          const std::string& state) {
  delegate_->UpdateState(this, page_id, state);
}

void RenderViewHostImpl::OnMsgUpdateTitle(
    int32 page_id,
    const string16& title,
    WebKit::WebTextDirection title_direction) {
  if (title.length() > content::kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }

  delegate_->UpdateTitle(this, page_id, title,
                         WebTextDirectionToChromeTextDirection(
                             title_direction));
}

void RenderViewHostImpl::OnMsgUpdateEncoding(const std::string& encoding_name) {
  delegate_->UpdateEncoding(this, encoding_name);
}

void RenderViewHostImpl::OnMsgUpdateTargetURL(int32 page_id,
                                              const GURL& url) {
  if (!is_swapped_out_)
    delegate_->UpdateTargetURL(page_id, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnUpdateInspectorSetting(
    const std::string& key, const std::string& value) {
  content::GetContentClient()->browser()->UpdateInspectorSetting(
      this, key, value);
}

void RenderViewHostImpl::OnMsgClose() {
  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnMsgRequestMove(const gfx::Rect& pos) {
  if (!is_swapped_out_)
    delegate_->RequestMove(pos);
  Send(new ViewMsg_Move_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnMsgDidStartLoading() {
  delegate_->DidStartLoading();
}

void RenderViewHostImpl::OnMsgDidStopLoading() {
  delegate_->DidStopLoading();
}

void RenderViewHostImpl::OnMsgDidChangeLoadProgress(double load_progress) {
  delegate_->DidChangeLoadProgress(load_progress);
}

void RenderViewHostImpl::OnMsgDocumentAvailableInMainFrame() {
  delegate_->DocumentAvailableInMainFrame(this);
}

void RenderViewHostImpl::OnMsgDocumentOnLoadCompletedInMainFrame(
    int32 page_id) {
  delegate_->DocumentOnLoadCompletedInMainFrame(this, page_id);
}

void RenderViewHostImpl::OnMsgContextMenu(
    const content::ContextMenuParams& params) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  content::ContextMenuParams validated_params(params);
  int renderer_id = GetProcess()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  FilterURL(policy, renderer_id, true, &validated_params.link_url);
  FilterURL(policy, renderer_id, true, &validated_params.src_url);
  FilterURL(policy, renderer_id, false, &validated_params.page_url);
  FilterURL(policy, renderer_id, true, &validated_params.frame_url);

  view->ShowContextMenu(validated_params);
}

void RenderViewHostImpl::OnMsgToggleFullscreen(bool enter_fullscreen) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->ToggleFullscreenMode(enter_fullscreen);
  WasResized();
}

void RenderViewHostImpl::OnMsgOpenURL(const GURL& url,
                                      const content::Referrer& referrer,
                                      WindowOpenDisposition disposition,
                                      int64 source_frame_id) {
  GURL validated_url(url);
  FilterURL(ChildProcessSecurityPolicyImpl::GetInstance(),
            GetProcess()->GetID(), false, &validated_url);

  delegate_->RequestOpenURL(
      this, validated_url, referrer, disposition, source_frame_id);
}

void RenderViewHostImpl::OnMsgDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  delegate_->UpdatePreferredSize(new_size);
}

void RenderViewHostImpl::OnRenderAutoResized(const gfx::Size& new_size) {
  delegate_->ResizeDueToAutoResize(new_size);
}

void RenderViewHostImpl::OnMsgDidChangeScrollbarsForMainFrame(
    bool has_horizontal_scrollbar, bool has_vertical_scrollbar) {
  if (view_)
    view_->SetHasHorizontalScrollbar(has_horizontal_scrollbar);
}

void RenderViewHostImpl::OnMsgDidChangeScrollOffsetPinningForMainFrame(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  if (view_)
    view_->SetScrollOffsetPinning(is_pinned_to_left, is_pinned_to_right);
}

void RenderViewHostImpl::OnMsgDidChangeNumWheelEvents(int count) {
}

void RenderViewHostImpl::OnMsgSelectionChanged(const string16& text,
                                               size_t offset,
                                               const ui::Range& range) {
  if (view_)
    view_->SelectionChanged(text, offset, range);
}

void RenderViewHostImpl::OnMsgSelectionBoundsChanged(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect) {
  if (view_)
    view_->SelectionBoundsChanged(start_rect, end_rect);
}

void RenderViewHostImpl::OnMsgRouteCloseEvent() {
  // Have the delegate route this to the active RenderViewHost.
  delegate_->RouteCloseEvent(this);
}

void RenderViewHostImpl::OnMsgRouteMessageEvent(
    const ViewMsg_PostMessage_Params& params) {
  // Give to the delegate to route to the active RenderViewHost.
  delegate_->RouteMessageEvent(this, params);
}

void RenderViewHostImpl::OnMsgRunJavaScriptMessage(
    const string16& message,
    const string16& default_prompt,
    const GURL& frame_url,
    ui::JavascriptMessageType type,
    IPC::Message* reply_msg) {
  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  StopHangMonitorTimeout();
  delegate_->RunJavaScriptMessage(this, message, default_prompt, frame_url,
                                  type, reply_msg,
                                  &are_javascript_messages_suppressed_);
}

void RenderViewHostImpl::OnMsgRunBeforeUnloadConfirm(const GURL& frame_url,
                                                     const string16& message,
                                                     bool is_reload,
                                                     IPC::Message* reply_msg) {
  // While a JS before unload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  StopHangMonitorTimeout();
  delegate_->RunBeforeUnloadConfirm(this, message, is_reload, reply_msg);
}

void RenderViewHostImpl::OnMsgStartDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask drag_operations_mask,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  WebDropData filtered_data(drop_data);
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Allow drag of Javascript URLs to enable bookmarklet drag to bookmark bar.
  if (!filtered_data.url.SchemeIs(chrome::kJavaScriptScheme))
    FilterURL(policy, GetProcess()->GetID(), true, &filtered_data.url);
  FilterURL(policy, GetProcess()->GetID(), false, &filtered_data.html_base_url);
  // Filter out any paths that the renderer didn't have access to. This prevents
  // the following attack on a malicious renderer:
  // 1. StartDragging IPC sent with renderer-specified filesystem paths that it
  //    doesn't have read permissions for.
  // 2. We initiate a native DnD operation.
  // 3. DnD operation immediately ends since mouse is not held down. DnD events
  //    still fire though, which causes read permissions to be granted to the
  //    renderer for any file paths in the drop.
  filtered_data.filenames.clear();
  for (std::vector<WebDropData::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end(); ++it) {
    FilePath path(FilePath::FromUTF8Unsafe(UTF16ToUTF8(it->path)));
    if (policy->CanReadFile(GetProcess()->GetID(), path))
      filtered_data.filenames.push_back(*it);
  }
  view->StartDragging(filtered_data, drag_operations_mask, image, image_offset);
}

void RenderViewHostImpl::OnUpdateDragCursor(WebDragOperation current_op) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->UpdateDragCursor(current_op);
}

void RenderViewHostImpl::OnTargetDropACK() {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDER_VIEW_HOST_DID_RECEIVE_DRAG_TARGET_DROP_ACK,
      content::Source<RenderViewHost>(this),
      content::NotificationService::NoDetails());
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnFocusedNodeChanged(bool is_editable_node) {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::Source<RenderViewHost>(this),
      content::Details<const bool>(&is_editable_node));
}

void RenderViewHostImpl::OnAddMessageToConsole(int32 level,
                                           const string16& message,
                                           int32 line_no,
                                           const string16& source_id) {
  // Pass through log level only on WebUI pages to limit console spew.
  int32 resolved_level =
      (enabled_bindings_ & content::BINDINGS_POLICY_WEB_UI) ? level : 0;

  logging::LogMessage("CONSOLE", line_no, resolved_level).stream() << "\"" <<
      message << "\", source: " << source_id << " (" << line_no << ")";
}

void RenderViewHostImpl::AddObserver(
    content::RenderViewHostObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderViewHostImpl::RemoveObserver(
    content::RenderViewHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool RenderViewHostImpl::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  return delegate_->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void RenderViewHostImpl::UnhandledKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  delegate_->HandleKeyboardEvent(event);
}

void RenderViewHostImpl::OnUserGesture() {
  delegate_->OnUserGesture();
}

void RenderViewHostImpl::OnMsgShouldCloseACK(
    bool proceed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  StopHangMonitorTimeout();
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in OnMsgNavigate, in which case we can ignore
  // this message.
  if (!is_waiting_for_beforeunload_ack_ || is_swapped_out_)
    return;

  is_waiting_for_beforeunload_ack_ = false;

  RenderViewHostDelegate::RendererManagement* management_delegate =
      delegate_->GetRendererManagementDelegate();
  if (management_delegate) {
    base::TimeTicks before_unload_end_time;
    if (!send_should_close_start_time_.is_null() &&
        !renderer_before_unload_start_time.is_null() &&
        !renderer_before_unload_end_time.is_null()) {
      // When passing TimeTicks across process boundaries, we need to compensate
      // for any skew between the processes. Here we are converting the
      // renderer's notion of before_unload_end_time to TimeTicks in the browser
      // process. See comments in inter_process_time_ticks_converter.h for more.
      content::InterProcessTimeTicksConverter converter(
          LocalTimeTicks::FromTimeTicks(send_should_close_start_time_),
          LocalTimeTicks::FromTimeTicks(base::TimeTicks::Now()),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      LocalTimeTicks browser_before_unload_end_time =
          converter.ToLocalTimeTicks(
              RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();
    }
    management_delegate->ShouldClosePage(
        unload_ack_is_for_cross_site_transition_, proceed,
        before_unload_end_time);
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  if (!proceed)
    delegate_->DidCancelLoading();
}

void RenderViewHostImpl::OnMsgClosePageACK() {
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::NotifyRendererUnresponsive() {
  delegate_->RendererUnresponsive(
      this, is_waiting_for_beforeunload_ack_ || is_waiting_for_unload_ack_);
}

void RenderViewHostImpl::NotifyRendererResponsive() {
  delegate_->RendererResponsive(this);
}

void RenderViewHostImpl::RequestToLockMouse(bool user_gesture) {
  delegate_->RequestToLockMouse(user_gesture);
}

bool RenderViewHostImpl::IsFullscreen() const {
  return delegate_->IsFullscreenForCurrentTab();
}

void RenderViewHostImpl::OnMsgFocus() {
  // Note: We allow focus and blur from swapped out RenderViewHosts, even when
  // the active RenderViewHost is in a different BrowsingInstance (e.g., WebUI).
  delegate_->Activate();
}

void RenderViewHostImpl::OnMsgBlur() {
  delegate_->Deactivate();
}

gfx::Rect RenderViewHostImpl::GetRootWindowResizerRect() const {
  return delegate_->GetRootWindowResizerRect();
}

void RenderViewHostImpl::ForwardMouseEvent(
    const WebKit::WebMouseEvent& mouse_event) {

  // We make a copy of the mouse event because
  // RenderWidgetHost::ForwardMouseEvent will delete |mouse_event|.
  WebKit::WebMouseEvent event_copy(mouse_event);
  RenderWidgetHostImpl::ForwardMouseEvent(event_copy);

  switch (event_copy.type) {
    case WebInputEvent::MouseMove:
      delegate_->HandleMouseMove();
      break;
    case WebInputEvent::MouseLeave:
      delegate_->HandleMouseLeave();
      break;
    case WebInputEvent::MouseDown:
      delegate_->HandleMouseDown();
      break;
    case WebInputEvent::MouseWheel:
      if (ignore_input_events())
        delegate_->OnIgnoredUIEvent();
      break;
    case WebInputEvent::MouseUp:
      delegate_->HandleMouseUp();
    default:
      // For now, we don't care about the rest.
      break;
  }
}

void RenderViewHostImpl::OnMouseActivate() {
  delegate_->HandleMouseActivate();
}

void RenderViewHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (ignore_input_events()) {
    if (key_event.type == WebInputEvent::RawKeyDown)
      delegate_->OnIgnoredUIEvent();
    return;
  }
  RenderWidgetHostImpl::ForwardKeyboardEvent(key_event);
}

#if defined(OS_MACOSX)
void RenderViewHostImpl::DidSelectPopupMenuItem(int selected_index) {
  Send(new ViewMsg_SelectPopupMenuItem(GetRoutingID(), selected_index));
}

void RenderViewHostImpl::DidCancelPopupMenu() {
  Send(new ViewMsg_SelectPopupMenuItem(GetRoutingID(), -1));
}
#endif

void RenderViewHostImpl::ToggleSpeechInput() {
  Send(new InputTagSpeechMsg_ToggleSpeechInput(GetRoutingID()));
}

void RenderViewHostImpl::FilterURL(ChildProcessSecurityPolicyImpl* policy,
                                   int renderer_id,
                                   bool empty_allowed,
                                   GURL* url) {
  if (empty_allowed && url->is_empty())
    return;

  if (!url->is_valid()) {
    // Have to use about:blank for the denied case, instead of an empty GURL.
    // This is because the browser treats navigation to an empty GURL as a
    // navigation to the home page. This is often a privileged page
    // (chrome://newtab/) which is exactly what we don't want.
    *url = GURL(chrome::kAboutBlankURL);
    return;
  }

  if (url->SchemeIs(chrome::kAboutScheme)) {
    // The renderer treats all URLs in the about: scheme as being about:blank.
    // Canonicalize about: URLs to about:blank.
    *url = GURL(chrome::kAboutBlankURL);
  }

  if (!policy->CanRequestURL(renderer_id, *url)) {
    // If this renderer is not permitted to request this URL, we invalidate the
    // URL.  This prevents us from storing the blocked URL and becoming confused
    // later.
    VLOG(1) << "Blocked URL " << url->spec();
    *url = GURL(chrome::kAboutBlankURL);
  }
}

void RenderViewHostImpl::SetAltErrorPageURL(const GURL& url) {
  Send(new ViewMsg_SetAltErrorPageURL(GetRoutingID(), url));
}

void RenderViewHostImpl::ExitFullscreen() {
  RejectMouseLockOrUnlockIfNecessary();
  // We need to notify the contents that its fullscreen state has changed. This
  // is done as part of the resize message.
  WasResized();
}

void RenderViewHostImpl::UpdateWebkitPreferences(
    const webkit_glue::WebPreferences& prefs) {
  Send(new ViewMsg_UpdateWebPreferences(GetRoutingID(), prefs));
}

void RenderViewHostImpl::ClearFocusedNode() {
  Send(new ViewMsg_ClearFocusedNode(GetRoutingID()));
}

void RenderViewHostImpl::SetZoomLevel(double level) {
  Send(new ViewMsg_SetZoomLevel(GetRoutingID(), level));
}

void RenderViewHostImpl::Zoom(content::PageZoom zoom) {
  Send(new ViewMsg_Zoom(GetRoutingID(), zoom));
}

void RenderViewHostImpl::ReloadFrame() {
  Send(new ViewMsg_ReloadFrame(GetRoutingID()));
}

void RenderViewHostImpl::Find(int request_id,
                              const string16& search_text,
                              const WebKit::WebFindOptions& options) {
  Send(new ViewMsg_Find(GetRoutingID(), request_id, search_text, options));
}

void RenderViewHostImpl::InsertCSS(const string16& frame_xpath,
                                   const std::string& css) {
  Send(new ViewMsg_CSSInsertRequest(GetRoutingID(), frame_xpath, css));
}

void RenderViewHostImpl::DisableScrollbarsForThreshold(const gfx::Size& size) {
  Send(new ViewMsg_DisableScrollbarsForSmallWindows(GetRoutingID(), size));
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  Send(new ViewMsg_EnablePreferredSizeChangedMode(GetRoutingID()));
}

void RenderViewHostImpl::EnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  SetShouldAutoResize(true);
  Send(new ViewMsg_EnableAutoResize(GetRoutingID(), min_size, max_size));
}

void RenderViewHostImpl::DisableAutoResize(const gfx::Size& new_size) {
  SetShouldAutoResize(false);
  Send(new ViewMsg_DisableAutoResize(GetRoutingID(), new_size));
}

void RenderViewHostImpl::ExecuteCustomContextMenuCommand(
    int action, const content::CustomContextMenuContext& context) {
  Send(new ViewMsg_CustomContextMenuAction(GetRoutingID(), context, action));
}

void RenderViewHostImpl::NotifyContextMenuClosed(
    const content::CustomContextMenuContext& context) {
  Send(new ViewMsg_ContextMenuClosed(GetRoutingID(), context));
}

void RenderViewHostImpl::CopyImageAt(int x, int y) {
  Send(new ViewMsg_CopyImageAt(GetRoutingID(), x, y));
}

void RenderViewHostImpl::ExecuteMediaPlayerActionAtLocation(
  const gfx::Point& location, const WebKit::WebMediaPlayerAction& action) {
  Send(new ViewMsg_MediaPlayerActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::ExecutePluginActionAtLocation(
  const gfx::Point& location, const WebKit::WebPluginAction& action) {
  Send(new ViewMsg_PluginActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::DisassociateFromPopupCount() {
  Send(new ViewMsg_DisassociateFromPopupCount(GetRoutingID()));
}

void RenderViewHostImpl::NotifyMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(GetRoutingID()));
}

void RenderViewHostImpl::StopFinding(content::StopFindAction action) {
  Send(new ViewMsg_StopFinding(GetRoutingID(), action));
}

content::SessionStorageNamespace*
RenderViewHostImpl::GetSessionStorageNamespace() {
  return session_storage_namespace_.get();
}

void RenderViewHostImpl::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
  if (view_ && !is_swapped_out_)
    view_->OnAccessibilityNotifications(params);

  if (!params.empty()) {
    for (unsigned i = 0; i < params.size(); i++) {
      const AccessibilityHostMsg_NotificationParams& param = params[i];

      if ((param.notification_type == AccessibilityNotificationLayoutComplete ||
           param.notification_type == AccessibilityNotificationLoadComplete) &&
          save_accessibility_tree_for_testing_) {
        accessibility_tree_ = param.acc_tree;

        // Only notify for non-blank pages.
        if (accessibility_tree_.children.size() > 0)
          content::NotificationService::current()->Notify(
              content::NOTIFICATION_RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,
              content::Source<RenderViewHost>(this),
              content::NotificationService::NoDetails());
      }
    }
  }

  if (send_accessibility_updated_notifications_) {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,
        content::Source<RenderViewHost>(this),
        content::NotificationService::NoDetails());
  }

  Send(new AccessibilityMsg_Notifications_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnScriptEvalResponse(int id, const ListValue& result) {
  Value* result_value;
  if (!result.Get(0, &result_value)) {
    // Programming error or rogue renderer.
    NOTREACHED() << "Got bad arguments for OnScriptEvalResponse";
    return;
  }
  std::pair<int, Value*> details(id, result_value);
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,
      content::Source<RenderViewHost>(this),
      content::Details<std::pair<int, Value*> >(&details));
}

void RenderViewHostImpl::OnDidZoomURL(double zoom_level,
                                      bool remember,
                                      const GURL& url) {
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(GetProcess()->GetBrowserContext()));
  if (remember) {
    host_zoom_map->SetZoomLevel(net::GetHostOrSpecFromURL(url), zoom_level);
  } else {
    host_zoom_map->SetTemporaryZoomLevel(
        GetProcess()->GetID(), GetRoutingID(), zoom_level);
  }
}

void RenderViewHostImpl::OnMediaNotification(int64 player_cookie,
                                             bool has_video,
                                             bool has_audio,
                                             bool is_playing) {
  if (is_playing) {
    PowerSaveBlocker* blocker = NULL;
    if (has_video) {
      blocker = new PowerSaveBlocker(
          PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep);
    } else if (has_audio) {
      blocker = new PowerSaveBlocker(
          PowerSaveBlocker::kPowerSaveBlockPreventSystemSleep);
    }

    if (blocker)
      power_save_blockers_[player_cookie] = blocker;
  } else {
    delete power_save_blockers_[player_cookie];
    power_save_blockers_.erase(player_cookie);
  }
}

void RenderViewHostImpl::OnRequestDesktopNotificationPermission(
    const GURL& source_origin, int callback_context) {
  content::GetContentClient()->browser()->RequestDesktopNotificationPermission(
      source_origin, callback_context, GetProcess()->GetID(), GetRoutingID());
}

void RenderViewHostImpl::OnShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params) {
  // Disallow HTML notifications from javascript: and file: schemes as this
  // allows unwanted cross-domain access.
  GURL url = params.contents_url;
  if (params.is_html &&
      (url.SchemeIs(chrome::kJavaScriptScheme) ||
       url.SchemeIs(chrome::kFileScheme))) {
    return;
  }

  content::GetContentClient()->browser()->ShowDesktopNotification(
      params, GetProcess()->GetID(), GetRoutingID(), false);
}

void RenderViewHostImpl::OnCancelDesktopNotification(int notification_id) {
  content::GetContentClient()->browser()->CancelDesktopNotification(
      GetProcess()->GetID(), GetRoutingID(), notification_id);
}

#if defined(OS_MACOSX)
void RenderViewHostImpl::OnMsgShowPopup(
    const ViewHostMsg_ShowPopup_Params& params) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    view->ShowPopupMenu(params.bounds,
                        params.item_height,
                        params.item_font_size,
                        params.selected_item,
                        params.popup_items,
                        params.right_aligned);
  }
}
#endif

void RenderViewHostImpl::OnRunFileChooser(
    const content::FileChooserParams& params) {
  delegate_->RunFileChooser(this, params);
}

void RenderViewHostImpl::OnDomOperationResponse(
    const std::string& json_string, int automation_id) {
  DomOperationNotificationDetails details(json_string, automation_id);
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_DOM_OPERATION_RESPONSE,
      content::Source<RenderViewHost>(this),
      content::Details<DomOperationNotificationDetails>(&details));
}

void RenderViewHostImpl::SetSwappedOut(bool is_swapped_out) {
  is_swapped_out_ = is_swapped_out;

  // Whenever we change swap out state, we should not be waiting for
  // beforeunload or unload acks.  We clear them here to be safe, since they
  // can cause navigations to be ignored in OnMsgNavigate.
  is_waiting_for_beforeunload_ack_ = false;
  is_waiting_for_unload_ack_ = false;
}

void RenderViewHostImpl::ClearPowerSaveBlockers() {
  STLDeleteValues(&power_save_blockers_);
}

}  // namespace content
