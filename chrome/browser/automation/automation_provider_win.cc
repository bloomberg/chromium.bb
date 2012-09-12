// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/external_tab/external_tab_container.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/ui_controls/ui_controls.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/widget/root_view.h"

using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;

namespace {

// Allow some pending tasks up to |num_deferrals| generations to complete.
void DeferredQuitRunLoop(const base::Closure& quit_task,
                         int num_quit_deferrals) {
  if (num_quit_deferrals <= 0) {
    quit_task.Run();
  } else {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&DeferredQuitRunLoop, quit_task, num_quit_deferrals - 1));
  }
}

// This callback just adds another callback to the event queue. This is useful
// if you want to ensure that any callbacks added to the event queue after this
// one have already been processed by the time |callback| is run.
void InvokeCallbackLater(const base::Closure& callback) {
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void MoveMouse(const POINT& point) {
  SetCursorPos(point.x, point.y);

  // Now, make sure that GetMessagePos returns the values we just set by
  // simulating a mouse move.  The value returned by GetMessagePos is updated
  // when a mouse move event is removed from the event queue.
  PostMessage(NULL, WM_MOUSEMOVE, 0, MAKELPARAM(point.x, point.y));
  MSG msg;
  while (PeekMessage(&msg, NULL, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
  }

  // Verify
#ifndef NDEBUG
  DWORD pos = GetMessagePos();
  gfx::Point cursor_point(pos);
  DCHECK_EQ(point.x, cursor_point.x());
  DCHECK_EQ(point.y, cursor_point.y());
#endif
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM l_param) {
  if (hwnd == reinterpret_cast<HWND>(l_param)) {
    return FALSE;
  }
  return TRUE;
}

// This callback sends a WindowDragResponse message with the appropriate routing
// ID to the automation proxy.  This is implemented as a task so that we know
// that the mouse events (and any tasks that they spawn on the message loop)
// have been processed by the time this is sent.
void WindowDragResponseCallback(AutomationProvider* provider,
                                IPC::Message* reply_message) {
  DCHECK(reply_message != NULL);
  AutomationMsg_WindowDrag::WriteReplyParams(reply_message, true);
  provider->Send(reply_message);
}

}  // namespace

void AutomationProvider::WindowSimulateDrag(
    int handle,
    const std::vector<gfx::Point>& drag_path,
    int flags,
    bool press_escape_en_route,
    IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(handle) && (drag_path.size() > 1)) {
    gfx::NativeWindow window =
        browser_tracker_->GetResource(handle)->window()->GetNativeWindow();

    UINT down_message = 0;
    UINT up_message = 0;
    WPARAM wparam_flags = 0;
    if (flags & ui::EF_SHIFT_DOWN)
      wparam_flags |= MK_SHIFT;
    if (flags & ui::EF_CONTROL_DOWN)
      wparam_flags |= MK_CONTROL;
    if (flags & ui::EF_LEFT_MOUSE_BUTTON) {
      wparam_flags |= MK_LBUTTON;
      down_message = WM_LBUTTONDOWN;
      up_message = WM_LBUTTONUP;
    }
    if (flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
      wparam_flags |= MK_MBUTTON;
      down_message = WM_MBUTTONDOWN;
      up_message = WM_MBUTTONUP;
    }
    if (flags & ui::EF_RIGHT_MOUSE_BUTTON) {
      wparam_flags |= MK_RBUTTON;
      down_message = WM_LBUTTONDOWN;
      up_message = WM_LBUTTONUP;
    }

    Browser* browser = browser_tracker_->GetResource(handle);
    DCHECK(browser);
    HWND top_level_hwnd =
        reinterpret_cast<HWND>(browser->window()->GetNativeWindow());
    POINT temp = drag_path[0].ToPOINT();
    MapWindowPoints(top_level_hwnd, HWND_DESKTOP, &temp, 1);
    MoveMouse(temp);
    SendMessage(top_level_hwnd, down_message, wparam_flags,
                MAKELPARAM(drag_path[0].x(), drag_path[0].y()));
    for (int i = 1; i < static_cast<int>(drag_path.size()); ++i) {
      temp = drag_path[i].ToPOINT();
      MapWindowPoints(top_level_hwnd, HWND_DESKTOP, &temp, 1);
      MoveMouse(temp);
      SendMessage(top_level_hwnd, WM_MOUSEMOVE, wparam_flags,
                  MAKELPARAM(drag_path[i].x(), drag_path[i].y()));
    }
    POINT end = drag_path[drag_path.size() - 1].ToPOINT();
    MapWindowPoints(top_level_hwnd, HWND_DESKTOP, &end, 1);
    MoveMouse(end);

    if (press_escape_en_route) {
      // Press Escape, making sure we wait until chrome processes the escape.
      // TODO(phajdan.jr): make this use ui_test_utils::SendKeyPressSync.
      views::AcceleratorHandler handler;
      base::RunLoop run_loop(&handler);
      // Number of times to repost Quit task to allow pending tasks to complete.
      // See kNumQuitDeferrals in ui_test_utils.cc for explanation.
      int num_quit_deferrals = 10;
      ui_controls::SendKeyPressNotifyWhenDone(
          window, ui::VKEY_ESCAPE,
          ((flags & ui::EF_CONTROL_DOWN) ==
           ui::EF_CONTROL_DOWN),
          ((flags & ui::EF_SHIFT_DOWN) ==
           ui::EF_SHIFT_DOWN),
          ((flags & ui::EF_ALT_DOWN) == ui::EF_ALT_DOWN),
          false,
          base::Bind(&DeferredQuitRunLoop, run_loop.QuitClosure(),
                     num_quit_deferrals));
      MessageLoopForUI* loop = MessageLoopForUI::current();
      MessageLoop::ScopedNestableTaskAllower allow(loop);
      run_loop.Run();
    }
    SendMessage(top_level_hwnd, up_message, wparam_flags,
                MAKELPARAM(end.x, end.y));

    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(
            &InvokeCallbackLater,
            base::Bind(&WindowDragResponseCallback, this, reply_message)));
  } else {
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
    Send(reply_message);
  }
}

void AutomationProvider::CreateExternalTab(
    const ExternalTabSettings& settings,
    gfx::NativeWindow* tab_container_window, gfx::NativeWindow* tab_window,
    int* tab_handle, int* session_id) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::CreateExternalTab", 0, "");

  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;
  *session_id = -1;
  scoped_refptr<ExternalTabContainer> external_tab_container =
      ExternalTabContainer::Create(this, automation_resource_message_filter_);

  Profile* profile = settings.is_incognito ?
      profile_->GetOffTheRecordProfile() : profile_;

  // When the ExternalTabContainer window is created we grab a reference on it
  // which is released when the window is destroyed.
  external_tab_container->Init(profile, settings.parent, settings.dimensions,
      settings.style, settings.load_requests_via_automation,
      settings.handle_top_level_requests, NULL, settings.initial_url,
      settings.referrer, settings.infobars_enabled,
      settings.route_all_top_level_navigations);

  if (AddExternalTab(external_tab_container)) {
    WebContents* web_contents = external_tab_container->GetWebContents();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(web_contents);
    *tab_handle = external_tab_container->GetTabHandle();
    *tab_container_window = external_tab_container->GetExternalTabNativeView();
    *tab_window = web_contents->GetNativeView();
    *session_id = session_tab_helper->session_id().id();
  } else {
    external_tab_container->Uninitialize();
  }

  TRACE_EVENT_END_ETW("AutomationProvider::CreateExternalTab", 0, "");
}

bool AutomationProvider::AddExternalTab(ExternalTabContainer* external_tab) {
  DCHECK(external_tab != NULL);

  WebContents* web_contents = external_tab->GetWebContents();
  if (web_contents) {
    int tab_handle = tab_tracker_->Add(&web_contents->GetController());
    external_tab->SetTabHandle(tab_handle);
    return true;
  }

  return false;
}

void AutomationProvider::ProcessUnhandledAccelerator(
    const IPC::Message& message, int handle, const MSG& msg) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->ProcessUnhandledAccelerator(msg);
  }
  // This message expects no response.
}

void AutomationProvider::SetInitialFocus(const IPC::Message& message,
                                         int handle, bool reverse,
                                         bool restore_focus_to_view) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->FocusThroughTabTraversal(reverse, restore_focus_to_view);
  }
  // This message expects no response.
}

void AutomationProvider::PrintAsync(int tab_handle) {
  WebContents* web_contents = GetWebContentsForHandle(tab_handle, NULL);
  if (!web_contents)
    return;

  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  tab_contents->print_view_manager()->PrintNow();
}

ExternalTabContainer* AutomationProvider::GetExternalTabForHandle(int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    return ExternalTabContainer::GetContainerForTab(
        tab->GetWebContents()->GetNativeView());
  }

  return NULL;
}

void AutomationProvider::OnTabReposition(
    int tab_handle, const Reposition_Params& params) {
  if (!tab_tracker_->ContainsHandle(tab_handle))
    return;

  if (!IsWindow(params.window))
    return;

  unsigned long process_id = 0;
  unsigned long thread_id = 0;

  thread_id = GetWindowThreadProcessId(params.window, &process_id);

  if (thread_id != GetCurrentThreadId()) {
    DCHECK_EQ(thread_id, GetCurrentThreadId());
    return;
  }

  SetWindowPos(params.window, params.window_insert_after, params.left,
               params.top, params.width, params.height, params.flags);

  if (params.set_parent) {
    if (IsWindow(params.parent_window)) {
      if (!SetParent(params.window, params.parent_window))
        DLOG(WARNING) << "SetParent failed. Error 0x%x" << GetLastError();
    }
  }
}

void AutomationProvider::OnForwardContextMenuCommandToChrome(int tab_handle,
                                                             int command) {
  if (!tab_tracker_->ContainsHandle(tab_handle))
    return;

  NavigationController* tab = tab_tracker_->GetResource(tab_handle);
  if (!tab) {
    NOTREACHED();
    return;
  }

  WebContents* web_contents = tab->GetWebContents();
  if (!web_contents || !web_contents->GetDelegate()) {
    NOTREACHED();
    return;
  }

  web_contents->GetDelegate()->ExecuteContextMenuCommand(command);
}

void AutomationProvider::ConnectExternalTab(
    uint64 cookie,
    bool allow,
    gfx::NativeWindow parent_window,
    gfx::NativeWindow* tab_container_window,
    gfx::NativeWindow* tab_window,
    int* tab_handle,
    int* session_id) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::ConnectExternalTab", 0, "");

  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;
  *session_id = -1;

  scoped_refptr<ExternalTabContainer> external_tab_container =
      ExternalTabContainer::RemovePendingTab(static_cast<uintptr_t>(cookie));
  if (!external_tab_container.get()) {
    NOTREACHED();
    return;
  }

  if (allow && AddExternalTab(external_tab_container)) {
    external_tab_container->Reinitialize(this,
                                         automation_resource_message_filter_,
                                         parent_window);
    WebContents* web_contents = external_tab_container->GetWebContents();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(web_contents);
    *tab_handle = external_tab_container->GetTabHandle();
    *tab_container_window = external_tab_container->GetExternalTabNativeView();
    *tab_window = web_contents->GetNativeView();
    *session_id = session_tab_helper->session_id().id();
  } else {
    external_tab_container->Uninitialize();
  }

  TRACE_EVENT_END_ETW("AutomationProvider::ConnectExternalTab", 0, "");
}

void AutomationProvider::OnBrowserMoved(int tab_handle) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(tab_handle);
  if (!external_tab) {
    DLOG(WARNING) <<
      "AutomationProvider::OnBrowserMoved called with invalid tab handle.";
  }
}

void AutomationProvider::OnMessageFromExternalHost(int handle,
                                                   const std::string& message,
                                                   const std::string& origin,
                                                   const std::string& target) {
  RenderViewHost* view_host = GetViewForTab(handle);
  if (!view_host)
    return;

  view_host->Send(new ChromeViewMsg_HandleMessageFromExternalHost(
      view_host->GetRoutingID(), message, origin, target));
}

void AutomationProvider::NavigateInExternalTab(
    int handle, const GURL& url, const GURL& referrer,
    AutomationMsg_NavigationResponseValues* status) {
  *status = AUTOMATION_MSG_NAVIGATION_ERROR;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab->LoadURL(
        url,
        content::Referrer(referrer, WebKit::WebReferrerPolicyDefault),
        content::PAGE_TRANSITION_TYPED, std::string());
    *status = AUTOMATION_MSG_NAVIGATION_SUCCESS;
  }
}

void AutomationProvider::NavigateExternalTabAtIndex(
    int handle, int navigation_index,
    AutomationMsg_NavigationResponseValues* status) {
  *status = AUTOMATION_MSG_NAVIGATION_ERROR;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab->GoToIndex(navigation_index);
    *status = AUTOMATION_MSG_NAVIGATION_SUCCESS;
  }
}

void AutomationProvider::OnRunUnloadHandlers(
    int handle, IPC::Message* reply_message) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->RunUnloadHandlers(reply_message);
  }
}

void AutomationProvider::OnSetZoomLevel(int handle, int zoom_level) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    if (tab->GetWebContents() && tab->GetWebContents()->GetRenderViewHost()) {
      RenderViewHost* host = tab->GetWebContents()->GetRenderViewHost();
      content::PageZoom zoom = static_cast<content::PageZoom>(zoom_level);
      host->Zoom(zoom);
    }
  }
}
