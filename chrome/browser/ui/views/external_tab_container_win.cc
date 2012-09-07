// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_tab_container_win.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_creator.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_win.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/view_prop.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/win/hwnd_message_handler.h"

using content::BrowserThread;
using content::LoadNotificationDetails;
using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;
using ui::ViewProp;
using WebKit::WebCString;
using WebKit::WebReferrerPolicy;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;

static const char kWindowObjectKey[] = "ChromeWindowObject";

namespace {

// Convert ui::MenuModel into a serializable form for Chrome Frame
ContextMenuModel* ConvertMenuModel(const ui::MenuModel* ui_model) {
  ContextMenuModel* new_model = new ContextMenuModel;

  const int index_base = ui_model->GetFirstItemIndex(NULL);
  const int item_count = ui_model->GetItemCount();
  new_model->items.reserve(item_count);
  for (int i = 0; i < item_count; ++i) {
    const int index = index_base + i;
    if (ui_model->IsVisibleAt(index)) {
      ContextMenuModel::Item item;
      item.type = ui_model->GetTypeAt(index);
      item.item_id = ui_model->GetCommandIdAt(index);
      item.label = ui_model->GetLabelAt(index);
      item.checked = ui_model->IsItemCheckedAt(index);
      item.enabled = ui_model->IsEnabledAt(index);
      if (item.type == ui::MenuModel::TYPE_SUBMENU)
        item.submenu = ConvertMenuModel(ui_model->GetSubmenuModelAt(index));

      new_model->items.push_back(item);
    }
  }

  return new_model;
}

}  // namespace

base::LazyInstance<ExternalTabContainerWin::PendingTabs>
    ExternalTabContainerWin::pending_tabs_ = LAZY_INSTANCE_INITIALIZER;

ExternalTabContainerWin::ExternalTabContainerWin(
    AutomationProvider* automation,
    AutomationResourceMessageFilter* filter)
    : views::NativeWidgetWin(new views::Widget),
      automation_(automation),
      tab_contents_container_(NULL),
      tab_handle_(0),
      ignore_next_load_notification_(false),
      automation_resource_message_filter_(filter),
      load_requests_via_automation_(false),
      handle_top_level_requests_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      pending_(false),
      focus_manager_(NULL),
      external_tab_view_(NULL),
      unload_reply_message_(NULL),
      route_all_top_level_navigations_(false),
      is_popup_window_(false) {
}

// static
scoped_refptr<ExternalTabContainer>
    ExternalTabContainerWin::RemovePendingExternalTab(uintptr_t cookie) {
  PendingTabs& pending_tabs = pending_tabs_.Get();
  PendingTabs::iterator index = pending_tabs.find(cookie);
  if (index != pending_tabs.end()) {
    scoped_refptr<ExternalTabContainer> container = (*index).second;
    pending_tabs.erase(index);
    return container;
  }

  NOTREACHED() << "Failed to find ExternalTabContainer for cookie: "
               << cookie;
  return NULL;
}

bool ExternalTabContainerWin::Init(Profile* profile,
                                   HWND parent,
                                   const gfx::Rect& bounds,
                                   DWORD style,
                                   bool load_requests_via_automation,
                                   bool handle_top_level_requests,
                                   TabContents* existing_contents,
                                   const GURL& initial_url,
                                   const GURL& referrer,
                                   bool infobars_enabled,
                                   bool route_all_top_level_navigations) {
  if (IsWindow(GetNativeView())) {
    NOTREACHED();
    return false;
  }

  load_requests_via_automation_ = load_requests_via_automation;
  handle_top_level_requests_ = handle_top_level_requests;
  route_all_top_level_navigations_ = route_all_top_level_navigations;

  GetMessageHandler()->set_window_style(WS_POPUP | WS_CLIPCHILDREN);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.bounds = bounds;
  params.native_widget = this;
  GetWidget()->Init(params);
  if (!IsWindow(GetNativeView())) {
    NOTREACHED();
    return false;
  }

  // TODO(jcampan): limit focus traversal to contents.

  prop_.reset(new ViewProp(GetNativeView(), kWindowObjectKey, this));

  if (existing_contents) {
    tab_contents_.reset(existing_contents);
    tab_contents_->web_contents()->GetController().SetBrowserContext(profile);
  } else {
    WebContents* new_contents = WebContents::Create(
        profile, NULL, MSG_ROUTING_NONE, NULL);
    tab_contents_.reset(TabContents::Factory::CreateTabContents(new_contents));
  }

  if (!infobars_enabled)
    tab_contents_->infobar_tab_helper()->set_infobars_enabled(false);

  tab_contents_->web_contents()->SetDelegate(this);

  tab_contents_->web_contents()->
      GetMutableRendererPrefs()->browser_handles_non_local_top_level_requests =
          handle_top_level_requests;

  if (!existing_contents) {
    tab_contents_->web_contents()->GetRenderViewHost()->AllowBindings(
        content::BINDINGS_POLICY_EXTERNAL_HOST);
  }

  NavigationController* controller =
      &tab_contents_->web_contents()->GetController();
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                 content::Source<WebContents>(tab_contents_->web_contents()));
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
                 content::NotificationService::AllSources());

  content::WebContentsObserver::Observe(tab_contents_->web_contents());

  // Start loading initial URL
  if (!initial_url.is_empty()) {
    // Navigate out of context since we don't have a 'tab_handle_' yet.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExternalTabContainerWin::Navigate,
                   weak_factory_.GetWeakPtr(),
                   initial_url, referrer));
  }

  // We need WS_POPUP to be on the window during initialization, but
  // once initialized we apply the requested style which may or may not
  // include the popup bit.
  // Note that it's important to do this before we call SetParent since
  // during the SetParent call we will otherwise get a WA_ACTIVATE call
  // that causes us to steal the current focus.
  SetWindowLong(
      GetNativeView(), GWL_STYLE,
      (GetWindowLong(GetNativeView(), GWL_STYLE) & ~WS_POPUP) | style);

  // Now apply the parenting and style
  if (parent)
    SetParent(GetNativeView(), parent);

  ::ShowWindow(tab_contents_->web_contents()->GetNativeView(), SW_SHOWNA);

  LoadAccelerators();
  SetupExternalTabView();
  tab_contents_->blocked_content_tab_helper()->set_delegate(this);
  return true;
}

void ExternalTabContainerWin::Uninitialize() {
  registrar_.RemoveAll();
  if (tab_contents_.get()) {
    UnregisterRenderViewHost(
        tab_contents_->web_contents()->GetRenderViewHost());

    if (GetWidget()->GetRootView())
      GetWidget()->GetRootView()->RemoveAllChildViews(true);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED,
        content::Source<NavigationController>(
            &tab_contents_->web_contents()->GetController()),
        content::Details<ExternalTabContainer>(this));

    tab_contents_.reset(NULL);
  }

  if (focus_manager_) {
    focus_manager_->UnregisterAccelerators(this);
    focus_manager_ = NULL;
  }

  external_tab_view_ = NULL;
  request_context_ = NULL;
  tab_contents_container_ = NULL;
}

bool ExternalTabContainerWin::Reinitialize(
    AutomationProvider* automation_provider,
    AutomationResourceMessageFilter* filter,
    gfx::NativeWindow parent_window) {
  if (!automation_provider || !filter) {
    NOTREACHED();
    return false;
  }

  automation_ = automation_provider;
  automation_resource_message_filter_ = filter;
  // Wait for the automation channel to be initialized before resuming pending
  // render views and sending in the navigation state.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ExternalTabContainerWin::OnReinitialize,
                            weak_factory_.GetWeakPtr()));

  if (parent_window)
    SetParent(GetNativeView(), parent_window);
  return true;
}

WebContents* ExternalTabContainerWin::GetWebContents() const {
  return tab_contents_.get() ? tab_contents_->web_contents() : NULL;
}

TabContents* ExternalTabContainerWin::GetTabContents() {
  return tab_contents_.get();
}

gfx::NativeView ExternalTabContainerWin::GetExternalTabNativeView() const {
  return GetNativeView();
}

void ExternalTabContainerWin::SetTabHandle(int handle) {
  tab_handle_ = handle;
}

int ExternalTabContainerWin::GetTabHandle() const {
  return tab_handle_;
}

void ExternalTabContainerWin::RunUnloadHandlers(IPC::Message* reply_message) {
  if (!automation_) {
    delete reply_message;
    return;
  }

  // If we have a pending unload message, then just respond back to this
  // request and continue processing the previous unload message.
  if (unload_reply_message_) {
     AutomationMsg_RunUnloadHandlers::WriteReplyParams(reply_message, true);
     automation_->Send(reply_message);
     return;
  }

  unload_reply_message_ = reply_message;
  bool wait_for_unload_handlers =
      tab_contents_.get() &&
      Browser::RunUnloadEventsHelper(tab_contents_->web_contents());
  if (!wait_for_unload_handlers) {
    AutomationMsg_RunUnloadHandlers::WriteReplyParams(reply_message, true);
    automation_->Send(reply_message);
    unload_reply_message_ = NULL;
  }
}

void ExternalTabContainerWin::ProcessUnhandledAccelerator(const MSG& msg) {
  NativeWebKeyboardEvent keyboard_event(msg);
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(keyboard_event,
                                                        focus_manager_);
}

void ExternalTabContainerWin::FocusThroughTabTraversal(
    bool reverse,
    bool restore_focus_to_view) {
  DCHECK(tab_contents_.get());
  if (tab_contents_.get())
    tab_contents_->web_contents()->Focus();

  // The tab_contents_ member can get destroyed in the context of the call to
  // TabContentsViewViews::Focus() above. This method eventually calls SetFocus
  // on the native window, which could end up dispatching messages like
  // WM_DESTROY for the external tab.
  if (tab_contents_.get() && restore_focus_to_view)
    tab_contents_->web_contents()->FocusThroughTabTraversal(reverse);
}

// static
bool ExternalTabContainerWin::IsExternalTabContainer(HWND window) {
  return ViewProp::GetValue(window, kWindowObjectKey) != NULL;
}

// static
ExternalTabContainer*
    ExternalTabContainerWin::GetExternalContainerFromNativeWindow(
        gfx::NativeView native_window) {
  ExternalTabContainer* tab_container = NULL;
  if (native_window) {
    tab_container = reinterpret_cast<ExternalTabContainer*>(
        ViewProp::GetValue(native_window, kWindowObjectKey));
  }
  return tab_container;
}
////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, content::WebContentsDelegate implementation:

WebContents* ExternalTabContainerWin::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  if (pending()) {
    pending_open_url_requests_.push_back(params);
    return NULL;
  }

  switch (params.disposition) {
    case CURRENT_TAB:
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
    case NEW_POPUP:
    case NEW_WINDOW:
    case SAVE_TO_DISK:
      if (automation_) {
        GURL referrer = GURL(WebSecurityPolicy::generateReferrerHeader(
            params.referrer.policy,
            params.url,
            WebString::fromUTF8(params.referrer.url.spec())).utf8());
        automation_->Send(new AutomationMsg_OpenURL(tab_handle_,
                                                    params.url,
                                                    referrer,
                                                    params.disposition));
        // TODO(ananta)
        // We should populate other fields in the
        // ViewHostMsg_FrameNavigate_Params structure. Another option could be
        // to refactor the UpdateHistoryForNavigation function in WebContents.
        content::FrameNavigateParams nav_params;
        nav_params.referrer = content::Referrer(referrer,
                                                params.referrer.policy);
        nav_params.url = params.url;
        nav_params.page_id = -1;
        nav_params.transition = content::PAGE_TRANSITION_LINK;

        content::LoadCommittedDetails details;
        details.did_replace_entry = false;

        scoped_refptr<history::HistoryAddPageArgs> add_page_args(
            tab_contents_->history_tab_helper()->
                CreateHistoryAddPageArgs(params.url, details, nav_params));
        tab_contents_->history_tab_helper()->
            UpdateHistoryForNavigation(add_page_args);

        return tab_contents_->web_contents();
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  return NULL;
}

void ExternalTabContainerWin::NavigationStateChanged(const WebContents* source,
                                                     unsigned changed_flags) {
  if (automation_) {
    NavigationInfo nav_info;
    if (InitNavigationInfo(&nav_info, content::NAVIGATION_TYPE_NAV_IGNORE, 0))
      automation_->Send(new AutomationMsg_NavigationStateChanged(
          tab_handle_, changed_flags, nav_info));
  }
}

void ExternalTabContainerWin::AddNewContents(WebContents* source,
                                             WebContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture,
                                             bool* was_blocked) {
  if (!automation_) {
    DCHECK(pending_);
    LOG(ERROR) << "Invalid automation provider. Dropping new contents notify";
    delete new_contents;
    return;
  }

  scoped_refptr<ExternalTabContainerWin> new_container;
  // If the host is a browser like IE8, then the URL being navigated to in the
  // new tab contents could potentially navigate back to Chrome from a new
  // IE process. We support full tab mode only for IE and hence we use that as
  // a determining factor in whether the new ExternalTabContainer instance is
  // created as pending or not.
  if (!route_all_top_level_navigations_) {
    new_container = new ExternalTabContainerWin(NULL, NULL);
  } else {
    // Reuse the same tab handle here as the new container instance is a dummy
    // instance which does not have an automation client connected at the other
    // end.
    new_container = new TemporaryPopupExternalTabContainerWin(
        automation_, automation_resource_message_filter_.get());
    new_container->SetTabHandle(tab_handle_);
  }

  // Make sure that ExternalTabContainer instance is initialized with
  // an unwrapped Profile.
  scoped_ptr<TabContents> tab_contents(
      TabContents::Factory::CreateTabContents(new_contents));
  bool result = new_container->Init(
      tab_contents->profile()->GetOriginalProfile(),
      NULL,
      initial_pos,
      WS_CHILD,
      load_requests_via_automation_,
      handle_top_level_requests_,
      tab_contents.get(),
      GURL(),
      GURL(),
      true,
      route_all_top_level_navigations_);

  if (result) {
    Profile* profile = tab_contents->profile();
    tab_contents.release();  // Ownership has been transferred.
    if (route_all_top_level_navigations_) {
      return;
    }
    uintptr_t cookie = reinterpret_cast<uintptr_t>(new_container.get());
    pending_tabs_.Get()[cookie] = new_container;
    new_container->set_pending(true);
    new_container->set_is_popup_window(disposition == NEW_POPUP);
    AttachExternalTabParams attach_params_;
    attach_params_.cookie = static_cast<uint64>(cookie);
    attach_params_.dimensions = initial_pos;
    attach_params_.user_gesture = user_gesture;
    attach_params_.disposition = disposition;
    attach_params_.profile_name = WideToUTF8(
        profile->GetPath().DirName().BaseName().value());
    automation_->Send(new AutomationMsg_AttachExternalTab(
        tab_handle_, attach_params_));
  } else {
    NOTREACHED();
  }
}

void ExternalTabContainerWin::WebContentsCreated(WebContents* source_contents,
                                                 int64 source_frame_id,
                                                 const GURL& target_url,
                                                 WebContents* new_contents) {
  if (!load_requests_via_automation_)
    return;

  RenderViewHost* rvh = new_contents->GetRenderViewHost();
  DCHECK(rvh != NULL);

  // Register this render view as a pending render view, i.e. any network
  // requests initiated by this render view would be serviced when the
  // external host connects to the new external tab instance.
  RegisterRenderViewHostForAutomation(rvh, true);
}

void ExternalTabContainerWin::CloseContents(content::WebContents* source) {
  if (!automation_)
    return;

  if (unload_reply_message_) {
    AutomationMsg_RunUnloadHandlers::WriteReplyParams(unload_reply_message_,
                                                      true);
    automation_->Send(unload_reply_message_);
    unload_reply_message_ = NULL;
  } else {
    automation_->Send(new AutomationMsg_CloseExternalTab(tab_handle_));
  }
}

void ExternalTabContainerWin::MoveContents(WebContents* source,
                                           const gfx::Rect& pos) {
  if (automation_ && is_popup_window_)
    automation_->Send(new AutomationMsg_MoveWindow(tab_handle_, pos));
}

TabContents* ExternalTabContainerWin::GetConstrainingTabContents(
    TabContents* source) {
  return source;
}

ExternalTabContainerWin::~ExternalTabContainerWin() {
  Uninitialize();
}

bool ExternalTabContainerWin::IsPopupOrPanel(const WebContents* source) const {
  return is_popup_window_;
}

void ExternalTabContainerWin::UpdateTargetURL(WebContents* source,
                                              int32 page_id,
                                              const GURL& url) {
  if (automation_) {
    string16 url_string = CA2W(url.spec().c_str());
    automation_->Send(
        new AutomationMsg_UpdateTargetUrl(tab_handle_, url_string));
  }
}

void ExternalTabContainerWin::ContentsZoomChange(bool zoom_in) {
}

gfx::NativeWindow ExternalTabContainerWin::GetFrameNativeWindow() {
  return GetNativeView();
}

bool ExternalTabContainerWin::TakeFocus(content::WebContents* source,
                                        bool reverse) {
  if (automation_) {
    automation_->Send(new AutomationMsg_TabbedOut(tab_handle_,
        base::win::IsShiftPressed()));
  }

  return true;
}

bool ExternalTabContainerWin::CanDownload(RenderViewHost* render_view_host,
                                          int request_id,
                                          const std::string& request_method) {
  if (load_requests_via_automation_) {
    if (automation_) {
      // In case the host needs to show UI that needs to take the focus.
      ::AllowSetForegroundWindow(ASFW_ANY);

      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
             base::IgnoreResult(
                 &AutomationResourceMessageFilter::SendDownloadRequestToHost),
             automation_resource_message_filter_.get(), 0, tab_handle_,
             request_id));
    }
  } else {
    DLOG(WARNING) << "Downloads are only supported with host browser network "
                     "stack enabled.";
  }

  // Never allow downloads.
  return false;
}

void ExternalTabContainerWin::RegisterRenderViewHostForAutomation(
    RenderViewHost* render_view_host,
    bool pending_view) {
  if (render_view_host) {
    AutomationResourceMessageFilter::RegisterRenderView(
        render_view_host->GetProcess()->GetID(),
        render_view_host->GetRoutingID(),
        GetTabHandle(),
        automation_resource_message_filter_,
        pending_view);
  }
}

void ExternalTabContainerWin::RegisterRenderViewHost(
    RenderViewHost* render_view_host) {
  // RenderViewHost instances that are to be associated with this
  // ExternalTabContainer should share the same resource request automation
  // settings.
  RegisterRenderViewHostForAutomation(
      render_view_host,
      false);  // Network requests should not be handled later.
}

void ExternalTabContainerWin::UnregisterRenderViewHost(
    RenderViewHost* render_view_host) {
  // Undo the resource automation registration performed in
  // ExternalTabContainerWin::RegisterRenderViewHost.
  if (render_view_host) {
    AutomationResourceMessageFilter::UnRegisterRenderView(
      render_view_host->GetProcess()->GetID(),
      render_view_host->GetRoutingID());
  }
}

content::JavaScriptDialogCreator*
ExternalTabContainerWin::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

bool ExternalTabContainerWin::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (!automation_) {
    NOTREACHED();
    return false;
  }
  external_context_menu_.reset(RenderViewContextMenuViews::Create(
      web_contents(), params));
  static_cast<RenderViewContextMenuWin*>(
      external_context_menu_.get())->SetExternal();
  external_context_menu_->Init();
  external_context_menu_->UpdateMenuItemStates();

  scoped_ptr<ContextMenuModel> context_menu_model(
    ConvertMenuModel(&external_context_menu_->menu_model()));

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetNativeView(), HWND_DESKTOP, &screen_pt, 1);

  MiniContextMenuParams ipc_params;
  ipc_params.screen_x = screen_pt.x;
  ipc_params.screen_y = screen_pt.y;
  ipc_params.link_url = params.link_url;
  ipc_params.unfiltered_link_url = params.unfiltered_link_url;
  ipc_params.src_url = params.src_url;
  ipc_params.page_url = params.page_url;
  ipc_params.keyword_url = params.keyword_url;
  ipc_params.frame_url = params.frame_url;

  bool rtl = base::i18n::IsRTL();
  automation_->Send(
      new AutomationMsg_ForwardContextMenuToExternalHost(tab_handle_,
          *context_menu_model,
          rtl ? TPM_RIGHTALIGN : TPM_LEFTALIGN, ipc_params));

  return true;
}

bool ExternalTabContainerWin::ExecuteContextMenuCommand(int command) {
  if (!external_context_menu_.get()) {
    NOTREACHED();
    return false;
  }

  switch (command) {
    case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
    case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDS_CONTENT_CONTEXT_SAVELINKAS: {
      NOTREACHED();  // Should be handled in host.
      break;
    }
  }

  external_context_menu_->ExecuteCommand(command);
  return true;
}

bool ExternalTabContainerWin::PreHandleKeyboardEvent(
    content::WebContents* source,
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

void ExternalTabContainerWin::HandleKeyboardEvent(
    content::WebContents* source,
    const NativeWebKeyboardEvent& event) {
  ProcessUnhandledKeyStroke(event.os_event.hwnd, event.os_event.message,
                            event.os_event.wParam, event.os_event.lParam);
}

void ExternalTabContainerWin::BeforeUnloadFired(WebContents* tab,
                                                bool proceed,
                                                bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;

  if (!automation_) {
    delete unload_reply_message_;
    unload_reply_message_ = NULL;
    return;
  }

  if (!unload_reply_message_) {
    NOTREACHED() << "**** NULL unload reply message pointer.";
    return;
  }

  if (!proceed) {
    AutomationMsg_RunUnloadHandlers::WriteReplyParams(unload_reply_message_,
                                                      false);
    automation_->Send(unload_reply_message_);
    unload_reply_message_ = NULL;
    *proceed_to_fire_unload = false;
  }
}

void ExternalTabContainerWin::ShowRepostFormWarningDialog(WebContents* source) {
  chrome::ShowTabModalConfirmDialog(new RepostFormWarningController(source),
                                    TabContents::FromWebContents(source));
}

void ExternalTabContainerWin::RunFileChooser(
    WebContents* tab,
    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

void ExternalTabContainerWin::EnumerateDirectory(WebContents* tab,
                                                 int request_id,
                                                 const FilePath& path) {
  FileSelectHelper::EnumerateDirectory(tab, request_id, path);
}

void ExternalTabContainerWin::JSOutOfMemory(WebContents* tab) {
  Browser::JSOutOfMemoryHelper(tab);
}

void ExternalTabContainerWin::RegisterProtocolHandler(
    WebContents* tab,
    const std::string& protocol,
    const GURL& url,
    const string16& title,
    bool user_gesture) {
  Browser::RegisterProtocolHandlerHelper(tab, protocol, url, title,
                                         user_gesture, NULL);
}

void ExternalTabContainerWin::RegisterIntentHandler(
    WebContents* tab,
    const webkit_glue::WebIntentServiceData& data,
    bool user_gesture) {
  Browser::RegisterIntentHandlerHelper(tab, data, user_gesture);
}

void ExternalTabContainerWin::WebIntentDispatch(
    WebContents* tab,
    content::WebIntentsDispatcher* intents_dispatcher) {
  // TODO(binji) How do we want to display the WebIntentPicker bubble if there
  // is no BrowserWindow?
  delete intents_dispatcher;
}

void ExternalTabContainerWin::FindReply(WebContents* tab,
                                        int request_id,
                                        int number_of_matches,
                                        const gfx::Rect& selection_rect,
                                        int active_match_ordinal,
                                        bool final_update) {
  Browser::FindReplyHelper(tab, request_id, number_of_matches, selection_rect,
                           active_match_ordinal, final_update);
}

void ExternalTabContainerWin::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback) {
  Browser::RequestMediaAccessPermissionHelper(web_contents, request, callback);
}

bool ExternalTabContainerWin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExternalTabContainerWin, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ForwardMessageToExternalHost,
                        OnForwardMessageToExternalHost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExternalTabContainerWin::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  automation_->Send(new AutomationMsg_NavigationFailed(
      tab_handle_, error_code, validated_url));
  ignore_next_load_notification_ = true;
}

void ExternalTabContainerWin::OnForwardMessageToExternalHost(
    const std::string& message,
    const std::string& origin,
    const std::string& target) {
  if (automation_) {
    automation_->Send(new AutomationMsg_ForwardMessageToExternalHost(
        tab_handle_, message, origin, target));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, NotificationObserver implementation:

void ExternalTabContainerWin::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!automation_)
    return;

  static const int kHttpClientErrorStart = 400;
  static const int kHttpServerErrorEnd = 510;

  switch (type) {
    case content::NOTIFICATION_LOAD_STOP: {
        const LoadNotificationDetails* load =
            content::Details<LoadNotificationDetails>(details).ptr();
        if (load && content::PageTransitionIsMainFrame(load->origin)) {
          TRACE_EVENT_END_ETW("ExternalTabContainerWin::Navigate", 0,
                              load->url.spec());
          automation_->Send(new AutomationMsg_TabLoaded(tab_handle_,
                                                        load->url));
        }
        break;
      }
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      if (ignore_next_load_notification_) {
        ignore_next_load_notification_ = false;
        return;
      }

      const content::LoadCommittedDetails* commit =
          content::Details<content::LoadCommittedDetails>(details).ptr();

      if (commit->http_status_code >= kHttpClientErrorStart &&
          commit->http_status_code <= kHttpServerErrorEnd) {
        automation_->Send(new AutomationMsg_NavigationFailed(
            tab_handle_, commit->http_status_code, commit->entry->GetURL()));

        ignore_next_load_notification_ = true;
      } else {
        NavigationInfo navigation_info;
        // When the previous entry index is invalid, it will be -1, which
        // will still make the computation come out right (navigating to the
        // 0th entry will be +1).
        if (InitNavigationInfo(&navigation_info, commit->type,
                commit->previous_entry_index -
                tab_contents_->web_contents()->
                    GetController().GetLastCommittedEntryIndex()))
          automation_->Send(new AutomationMsg_DidNavigate(tab_handle_,
                                                          navigation_info));
      }
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = content::Details<RenderViewHost>(details).ptr();
        RegisterRenderViewHostForAutomation(rvh, false);
      }
      break;
    }
    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = content::Source<RenderViewHost>(source).ptr();
        UnregisterRenderViewHost(rvh);
      }
      break;
    }
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED: {
      if (load_requests_via_automation_) {
        RenderViewHost* rvh = content::Source<RenderViewHost>(source).ptr();
        RegisterRenderViewHostForAutomation(rvh, false);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, views::NativeWidgetWin overrides:

bool ExternalTabContainerWin::PreHandleMSG(UINT message,
                                           WPARAM w_param,
                                           LPARAM l_param,
                                           LRESULT* result) {
  if (message == WM_DESTROY) {
    prop_.reset();
    Uninitialize();
  }
  return false;
}

void ExternalTabContainerWin::PostHandleMSG(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
    // Grab a reference here which will be released in OnFinalMessage
  if (message == WM_CREATE)
    AddRef();
}

void ExternalTabContainerWin::OnFinalMessage(HWND window) {
  GetWidget()->OnNativeWidgetDestroyed();
  // Release the reference which we grabbed in WM_CREATE.
  Release();
}

////////////////////////////////////////////////////////////////////////////////
// ExternalTabContainer, private:
bool ExternalTabContainerWin::ProcessUnhandledKeyStroke(HWND window,
                                                        UINT message,
                                                        WPARAM wparam,
                                                        LPARAM lparam) {
  if (!automation_) {
    return false;
  }
  if ((wparam == VK_TAB) && !base::win::IsCtrlPressed()) {
    // Tabs are handled separately (except if this is Ctrl-Tab or
    // Ctrl-Shift-Tab)
    return false;
  }

  // Send this keystroke to the external host as it could be processed as an
  // accelerator there. If the host does not handle this accelerator, it will
  // reflect the accelerator back to us via the ProcessUnhandledAccelerator
  // method.
  MSG msg = {0};
  msg.hwnd = window;
  msg.message = message;
  msg.wParam = wparam;
  msg.lParam = lparam;
  automation_->Send(new AutomationMsg_HandleAccelerator(tab_handle_, msg));
  return true;
}

bool ExternalTabContainerWin::InitNavigationInfo(
    NavigationInfo* nav_info,
    content::NavigationType nav_type,
    int relative_offset) {
  DCHECK(nav_info);
  NavigationEntry* entry =
      tab_contents_->web_contents()->GetController().GetActiveEntry();
  // If this is very early in the game then we may not have an entry.
  if (!entry)
    return false;

  nav_info->navigation_type = nav_type;
  nav_info->relative_offset = relative_offset;
  nav_info->navigation_index =
      tab_contents_->web_contents()->GetController().GetCurrentEntryIndex();
  nav_info->url = entry->GetURL();
  nav_info->referrer = entry->GetReferrer().url;
  nav_info->title =  UTF16ToWideHack(entry->GetTitle());
  if (nav_info->title.empty())
    nav_info->title = UTF8ToWide(nav_info->url.spec());

  nav_info->security_style = entry->GetSSL().security_style;
  int content_status = entry->GetSSL().content_status;
  nav_info->displayed_insecure_content =
      !!(content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT);
  nav_info->ran_insecure_content =
      !!(content_status & SSLStatus::RAN_INSECURE_CONTENT);
  return true;
}

SkColor ExternalTabContainerWin::GetInfoBarSeparatorColor() const {
  return ThemeService::GetDefaultColor(ThemeService::COLOR_TOOLBAR_SEPARATOR);
}

void ExternalTabContainerWin::InfoBarContainerStateChanged(bool is_animating) {
  if (external_tab_view_)
    external_tab_view_->Layout();
}

bool ExternalTabContainerWin::DrawInfoBarArrows(int* x) const {
  return false;
}

bool ExternalTabContainerWin::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());

  if (!tab_contents_.get() ||
      !tab_contents_->web_contents()->GetRenderViewHost()) {
    NOTREACHED();
    return false;
  }

  RenderViewHost* host = tab_contents_->web_contents()->GetRenderViewHost();
  int command_id = iter->second;
  switch (command_id) {
    case IDC_ZOOM_PLUS:
      host->Zoom(content::PAGE_ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      host->Zoom(content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_MINUS:
      host->Zoom(content::PAGE_ZOOM_OUT);
      break;
    case IDC_DEV_TOOLS:
      DevToolsWindow::ToggleDevToolsWindow(
          tab_contents_->web_contents()->GetRenderViewHost(),
          false,
          DEVTOOLS_TOGGLE_ACTION_SHOW);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      DevToolsWindow::ToggleDevToolsWindow(
          tab_contents_->web_contents()->GetRenderViewHost(),
          false,
          DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
      break;
    case IDC_DEV_TOOLS_INSPECT:
      DevToolsWindow::ToggleDevToolsWindow(
          tab_contents_->web_contents()->GetRenderViewHost(),
          false,
          DEVTOOLS_TOGGLE_ACTION_INSPECT);
      break;
    case IDC_DEV_TOOLS_TOGGLE:
      DevToolsWindow::ToggleDevToolsWindow(
          tab_contents_->web_contents()->GetRenderViewHost(),
          false,
          DEVTOOLS_TOGGLE_ACTION_TOGGLE);
      break;
    default:
      NOTREACHED() << "Unsupported accelerator: " << command_id;
      return false;
  }
  return true;
}

bool ExternalTabContainerWin::CanHandleAccelerators() const {
  return true;
}

void ExternalTabContainerWin::Navigate(const GURL& url, const GURL& referrer) {
  if (!tab_contents_.get()) {
    NOTREACHED();
    return;
  }

  TRACE_EVENT_BEGIN_ETW("ExternalTabContainerWin::Navigate", 0, url.spec());

  tab_contents_->web_contents()->GetController().LoadURL(
      url, content::Referrer(referrer, WebKit::WebReferrerPolicyDefault),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
}

bool ExternalTabContainerWin::OnGoToEntryOffset(int offset) {
  if (load_requests_via_automation_) {
    automation_->Send(new AutomationMsg_RequestGoToHistoryEntryOffset(
        tab_handle_, offset));
    return false;
  }

  return true;
}

void ExternalTabContainerWin::LoadAccelerators() {
  HACCEL accelerator_table = AtlLoadAccelerators(IDR_CHROMEFRAME);
  DCHECK(accelerator_table);

  // We have to copy the table to access its contents.
  int count = CopyAcceleratorTable(accelerator_table, 0, 0);
  if (count == 0) {
    // Nothing to do in that case.
    return;
  }

  scoped_array<ACCEL> scoped_accelerators(new ACCEL[count]);
  ACCEL* accelerators = scoped_accelerators.get();
  DCHECK(accelerators != NULL);

  CopyAcceleratorTable(accelerator_table, accelerators, count);

  focus_manager_ = GetWidget()->GetFocusManager();
  DCHECK(focus_manager_);

  // Let's fill our own accelerator table.
  for (int i = 0; i < count; ++i) {
    ui::Accelerator accelerator(
        static_cast<ui::KeyboardCode>(accelerators[i].key),
        ui::GetModifiersFromACCEL(accelerators[i]));
    accelerator_table_[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    if (focus_manager_) {
      focus_manager_->RegisterAccelerator(
          accelerator, ui::AcceleratorManager::kNormalPriority, this);
    }
  }
}

void ExternalTabContainerWin::OnReinitialize() {
  if (load_requests_via_automation_) {
    RenderViewHost* rvh = tab_contents_->web_contents()->GetRenderViewHost();
    if (rvh) {
      AutomationResourceMessageFilter::ResumePendingRenderView(
          rvh->GetProcess()->GetID(), rvh->GetRoutingID(),
          tab_handle_, automation_resource_message_filter_);
    }
  }

  NavigationStateChanged(web_contents(), 0);
  ServicePendingOpenURLRequests();
}

void ExternalTabContainerWin::ServicePendingOpenURLRequests() {
  DCHECK(pending());

  set_pending(false);

  for (size_t index = 0; index < pending_open_url_requests_.size();
       ++index) {
    const OpenURLParams& url_request = pending_open_url_requests_[index];
    OpenURLFromTab(web_contents(), url_request);
  }
  pending_open_url_requests_.clear();
}

void ExternalTabContainerWin::SetupExternalTabView() {
  // Create a TabContentsContainer to handle focus cycling using Tab and
  // Shift-Tab.
  tab_contents_container_ = new views::WebView(tab_contents_->profile());

  // The views created here will be destroyed when the ExternalTabContainer
  // widget is torn down.
  external_tab_view_ = new views::View();

  InfoBarContainerView* info_bar_container = new InfoBarContainerView(this);
  info_bar_container->ChangeTabContents(tab_contents_->infobar_tab_helper());

  views::GridLayout* layout = new views::GridLayout(external_tab_view_);
  // Give this column an identifier of 0.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL,
                     views::GridLayout::FILL,
                     1,
                     views::GridLayout::USE_PREF,
                     0,
                     0);

  external_tab_view_->SetLayoutManager(layout);

  layout->StartRow(0, 0);
  layout->AddView(info_bar_container);
  layout->StartRow(1, 0);
  layout->AddView(tab_contents_container_);
  GetWidget()->SetContentsView(external_tab_view_);
  // Note that SetTabContents must be called after AddChildView is called
  tab_contents_container_->SetWebContents(web_contents());
}

// static
ExternalTabContainer* ExternalTabContainer::Create(
    AutomationProvider* automation_provider,
    AutomationResourceMessageFilter* filter) {
  return new ExternalTabContainerWin(automation_provider, filter);
}

// static
ExternalTabContainer* ExternalTabContainer::GetContainerForTab(
    HWND tab_window) {
  HWND parent_window = ::GetParent(tab_window);
  if (!::IsWindow(parent_window)) {
    return NULL;
  }
  if (!ExternalTabContainerWin::IsExternalTabContainer(parent_window)) {
    return NULL;
  }
  ExternalTabContainer* container = reinterpret_cast<ExternalTabContainer*>(
      ViewProp::GetValue(parent_window, kWindowObjectKey));
  return container;
}

// static
scoped_refptr<ExternalTabContainer> ExternalTabContainer::RemovePendingTab(
    uintptr_t cookie) {
  return ExternalTabContainerWin::RemovePendingExternalTab(cookie);
}

///////////////////////////////////////////////////////////////////////////////
// TemporaryPopupExternalTabContainerWin

TemporaryPopupExternalTabContainerWin::TemporaryPopupExternalTabContainerWin(
    AutomationProvider* automation,
    AutomationResourceMessageFilter* filter)
    : ExternalTabContainerWin(automation, filter) {
}

TemporaryPopupExternalTabContainerWin::~TemporaryPopupExternalTabContainerWin(
    ) {
  DVLOG(1) << __FUNCTION__;
}

WebContents* TemporaryPopupExternalTabContainerWin::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  if (!automation_)
    return NULL;

  OpenURLParams forward_params = params;
  if (params.disposition == CURRENT_TAB) {
    DCHECK(route_all_top_level_navigations_);
    forward_params.disposition = NEW_FOREGROUND_TAB;
  }
  WebContents* new_contents =
      ExternalTabContainerWin::OpenURLFromTab(source, forward_params);
  // support only one navigation for a dummy tab before it is killed.
  ::DestroyWindow(GetNativeView());
  return new_contents;
}
