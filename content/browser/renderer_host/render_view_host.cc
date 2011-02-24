// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_apps.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/site_instance.h"
#include "net/base/net_util.h"
#include "printing/native_metafile.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webdropdata.h"

using base::TimeDelta;
using WebKit::WebConsoleMessage;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationsMask;
using WebKit::WebFindOptions;
using WebKit::WebInputEvent;
using WebKit::WebMediaPlayerAction;
using WebKit::WebTextDirection;

namespace {

// Delay to wait on closing the tab for a beforeunload/unload handler to fire.
const int kUnloadTimeoutMS = 1000;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id);
  if (!process)
    return NULL;
  RenderWidgetHost* widget = static_cast<RenderWidgetHost*>(
      process->GetListenerByID(render_view_id));
  if (!widget || !widget->IsRenderView())
    return NULL;
  return static_cast<RenderViewHost*>(widget);
}

RenderViewHost::RenderViewHost(SiteInstance* instance,
                               RenderViewHostDelegate* delegate,
                               int routing_id,
                               SessionStorageNamespace* session_storage)
    : RenderWidgetHost(instance->GetProcess(), routing_id),
      instance_(instance),
      delegate_(delegate),
      waiting_for_drag_context_response_(false),
      enabled_bindings_(0),
      pending_request_id_(0),
      navigations_suspended_(false),
      suspended_nav_message_(NULL),
      run_modal_reply_msg_(NULL),
      is_waiting_for_beforeunload_ack_(false),
      is_waiting_for_unload_ack_(false),
      unload_ack_is_for_cross_site_transition_(false),
      are_javascript_messages_suppressed_(false),
      sudden_termination_allowed_(false),
      session_storage_namespace_(session_storage),
      is_extension_process_(false),
      save_accessibility_tree_for_testing_(false),
      render_view_termination_status_(base::TERMINATION_STATUS_STILL_RUNNING) {
  if (!session_storage_namespace_) {
    session_storage_namespace_ =
        new SessionStorageNamespace(process()->profile());
  }

  DCHECK(instance_);
  DCHECK(delegate_);
}

RenderViewHost::~RenderViewHost() {
  delegate()->RenderViewDeleted(this);

  // Be sure to clean up any leftover state from cross-site requests.
  CrossSiteRequestManager::GetInstance()->SetHasPendingCrossSiteRequest(
      process()->id(), routing_id(), false);
}

bool RenderViewHost::CreateRenderView(const string16& frame_name) {
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!process()->Init(renderer_accessible(), is_extension_process_))
    return false;
  DCHECK(process()->HasConnection());
  DCHECK(process()->profile());

  if (BindingsPolicy::is_web_ui_enabled(enabled_bindings_)) {
    ChildProcessSecurityPolicy::GetInstance()->GrantWebUIBindings(
        process()->id());
  }

  if (BindingsPolicy::is_extension_enabled(enabled_bindings_)) {
    ChildProcessSecurityPolicy::GetInstance()->GrantExtensionBindings(
        process()->id());

    // Extensions may have permission to access chrome:// URLs.
    ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
        process()->id(), chrome::kChromeUIScheme);
  }

  renderer_initialized_ = true;

  ViewMsg_New_Params params;
  params.parent_window = GetNativeViewId();
  params.renderer_preferences =
      delegate_->GetRendererPrefs(process()->profile());
  params.web_preferences = delegate_->GetWebkitPrefs();
  params.view_id = routing_id();
  params.session_storage_namespace_id = session_storage_namespace_->id();
  params.frame_name = frame_name;
  Send(new ViewMsg_New(params));

  // Set the alternate error page, which is profile specific, in the renderer.
  GURL url = delegate_->GetAlternateErrorPageURL();
  SetAlternateErrorPageURL(url);

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  Send(new ViewMsg_AllowBindings(routing_id(), enabled_bindings_));
  UpdateBrowserWindowId(delegate_->GetBrowserWindowID());
  Send(new ViewMsg_NotifyRenderViewType(routing_id(),
                                        delegate_->GetRenderViewType()));
  // Let our delegate know that we created a RenderView.
  delegate_->RenderViewCreated(this);
  process()->ViewCreated();

  return true;
}

bool RenderViewHost::IsRenderViewLive() const {
  return process()->HasConnection() && renderer_initialized_;
}

void RenderViewHost::SyncRendererPrefs() {
  Send(new ViewMsg_SetRendererPrefs(routing_id(),
                                    delegate_->GetRendererPrefs(
                                        process()->profile())));
}

void RenderViewHost::Navigate(const ViewMsg_Navigate_Params& params) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      process()->id(), params.url);

  ViewMsg_Navigate* nav_message = new ViewMsg_Navigate(routing_id(), params);

  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (navigations_suspended_) {
    // Shouldn't be possible to have a second navigation while suspended, since
    // navigations will only be suspended during a cross-site request.  If a
    // second navigation occurs, TabContents will cancel this pending RVH
    // create a new pending RVH.
    DCHECK(!suspended_nav_message_.get());
    suspended_nav_message_.reset(nav_message);
  } else {
    // Unset this, otherwise if true and the hang monitor fires we'll
    // incorrectly close the tab.
    is_waiting_for_unload_ack_ = false;

    Send(nav_message);

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
  }
  const GURL& url = params.url;
  if (!delegate_->IsExternalTabContainer() &&
      (url.SchemeIs("http") || url.SchemeIs("https")))
    chrome_browser_net::PreconnectUrlAndSubresources(url);
}

void RenderViewHost::NavigateToURL(const GURL& url) {
  ViewMsg_Navigate_Params params;
  params.page_id = -1;
  params.url = url;
  params.transition = PageTransition::LINK;
  params.navigation_type = ViewMsg_Navigate_Params::NORMAL;
  Navigate(params);
}

void RenderViewHost::SetNavigationsSuspended(bool suspend) {
  // This should only be called to toggle the state.
  DCHECK(navigations_suspended_ != suspend);

  navigations_suspended_ = suspend;
  if (!suspend && suspended_nav_message_.get()) {
    // There's a navigation message waiting to be sent.  Now that we're not
    // suspended anymore, resume navigation by sending it.
    Send(suspended_nav_message_.release());
  }
}

void RenderViewHost::FirePageBeforeUnload(bool for_cross_site_transition) {
  if (!IsRenderViewLive()) {
    // This RenderViewHost doesn't have a live renderer, so just skip running
    // the onbeforeunload handler.
    is_waiting_for_beforeunload_ack_ = true;  // Checked by OnMsgShouldCloseACK.
    unload_ack_is_for_cross_site_transition_ = for_cross_site_transition;
    OnMsgShouldCloseACK(true);
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
    Send(new ViewMsg_ShouldClose(routing_id()));
  }
}

void RenderViewHost::ClosePage(bool for_cross_site_transition,
                               int new_render_process_host_id,
                               int new_request_id) {
  // In most cases, this will not be set to false afterward.  Either the tab
  // will be closed, or a pending RenderViewHost will replace this one.
  is_waiting_for_unload_ack_ = true;
  // Start the hang monitor in case the renderer hangs in the unload handler.
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  ViewMsg_ClosePage_Params params;
  params.closing_process_id = process()->id();
  params.closing_route_id = routing_id();
  params.for_cross_site_transition = for_cross_site_transition;
  params.new_render_process_host_id = new_render_process_host_id;
  params.new_request_id = new_request_id;
  if (IsRenderViewLive()) {
    NotificationService::current()->Notify(
        NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        Source<RenderViewHost>(this),
        NotificationService::NoDetails());

    Send(new ViewMsg_ClosePage(routing_id(), params));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip closing
    // the page.  We must notify the ResourceDispatcherHost on the IO thread,
    // which we will do through the RenderProcessHost's widget helper.
    process()->CrossSiteClosePageACK(params);
  }
}

void RenderViewHost::ClosePageIgnoringUnloadEvents() {
  StopHangMonitorTimeout();
  is_waiting_for_beforeunload_ack_ = false;
  is_waiting_for_unload_ack_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

void RenderViewHost::SetHasPendingCrossSiteRequest(bool has_pending_request,
                                                   int request_id) {
  CrossSiteRequestManager::GetInstance()->SetHasPendingCrossSiteRequest(
      process()->id(), routing_id(), has_pending_request);
  pending_request_id_ = request_id;
}

int RenderViewHost::GetPendingRequestId() {
  return pending_request_id_;
}

RenderViewHost::CommandState RenderViewHost::GetStateForCommand(
    RenderViewCommand command) const {
  if (command != RENDER_VIEW_COMMAND_TOGGLE_SPELL_CHECK)
    LOG(DFATAL) << "Unknown command " << command;

  std::map<RenderViewCommand, CommandState>::const_iterator it =
      command_states_.find(command);
  if (it == command_states_.end()) {
    CommandState state;
    state.is_enabled = false;
    state.checked_state = RENDER_VIEW_COMMAND_CHECKED_STATE_UNCHECKED;
    return state;
  }
  return it->second;
}

void RenderViewHost::Stop() {
  Send(new ViewMsg_Stop(routing_id()));
}

void RenderViewHost::ReloadFrame() {
  Send(new ViewMsg_ReloadFrame(routing_id()));
}

bool RenderViewHost::PrintPages() {
  return Send(new ViewMsg_PrintPages(routing_id()));
}

bool RenderViewHost::PrintPreview() {
  return Send(new ViewMsg_PrintPreview(routing_id()));
}

void RenderViewHost::PrintingDone(int document_cookie, bool success) {
  Send(new ViewMsg_PrintingDone(routing_id(), document_cookie, success));
}

void RenderViewHost::StartFinding(int request_id,
                                  const string16& search_text,
                                  bool forward,
                                  bool match_case,
                                  bool find_next) {
  if (search_text.empty())
    return;

  WebFindOptions options;
  options.forward = forward;
  options.matchCase = match_case;
  options.findNext = find_next;
  Send(new ViewMsg_Find(routing_id(), request_id, search_text, options));

  // This call is asynchronous and returns immediately.
  // The result of the search is sent as a notification message by the renderer.
}

void RenderViewHost::StopFinding(
    FindBarController::SelectionAction selection_action) {
  ViewMsg_StopFinding_Params params;

  switch (selection_action) {
    case FindBarController::kClearSelection:
      params.action = ViewMsg_StopFinding_Params::kClearSelection;
      break;
    case FindBarController::kKeepSelection:
      params.action = ViewMsg_StopFinding_Params::kKeepSelection;
      break;
    case FindBarController::kActivateSelection:
      params.action = ViewMsg_StopFinding_Params::kActivateSelection;
      break;
    default:
      NOTREACHED();
      params.action = ViewMsg_StopFinding_Params::kKeepSelection;
  }
  Send(new ViewMsg_StopFinding(routing_id(), params));
}

void RenderViewHost::Zoom(PageZoom::Function function) {
  Send(new ViewMsg_Zoom(routing_id(), function));
}

void RenderViewHost::SetZoomLevel(double zoom_level) {
  Send(new ViewMsg_SetZoomLevel(routing_id(), zoom_level));
}

void RenderViewHost::SetPageEncoding(const std::string& encoding_name) {
  Send(new ViewMsg_SetPageEncoding(routing_id(), encoding_name));
}

void RenderViewHost::ResetPageEncodingToDefault() {
  Send(new ViewMsg_ResetPageEncodingToDefault(routing_id()));
}

void RenderViewHost::SetAlternateErrorPageURL(const GURL& url) {
  Send(new ViewMsg_SetAltErrorPageURL(routing_id(), url));
}

void RenderViewHost::DragTargetDragEnter(
    const WebDropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed) {
  // Grant the renderer the ability to load the drop_data.
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  policy->GrantRequestURL(process()->id(), drop_data.url);
  for (std::vector<string16>::const_iterator iter(drop_data.filenames.begin());
       iter != drop_data.filenames.end(); ++iter) {
    FilePath path = FilePath::FromWStringHack(UTF16ToWideHack(*iter));
    policy->GrantRequestURL(process()->id(),
                            net::FilePathToFileURL(path));
    policy->GrantReadFile(process()->id(), path);
  }
  Send(new ViewMsg_DragTargetDragEnter(routing_id(), drop_data, client_pt,
                                       screen_pt, operations_allowed));
}

void RenderViewHost::DragTargetDragOver(
    const gfx::Point& client_pt, const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed) {
  Send(new ViewMsg_DragTargetDragOver(routing_id(), client_pt, screen_pt,
                                      operations_allowed));
}

void RenderViewHost::DragTargetDragLeave() {
  Send(new ViewMsg_DragTargetDragLeave(routing_id()));
}

void RenderViewHost::DragTargetDrop(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  Send(new ViewMsg_DragTargetDrop(routing_id(), client_pt, screen_pt));
}

void RenderViewHost::ReservePageIDRange(int size) {
  Send(new ViewMsg_ReservePageIDRange(routing_id(), size));
}

void RenderViewHost::ExecuteJavascriptInWebFrame(
    const string16& frame_xpath,
    const string16& jscript) {
  Send(new ViewMsg_ScriptEvalRequest(routing_id(), frame_xpath, jscript,
                                     0, false));
}

int RenderViewHost::ExecuteJavascriptInWebFrameNotifyResult(
    const string16& frame_xpath,
    const string16& jscript) {
  static int next_id = 1;
  Send(new ViewMsg_ScriptEvalRequest(routing_id(), frame_xpath, jscript,
                                     next_id, true));
  return next_id++;
}

void RenderViewHost::InsertCSSInWebFrame(
    const std::wstring& frame_xpath,
    const std::string& css,
    const std::string& id) {
  Send(new ViewMsg_CSSInsertRequest(routing_id(), frame_xpath, css, id));
}

void RenderViewHost::AddMessageToConsole(
    const string16& frame_xpath,
    const string16& message,
    const WebConsoleMessage::Level& level) {
  Send(new ViewMsg_AddMessageToConsole(
      routing_id(), frame_xpath, message, level));
}

void RenderViewHost::Undo() {
  Send(new ViewMsg_Undo(routing_id()));
}

void RenderViewHost::Redo() {
  Send(new ViewMsg_Redo(routing_id()));
}

void RenderViewHost::Cut() {
  Send(new ViewMsg_Cut(routing_id()));
}

void RenderViewHost::Copy() {
  Send(new ViewMsg_Copy(routing_id()));
}

void RenderViewHost::CopyToFindPboard() {
#if defined(OS_MACOSX)
  // Windows/Linux don't have the concept of a find pasteboard.
  Send(new ViewMsg_CopyToFindPboard(routing_id()));
#endif
}

void RenderViewHost::Paste() {
  Send(new ViewMsg_Paste(routing_id()));
}

void RenderViewHost::ToggleSpellCheck() {
  Send(new ViewMsg_ToggleSpellCheck(routing_id()));
}

void RenderViewHost::Delete() {
  Send(new ViewMsg_Delete(routing_id()));
}

void RenderViewHost::SelectAll() {
  Send(new ViewMsg_SelectAll(routing_id()));
}

void RenderViewHost::ToggleSpellPanel(bool is_currently_visible) {
  Send(new ViewMsg_ToggleSpellPanel(routing_id(), is_currently_visible));
}

int RenderViewHost::DownloadFavIcon(const GURL& url, int image_size) {
  if (!url.is_valid()) {
    NOTREACHED();
    return 0;
  }
  static int next_id = 1;
  int id = next_id++;
  Send(new ViewMsg_DownloadFavIcon(routing_id(), id, url, image_size));
  return id;
}

void RenderViewHost::GetApplicationInfo(int32 page_id) {
  Send(new ViewMsg_GetApplicationInfo(routing_id(), page_id));
}

void RenderViewHost::CaptureThumbnail() {
  Send(new ViewMsg_CaptureThumbnail(routing_id()));
}

void RenderViewHost::CaptureSnapshot() {
  Send(new ViewMsg_CaptureSnapshot(routing_id()));
}

void RenderViewHost::JavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                                bool success,
                                                const std::wstring& prompt) {
  process()->set_ignore_input_events(false);
  bool is_waiting =
      is_waiting_for_beforeunload_ack_ || is_waiting_for_unload_ack_;
  if (is_waiting)
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  ViewHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg,
                                                     success, prompt);
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

void RenderViewHost::ModalHTMLDialogClosed(IPC::Message* reply_msg,
                                           const std::string& json_retval) {
  if (is_waiting_for_beforeunload_ack_ || is_waiting_for_unload_ack_)
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  ViewHostMsg_ShowModalHTMLDialog::WriteReplyParams(reply_msg, json_retval);
  Send(reply_msg);
}

void RenderViewHost::CopyImageAt(int x, int y) {
  Send(new ViewMsg_CopyImageAt(routing_id(), x, y));
}

void RenderViewHost::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y,
    WebDragOperation operation) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      true, operation));
}

void RenderViewHost::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      false, WebDragOperationNone));
}

void RenderViewHost::DragSourceSystemDragEnded() {
  Send(new ViewMsg_DragSourceSystemDragEnded(routing_id()));
}

void RenderViewHost::AllowBindings(int bindings_flags) {
  DCHECK(!renderer_initialized_);
  enabled_bindings_ |= bindings_flags;
}

void RenderViewHost::SetWebUIProperty(const std::string& name,
                                      const std::string& value) {
  DCHECK(BindingsPolicy::is_web_ui_enabled(enabled_bindings_));
  Send(new ViewMsg_SetWebUIProperty(routing_id(), name, value));
}

void RenderViewHost::GotFocus() {
  RenderWidgetHost::GotFocus();  // Notifies the renderer it got focus.

  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->GotFocus();
}

void RenderViewHost::LostCapture() {
  RenderWidgetHost::LostCapture();

  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->LostCapture();
}

void RenderViewHost::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(routing_id(), reverse));
}

void RenderViewHost::ClearFocusedNode() {
  Send(new ViewMsg_ClearFocusedNode(routing_id()));
}

void RenderViewHost::ScrollFocusedEditableNodeIntoView() {
  Send(new ViewMsg_ScrollFocusedEditableNodeIntoView(routing_id()));
}

void RenderViewHost::UpdateWebPreferences(const WebPreferences& prefs) {
  Send(new ViewMsg_UpdateWebPreferences(routing_id(), prefs));
}

void RenderViewHost::InstallMissingPlugin() {
  Send(new ViewMsg_InstallMissingPlugin(routing_id()));
}

void RenderViewHost::LoadBlockedPlugins() {
  Send(new ViewMsg_LoadBlockedPlugins(routing_id()));
}

void RenderViewHost::FilesSelectedInChooser(
    const std::vector<FilePath>& files) {
  // Grant the security access requested to the given files.
  for (std::vector<FilePath>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
        process()->id(), *file);
  }
  Send(new ViewMsg_RunFileChooserResponse(routing_id(), files));
}

void RenderViewHost::LoadStateChanged(const GURL& url,
                                      net::LoadState load_state,
                                      uint64 upload_position,
                                      uint64 upload_size) {
  delegate_->LoadStateChanged(url, load_state, upload_position, upload_size);
}

bool RenderViewHost::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_ || process()->sudden_termination_allowed();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, IPC message handlers:

bool RenderViewHost::OnMessageReceived(const IPC::Message& msg) {
#if defined(OS_WIN)
  // On Windows there's a potential deadlock with sync messsages going in
  // a circle from browser -> plugin -> renderer -> browser.
  // On Linux we can avoid this by avoiding sync messages from browser->plugin.
  // On Mac we avoid this by not supporting windowed plugins.
  if (msg.is_sync() && !msg.is_caller_pumping_messages()) {
    // NOTE: IF YOU HIT THIS ASSERT, THE SOLUTION IS ALMOST NEVER TO RUN A
    // NESTED MESSAGE LOOP IN THE RENDERER!!!
    // That introduces reentrancy which causes hard to track bugs.  You should
    // find a way to either turn this into an asynchronous message, or one
    // that can be answered on the IO thread.
    NOTREACHED() << "Can't send sync messages to UI thread without pumping "
        "messages in the renderer or else deadlocks can occur if the page "
        "has windowed plugins! (message type " << msg.type() << ")";
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
    return true;
  }
#endif

  if (delegate_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderViewHost, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnMsgShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnMsgShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowFullscreenWidget,
                        OnMsgShowFullscreenWidget)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunModal, OnMsgRunModal)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnMsgRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnMsgRenderViewGone)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_FrameNavigate, OnMsgNavigate(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnMsgUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTitle, OnMsgUpdateTitle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateEncoding, OnMsgUpdateEncoding)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnMsgUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Thumbnail, OnMsgThumbnail)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Snapshot, OnMsgScreenshot)
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
    IPC_MESSAGE_HANDLER(ViewMsg_ExecuteCodeFinished,
                        OnExecuteCodeFinished)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ContextMenu, OnMsgContextMenu)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenURL, OnMsgOpenURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredSizeChange,
                        OnMsgDidContentsPreferredSizeChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DomOperationResponse,
                        OnMsgDomOperationResponse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WebUISend, OnMsgWebUISend)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardMessageToExternalHost,
                        OnMsgForwardMessageToExternalHost)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetTooltipText, OnMsgSetTooltipText)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunJavaScriptMessage,
                                    OnMsgRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunBeforeUnloadConfirm,
                                    OnMsgRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ShowModalHTMLDialog,
                                    OnMsgShowModalHTMLDialog)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartDragging, OnMsgStartDragging)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToDevToolsAgent,
                        OnForwardToDevToolsAgent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToDevToolsClient,
                        OnForwardToDevToolsClient)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ActivateDevToolsWindow,
                        OnActivateDevToolsWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseDevToolsWindow,
                        OnCloseDevToolsWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestDockDevToolsWindow,
                        OnRequestDockDevToolsWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestUndockDevToolsWindow,
                        OnRequestUndockDevToolsWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DevToolsRuntimePropertyChanged,
                        OnDevToolsRuntimePropertyChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShouldClose_ACK, OnMsgShouldCloseACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionRequest, OnExtensionRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionChanged, OnMsgSelectionChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionPostMessage,
                        OnExtensionPostMessage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AccessibilityNotifications,
                        OnAccessibilityNotifications)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnCSSInserted, OnCSSInserted)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AppCacheAccessed, OnAppCacheAccessed)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WebDatabaseAccessed, OnWebDatabaseAccessed)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnMsgFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateZoomLimits, OnUpdateZoomLimits)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ScriptEvalResponse, OnScriptEvalResponse)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnMsgShowPopup)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_CommandStateChanged,
                        OnCommandStateChanged)
    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = RenderWidgetHost::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message had a handler, but its de-serialization failed.
    // Kill the renderer.
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RVH"));
    process()->ReceivedBadMessage();
  }

  return handled;
}

void RenderViewHost::Shutdown() {
  // If we are being run modally (see RunModal), then we need to cleanup.
  if (run_modal_reply_msg_) {
    Send(run_modal_reply_msg_);
    run_modal_reply_msg_ = NULL;
  }

  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (devtools_manager)  // NULL in tests
    devtools_manager->UnregisterDevToolsClientHostFor(this);

  RenderWidgetHost::Shutdown();
}

bool RenderViewHost::IsRenderView() const {
  return true;
}

void RenderViewHost::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  view->CreateNewWindow(route_id, params);
}

void RenderViewHost::CreateNewWidget(int route_id,
                                     WebKit::WebPopupType popup_type) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->CreateNewWidget(route_id, popup_type);
}

void RenderViewHost::CreateNewFullscreenWidget(int route_id) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->CreateNewFullscreenWidget(route_id);
}

void RenderViewHost::OnMsgShowView(int route_id,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    view->ShowCreatedWindow(route_id, disposition, initial_pos, user_gesture);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHost::OnMsgShowWidget(int route_id,
                                     const gfx::Rect& initial_pos) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    view->ShowCreatedWidget(route_id, initial_pos);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHost::OnMsgShowFullscreenWidget(int route_id) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    view->ShowCreatedFullscreenWidget(route_id);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHost::OnMsgRunModal(IPC::Message* reply_msg) {
  DCHECK(!run_modal_reply_msg_);
  run_modal_reply_msg_ = reply_msg;

  // TODO(darin): Bug 1107929: Need to inform our delegate to show this view in
  // an app-modal fashion.
}

void RenderViewHost::OnMsgRenderViewReady() {
  render_view_termination_status_ = base::TERMINATION_STATUS_STILL_RUNNING;
  WasResized();
  delegate_->RenderViewReady(this);
}

void RenderViewHost::OnMsgRenderViewGone(int status, int exit_code) {
  // Keep the termination status so we can get at it later when we
  // need to know why it died.
  render_view_termination_status_ =
      static_cast<base::TerminationStatus>(status);

  // Our base class RenderWidgetHost needs to reset some stuff.
  RendererExited(render_view_termination_status_, exit_code);

  delegate_->RenderViewGone(this,
                            static_cast<base::TerminationStatus>(status),
                            exit_code);
}

// Called when the renderer navigates.  For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
//
// Subframes are identified by the page transition type.  For subframes loaded
// as part of a wider page load, the page_id will be the same as for the top
// level frame.  If the user explicitly requests a subframe navigation, we will
// get a new page_id because we need to create a new navigation entry for that
// action.
void RenderViewHost::OnMsgNavigate(const IPC::Message& msg) {
  // Read the parameters out of the IPC message directly to avoid making another
  // copy when we filter the URLs.
  void* iter = NULL;
  ViewHostMsg_FrameNavigate_Params validated_params;
  if (!IPC::ParamTraits<ViewHostMsg_FrameNavigate_Params>::
      Read(&msg, &iter, &validated_params))
    return;

  // If we're waiting for a beforeunload ack from this renderer and we receive
  // a Navigate message from the main frame, then the renderer was navigating
  // before it received the request.  If it is during a cross-site navigation,
  // then we should forget about the beforeunload, because the navigation will
  // now be canceled.  (If it is instead during an attempt to close the page,
  // we should be sure to keep waiting for the ack, which the new page will
  // send.)
  //
  // If we did not clear this state, an unresponsiveness timer might think we
  // are waiting for an ack but are not in a cross-site navigation, and would
  // close the tab.  TODO(creis): That timer code should be refactored to only
  // close the tab if we explicitly know the user tried to close the tab, and
  // not just check for the absence of a cross-site navigation.  Once that's
  // fixed, this check can go away.
  if (is_waiting_for_beforeunload_ack_ &&
      unload_ack_is_for_cross_site_transition_ &&
      PageTransition::IsMainFrame(validated_params.transition)) {
    is_waiting_for_beforeunload_ack_ = false;
    StopHangMonitorTimeout();
  }

  // If we're waiting for an unload ack from this renderer and we receive a
  // Navigate message, then the renderer was navigating before it received the
  // unload request.  It will either respond to the unload request soon or our
  // timer will expire.  Either way, we should ignore this message, because we
  // have already committed to closing this renderer.
  if (is_waiting_for_unload_ack_)
    return;

  const int renderer_id = process()->id();
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  FilterURL(policy, renderer_id, &validated_params.url);
  FilterURL(policy, renderer_id, &validated_params.referrer);
  for (std::vector<GURL>::iterator it(validated_params.redirects.begin());
      it != validated_params.redirects.end(); ++it) {
    FilterURL(policy, renderer_id, &(*it));
  }
  FilterURL(policy, renderer_id, &validated_params.searchable_form_url);
  FilterURL(policy, renderer_id, &validated_params.password_form.origin);
  FilterURL(policy, renderer_id, &validated_params.password_form.action);

  delegate_->DidNavigate(this, validated_params);
}

void RenderViewHost::OnMsgUpdateState(int32 page_id,
                                      const std::string& state) {
  delegate_->UpdateState(this, page_id, state);
}

void RenderViewHost::OnMsgUpdateTitle(int32 page_id,
                                      const std::wstring& title) {
  if (title.length() > chrome::kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }
  delegate_->UpdateTitle(this, page_id, title);
}

void RenderViewHost::OnMsgUpdateEncoding(const std::string& encoding_name) {
  delegate_->UpdateEncoding(this, encoding_name);
}

void RenderViewHost::OnMsgUpdateTargetURL(int32 page_id,
                                          const GURL& url) {
  delegate_->UpdateTargetURL(page_id, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(routing_id()));
}

void RenderViewHost::OnMsgThumbnail(const GURL& url,
                                    const ThumbnailScore& score,
                                    const SkBitmap& bitmap) {
  delegate_->UpdateThumbnail(url, bitmap, score);
}

void RenderViewHost::OnMsgScreenshot(const SkBitmap& bitmap) {
  NotificationService::current()->Notify(
      NotificationType::TAB_SNAPSHOT_TAKEN,
      Source<RenderViewHost>(this),
      Details<const SkBitmap>(&bitmap));
}

void RenderViewHost::OnUpdateInspectorSetting(
    const std::string& key, const std::string& value) {
  delegate_->UpdateInspectorSetting(key, value);
}

void RenderViewHost::OnMsgClose() {
  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHost::OnMsgRequestMove(const gfx::Rect& pos) {
  delegate_->RequestMove(pos);
  Send(new ViewMsg_Move_ACK(routing_id()));
}

void RenderViewHost::OnMsgDidStartLoading() {
  delegate_->DidStartLoading();
}

void RenderViewHost::OnMsgDidStopLoading() {
  delegate_->DidStopLoading();
}

void RenderViewHost::OnMsgDidChangeLoadProgress(double load_progress) {
  delegate_->DidChangeLoadProgress(load_progress);
}

void RenderViewHost::OnMsgDocumentAvailableInMainFrame() {
  delegate_->DocumentAvailableInMainFrame(this);
}

void RenderViewHost::OnMsgDocumentOnLoadCompletedInMainFrame(int32 page_id) {
  delegate_->DocumentOnLoadCompletedInMainFrame(this, page_id);
}

void RenderViewHost::OnExecuteCodeFinished(int request_id, bool success) {
  std::pair<int, bool> result_details(request_id, success);
  NotificationService::current()->Notify(
      NotificationType::TAB_CODE_EXECUTED,
      NotificationService::AllSources(),
      Details<std::pair<int, bool> >(&result_details));
}

void RenderViewHost::OnMsgContextMenu(const ContextMenuParams& params) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  int renderer_id = process()->id();
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  FilterURL(policy, renderer_id, &validated_params.link_url);
  FilterURL(policy, renderer_id, &validated_params.src_url);
  FilterURL(policy, renderer_id, &validated_params.page_url);
  FilterURL(policy, renderer_id, &validated_params.frame_url);

  view->ShowContextMenu(validated_params);
}

void RenderViewHost::OnMsgOpenURL(const GURL& url,
                                  const GURL& referrer,
                                  WindowOpenDisposition disposition) {
  GURL validated_url(url);
  FilterURL(ChildProcessSecurityPolicy::GetInstance(),
            process()->id(), &validated_url);

  delegate_->RequestOpenURL(validated_url, referrer, disposition);
}

void RenderViewHost::OnMsgDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;
  view->UpdatePreferredSize(new_size);
}

void RenderViewHost::OnMsgDomOperationResponse(
    const std::string& json_string, int automation_id) {
  delegate_->DomOperationResponse(json_string, automation_id);

  // We also fire a notification for more loosely-coupled use cases.
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NotificationType::DOM_OPERATION_RESPONSE, Source<RenderViewHost>(this),
      Details<DomOperationNotificationDetails>(&details));
}

void RenderViewHost::OnMsgWebUISend(
    const GURL& source_url, const std::string& message,
    const std::string& content) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasWebUIBindings(process()->id())) {
    NOTREACHED() << "Blocked unauthorized use of WebUIBindings.";
    return;
  }

  scoped_ptr<Value> value;
  if (!content.empty()) {
    value.reset(base::JSONReader::Read(content, false));
    if (!value.get() || !value->IsType(Value::TYPE_LIST)) {
      // The page sent us something that we didn't understand.
      // This probably indicates a programming error.
      NOTREACHED() << "Invalid JSON argument in OnMsgWebUISend.";
      return;
    }
  }

  ViewHostMsg_DomMessage_Params params;
  params.name = message;
  if (value.get())
    params.arguments.Swap(static_cast<ListValue*>(value.get()));
  params.source_url = source_url;
  // WebUI doesn't use these values yet.
  // TODO(aa): When WebUI is ported to ExtensionFunctionDispatcher, send real
  // values here.
  params.request_id = -1;
  params.has_callback = false;
  params.user_gesture = false;
  delegate_->ProcessWebUIMessage(params);
}

void RenderViewHost::OnMsgForwardMessageToExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  delegate_->ProcessExternalHostMessage(message, origin, target);
}

void RenderViewHost::DisassociateFromPopupCount() {
  Send(new ViewMsg_DisassociateFromPopupCount(routing_id()));
}

void RenderViewHost::AllowScriptToClose(bool script_can_close) {
  Send(new ViewMsg_AllowScriptToClose(routing_id(), script_can_close));
}

void RenderViewHost::OnMsgSetTooltipText(
    const std::wstring& tooltip_text,
    WebTextDirection text_direction_hint) {
  // First, add directionality marks around tooltip text if necessary.
  // A naive solution would be to simply always wrap the text. However, on
  // windows, Unicode directional embedding characters can't be displayed on
  // systems that lack RTL fonts and are instead displayed as empty squares.
  //
  // To get around this we only wrap the string when we deem it necessary i.e.
  // when the locale direction is different than the tooltip direction hint.
  //
  // Currently, we use element's directionality as the tooltip direction hint.
  // An alternate solution would be to set the overall directionality based on
  // trying to detect the directionality from the tooltip text rather than the
  // element direction.  One could argue that would be a preferable solution
  // but we use the current approach to match Fx & IE's behavior.
  std::wstring wrapped_tooltip_text = tooltip_text;
  if (!tooltip_text.empty()) {
    if (text_direction_hint == WebKit::WebTextDirectionLeftToRight) {
      // Force the tooltip to have LTR directionality.
      wrapped_tooltip_text = UTF16ToWide(
          base::i18n::GetDisplayStringInLTRDirectionality(
              WideToUTF16(wrapped_tooltip_text)));
    } else if (text_direction_hint == WebKit::WebTextDirectionRightToLeft &&
               !base::i18n::IsRTL()) {
      // Force the tooltip to have RTL directionality.
      base::i18n::WrapStringWithRTLFormatting(&wrapped_tooltip_text);
    }
  }
  if (view())
    view()->SetTooltipText(wrapped_tooltip_text);
}

void RenderViewHost::OnMsgSelectionChanged(const std::string& text) {
  if (view())
    view()->SelectionChanged(text);
}

void RenderViewHost::OnMsgRunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg) {
  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  process()->set_ignore_input_events(true);
  StopHangMonitorTimeout();
  delegate_->RunJavaScriptMessage(message, default_prompt, frame_url, flags,
                                  reply_msg,
                                  &are_javascript_messages_suppressed_);
}

void RenderViewHost::OnMsgRunBeforeUnloadConfirm(const GURL& frame_url,
                                                 const std::wstring& message,
                                                 IPC::Message* reply_msg) {
  // While a JS before unload dialog is showing, tabs in the same process
  // shouldn't process input events.
  process()->set_ignore_input_events(true);
  StopHangMonitorTimeout();
  delegate_->RunBeforeUnloadConfirm(message, reply_msg);
}

void RenderViewHost::OnMsgShowModalHTMLDialog(
    const GURL& url, int width, int height, const std::string& json_arguments,
    IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  delegate_->ShowModalHTMLDialog(url, width, height, json_arguments, reply_msg);
}

void RenderViewHost::MediaPlayerActionAt(const gfx::Point& location,
                                         const WebMediaPlayerAction& action) {
  // TODO(ajwong): Which thread should run this?  Does it matter?
  Send(new ViewMsg_MediaPlayerActionAt(routing_id(), location, action));
}

void RenderViewHost::ContextMenuClosed(
    const webkit_glue::CustomContextMenuContext& custom_context) {
  Send(new ViewMsg_ContextMenuClosed(routing_id(), custom_context));
}

void RenderViewHost::PrintNodeUnderContextMenu() {
  Send(new ViewMsg_PrintNodeUnderContextMenu(routing_id()));
}

void RenderViewHost::PrintForPrintPreview() {
  Send(new ViewMsg_PrintForPrintPreview(routing_id()));
}

void RenderViewHost::OnMsgStartDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask drag_operations_mask,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
      view->StartDragging(drop_data, drag_operations_mask, image, image_offset);
}

void RenderViewHost::OnUpdateDragCursor(WebDragOperation current_op) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->UpdateDragCursor(current_op);
}

void RenderViewHost::OnTakeFocus(bool reverse) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHost::OnAddMessageToConsole(const std::wstring& message,
                                           int32 line_no,
                                           const std::wstring& source_id) {
  std::wstring msg = StringPrintf(L"\"%ls,\" source: %ls (%d)", message.c_str(),
                                  source_id.c_str(), line_no);
  logging::LogMessage("CONSOLE", 0).stream() << msg;
}

void RenderViewHost::OnForwardToDevToolsAgent(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(this, message);
}

void RenderViewHost::OnForwardToDevToolsClient(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(this, message);
}

void RenderViewHost::OnActivateDevToolsWindow() {
  DevToolsManager::GetInstance()->ActivateWindow(this);
}

void RenderViewHost::OnCloseDevToolsWindow() {
  DevToolsManager::GetInstance()->CloseWindow(this);
}

void RenderViewHost::OnRequestDockDevToolsWindow() {
  DevToolsManager::GetInstance()->RequestDockWindow(this);
}

void RenderViewHost::OnRequestUndockDevToolsWindow() {
  DevToolsManager::GetInstance()->RequestUndockWindow(this);
}

void RenderViewHost::OnDevToolsRuntimePropertyChanged(
    const std::string& name,
    const std::string& value) {
  DevToolsManager::GetInstance()->
      RuntimePropertyChanged(this, name, value);
}

bool RenderViewHost::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  return view && view->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void RenderViewHost::UnhandledKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->HandleKeyboardEvent(event);
}

void RenderViewHost::OnUserGesture() {
  delegate_->OnUserGesture();
}

void RenderViewHost::GetMalwareDOMDetails() {
  Send(new ViewMsg_GetMalwareDOMDetails(routing_id()));
}

void RenderViewHost::GetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  Send(new ViewMsg_GetAllSavableResourceLinksForCurrentPage(routing_id(),
                                                            page_url));
}

void RenderViewHost::GetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<FilePath>& local_paths,
    const FilePath& local_directory_name) {
  Send(new ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      routing_id(), links, local_paths, local_directory_name));
}

void RenderViewHost::OnMsgShouldCloseACK(bool proceed) {
  StopHangMonitorTimeout();
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in OnMsgNavigate, in which case we can ignore
  // this message.
  if (!is_waiting_for_beforeunload_ack_)
    return;

  is_waiting_for_beforeunload_ack_ = false;

  RenderViewHostDelegate::RendererManagement* management_delegate =
      delegate_->GetRendererManagementDelegate();
  if (management_delegate) {
    management_delegate->ShouldClosePage(
        unload_ack_is_for_cross_site_transition_, proceed);
  }
}

void RenderViewHost::WindowMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(routing_id()));
}

void RenderViewHost::NotifyRendererUnresponsive() {
  delegate_->RendererUnresponsive(
      this, is_waiting_for_beforeunload_ack_ || is_waiting_for_unload_ack_);
}

void RenderViewHost::NotifyRendererResponsive() {
  delegate_->RendererResponsive(this);
}

void RenderViewHost::OnMsgFocusedNodeChanged(bool is_editable_node) {
  delegate_->FocusedNodeChanged(is_editable_node);
}

void RenderViewHost::OnMsgFocus() {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->Activate();
}

void RenderViewHost::OnMsgBlur() {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->Deactivate();
}

void RenderViewHost::ForwardMouseEvent(
    const WebKit::WebMouseEvent& mouse_event) {

  // We make a copy of the mouse event because
  // RenderWidgetHost::ForwardMouseEvent will delete |mouse_event|.
  WebKit::WebMouseEvent event_copy(mouse_event);
  RenderWidgetHost::ForwardMouseEvent(event_copy);

  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    switch (event_copy.type) {
      case WebInputEvent::MouseMove:
        view->HandleMouseMove();
        break;
      case WebInputEvent::MouseLeave:
        view->HandleMouseLeave();
        break;
      case WebInputEvent::MouseDown:
        view->HandleMouseDown();
        break;
      case WebInputEvent::MouseWheel:
        if (ignore_input_events() && delegate_)
          delegate_->OnIgnoredUIEvent();
        break;
      case WebInputEvent::MouseUp:
        view->HandleMouseUp();
      default:
        // For now, we don't care about the rest.
        break;
    }
  }
}

void RenderViewHost::OnMouseActivate() {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->HandleMouseActivate();
}

void RenderViewHost::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (ignore_input_events()) {
    if (key_event.type == WebInputEvent::RawKeyDown && delegate_)
      delegate_->OnIgnoredUIEvent();
    return;
  }
  RenderWidgetHost::ForwardKeyboardEvent(key_event);
}

void RenderViewHost::ForwardEditCommand(const std::string& name,
                                        const std::string& value) {
  IPC::Message* message = new ViewMsg_ExecuteEditCommand(routing_id(),
                                                         name,
                                                         value);
  Send(message);
}

void RenderViewHost::ForwardEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  IPC::Message* message = new ViewMsg_SetEditCommandsForNextKeyEvent(
      routing_id(), edit_commands);
  Send(message);
}

void RenderViewHost::ForwardMessageFromExternalHost(const std::string& message,
                                                    const std::string& origin,
                                                    const std::string& target) {
  Send(new ViewMsg_HandleMessageFromExternalHost(routing_id(), message, origin,
                                                 target));
}

void RenderViewHost::OnExtensionRequest(
    const ViewHostMsg_DomMessage_Params& params) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasExtensionBindings(process()->id())) {
    // This can happen if someone uses window.open() to open an extension URL
    // from a non-extension context.
    BlockExtensionRequest(params.request_id);
    return;
  }

  delegate_->ProcessWebUIMessage(params);
}

void RenderViewHost::SendExtensionResponse(int request_id, bool success,
                                           const std::string& response,
                                           const std::string& error) {
  Send(new ViewMsg_ExtensionResponse(routing_id(), request_id, success,
      response, error));
}

void RenderViewHost::BlockExtensionRequest(int request_id) {
  SendExtensionResponse(request_id, false, "",
                        "Access to extension API denied.");
}

void RenderViewHost::UpdateBrowserWindowId(int window_id) {
  Send(new ViewMsg_UpdateBrowserWindowId(routing_id(), window_id));
}

void RenderViewHost::PerformCustomContextMenuAction(
    const webkit_glue::CustomContextMenuContext& custom_context,
    unsigned action) {
  Send(new ViewMsg_CustomContextMenuAction(routing_id(),
                                           custom_context,
                                           action));
}

void RenderViewHost::SendContentSettings(const GURL& url,
                                         const ContentSettings& settings) {
  Send(new ViewMsg_SetContentSettingsForCurrentURL(url, settings));
}

void RenderViewHost::EnablePreferredSizeChangedMode(int flags) {
  Send(new ViewMsg_EnablePreferredSizeChangedMode(routing_id(), flags));
}

#if defined(OS_MACOSX)
void RenderViewHost::DidSelectPopupMenuItem(int selected_index) {
  Send(new ViewMsg_SelectPopupMenuItem(routing_id(), selected_index));
}

void RenderViewHost::DidCancelPopupMenu() {
  Send(new ViewMsg_SelectPopupMenuItem(routing_id(), -1));
}
#endif

void RenderViewHost::SearchBoxChange(const string16& value,
                                     bool verbatim,
                                     int selection_start,
                                     int selection_end) {
  Send(new ViewMsg_SearchBoxChange(
      routing_id(), value, verbatim, selection_start, selection_end));
}

void RenderViewHost::SearchBoxSubmit(const string16& value,
                                     bool verbatim) {
  Send(new ViewMsg_SearchBoxSubmit(routing_id(), value, verbatim));
}

void RenderViewHost::SearchBoxCancel() {
  Send(new ViewMsg_SearchBoxCancel(routing_id()));
}

void RenderViewHost::SearchBoxResize(const gfx::Rect& search_box_bounds) {
  Send(new ViewMsg_SearchBoxResize(routing_id(), search_box_bounds));
}

void RenderViewHost::DetermineIfPageSupportsInstant(const string16& value,
                                                    bool verbatim,
                                                    int selection_start,
                                                    int selection_end) {
  Send(new ViewMsg_DetermineIfPageSupportsInstant(
           routing_id(), value, verbatim, selection_start, selection_end));
}

void RenderViewHost::FilterURL(ChildProcessSecurityPolicy* policy,
                               int renderer_id,
                               GURL* url) {
  if (!url->is_valid())
    return;  // We don't need to block invalid URLs.

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
    *url = GURL();
  }
}

void RenderViewHost::JavaScriptStressTestControl(int cmd, int param) {
  Send(new ViewMsg_JavaScriptStressTestControl(routing_id(), cmd, param));
}

void RenderViewHost::OnExtensionPostMessage(
    int port_id, const std::string& message) {
  if (process()->profile()->GetExtensionMessageService()) {
    process()->profile()->GetExtensionMessageService()->
        PostMessageFromRenderer(port_id, message);
  }
}

void RenderViewHost::OnAccessibilityNotifications(
    const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  if (view())
    view()->OnAccessibilityNotifications(params);

  if (params.size() > 0) {
    for (unsigned i = 0; i < params.size(); i++) {
      const ViewHostMsg_AccessibilityNotification_Params& param = params[i];

      if (param.notification_type ==
              ViewHostMsg_AccessibilityNotification_Params::
                  NOTIFICATION_TYPE_LOAD_COMPLETE) {
        // TODO(ctguil): Remove when mac processes OnAccessibilityNotifications.
        if (view())
          view()->UpdateAccessibilityTree(param.acc_obj);

        if (save_accessibility_tree_for_testing_)
          accessibility_tree_ = param.acc_obj;
      }
    }

    NotificationService::current()->Notify(
        NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,
        Source<RenderViewHost>(this),
        NotificationService::NoDetails());
  }

  AccessibilityNotificationsAck();
}

void RenderViewHost::OnCSSInserted() {
  delegate_->DidInsertCSS();
}

void RenderViewHost::OnContentBlocked(ContentSettingsType type,
                                      const std::string& resource_identifier) {
  RenderViewHostDelegate::ContentSettings* content_settings_delegate =
      delegate_->GetContentSettingsDelegate();
  if (content_settings_delegate)
    content_settings_delegate->OnContentBlocked(type, resource_identifier);
}

void RenderViewHost::OnAppCacheAccessed(const GURL& manifest_url,
                                        bool blocked_by_policy) {
  RenderViewHostDelegate::ContentSettings* content_settings_delegate =
      delegate_->GetContentSettingsDelegate();
  if (content_settings_delegate)
    content_settings_delegate->OnAppCacheAccessed(manifest_url,
                                                  blocked_by_policy);
}

void RenderViewHost::OnWebDatabaseAccessed(const GURL& url,
                                           const string16& name,
                                           const string16& display_name,
                                           unsigned long estimated_size,
                                           bool blocked_by_policy) {
  RenderViewHostDelegate::ContentSettings* content_settings_delegate =
      delegate_->GetContentSettingsDelegate();
  if (content_settings_delegate)
    content_settings_delegate->OnWebDatabaseAccessed(
        url, name, display_name, estimated_size, blocked_by_policy);
}

void RenderViewHost::OnUpdateZoomLimits(int minimum_percent,
                                        int maximum_percent,
                                        bool remember) {
  delegate_->UpdateZoomLimits(minimum_percent, maximum_percent, remember);
}

void RenderViewHost::OnScriptEvalResponse(int id, const ListValue& result) {
  Value* result_value;
  result.Get(0, &result_value);
  std::pair<int, Value*> details(id, result_value);
  NotificationService::current()->Notify(
      NotificationType::EXECUTE_JAVASCRIPT_RESULT,
      Source<RenderViewHost>(this),
      Details<std::pair<int, Value*> >(&details));
}

#if defined(OS_MACOSX)
void RenderViewHost::OnMsgShowPopup(
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

void RenderViewHost::OnCommandStateChanged(int command,
                                           bool is_enabled,
                                           int checked_state) {
  if (command != RENDER_VIEW_COMMAND_TOGGLE_SPELL_CHECK) {
    LOG(DFATAL) << "Unknown command " << command;
    return;
  }

  if (checked_state != RENDER_VIEW_COMMAND_CHECKED_STATE_UNCHECKED &&
      checked_state != RENDER_VIEW_COMMAND_CHECKED_STATE_CHECKED &&
      checked_state != RENDER_VIEW_COMMAND_CHECKED_STATE_MIXED) {
    LOG(DFATAL) << "Invalid checked state " << checked_state;
    return;
  }

  CommandState state;
  state.is_enabled = is_enabled;
  state.checked_state =
      static_cast<RenderViewCommandCheckedState>(checked_state);
  command_states_[static_cast<RenderViewCommand>(command)] = state;
}
