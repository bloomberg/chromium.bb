// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "base/keyboard_codes.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/external_tab_container.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/test/automation/automation_messages.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

// This task just adds another task to the event queue.  This is useful if
// you want to ensure that any tasks added to the event queue after this one
// have already been processed by the time |task| is run.
class InvokeTaskLaterTask : public Task {
 public:
  explicit InvokeTaskLaterTask(Task* task) : task_(task) {}
  virtual ~InvokeTaskLaterTask() {}

  virtual void Run() {
    MessageLoop::current()->PostTask(FROM_HERE, task_);
  }

 private:
  Task* task_;

  DISALLOW_COPY_AND_ASSIGN(InvokeTaskLaterTask);
};

static void MoveMouse(const POINT& point) {
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
  DCHECK_EQ(point.x, GET_X_LPARAM(pos));
  DCHECK_EQ(point.y, GET_Y_LPARAM(pos));
#endif
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM l_param) {
  if (hwnd == reinterpret_cast<HWND>(l_param)) {
    return FALSE;
  }
  return TRUE;
}

void AutomationProvider::GetActiveWindow(int* handle) {
  HWND window = GetForegroundWindow();

  // Let's make sure this window belongs to our process.
  if (EnumThreadWindows(::GetCurrentThreadId(),
                        EnumThreadWndProc,
                        reinterpret_cast<LPARAM>(window))) {
    // We enumerated all the windows and did not find the foreground window,
    // it is not our window, ignore it.
    *handle = 0;
    return;
  }

  *handle = window_tracker_->Add(window);
}

// This task enqueues a mouse event on the event loop, so that the view
// that it's being sent to can do the requisite post-processing.
class MouseEventTask : public Task {
 public:
  MouseEventTask(views::View* view,
                 views::Event::EventType type,
                 const gfx::Point& point,
                 int flags)
      : view_(view), type_(type), point_(point), flags_(flags) {}
  virtual ~MouseEventTask() {}

  virtual void Run() {
    views::MouseEvent event(type_, point_.x(), point_.y(), flags_);
    // We need to set the cursor position before we process the event because
    // some code (tab dragging, for instance) queries the actual cursor location
    // rather than the location of the mouse event. Note that the reason why
    // the drag code moved away from using mouse event locations was because
    // our conversion to screen location doesn't work well with multiple
    // monitors, so this only works reliably in a single monitor setup.
    gfx::Point screen_location(point_.x(), point_.y());
    view_->ConvertPointToScreen(view_, &screen_location);
    MoveMouse(screen_location.ToPOINT());
    switch (type_) {
      case views::Event::ET_MOUSE_PRESSED:
        view_->OnMousePressed(event);
        break;

      case views::Event::ET_MOUSE_DRAGGED:
        view_->OnMouseDragged(event);
        break;

      case views::Event::ET_MOUSE_RELEASED:
        view_->OnMouseReleased(event, false);
        break;

      default:
        NOTREACHED();
    }
  }

 private:
  views::View* view_;
  views::Event::EventType type_;
  gfx::Point point_;
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventTask);
};

void AutomationProvider::ScheduleMouseEvent(views::View* view,
                                            views::Event::EventType type,
                                            const gfx::Point& point,
                                            int flags) {
  MessageLoop::current()->PostTask(FROM_HERE,
      new MouseEventTask(view, type, point, flags));
}

// This task sends a WindowDragResponse message with the appropriate
// routing ID to the automation proxy.  This is implemented as a task so that
// we know that the mouse events (and any tasks that they spawn on the message
// loop) have been processed by the time this is sent.
class WindowDragResponseTask : public Task {
 public:
  WindowDragResponseTask(AutomationProvider* provider,
                         IPC::Message* reply_message)
      : provider_(provider), reply_message_(reply_message) {}
  virtual ~WindowDragResponseTask() {}

  virtual void Run() {
    DCHECK(reply_message_ != NULL);
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message_, true);
    provider_->Send(reply_message_);
  }

 private:
  AutomationProvider* provider_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(WindowDragResponseTask);
};

void AutomationProvider::WindowSimulateDrag(int handle,
                                            std::vector<gfx::Point> drag_path,
                                            int flags,
                                            bool press_escape_en_route,
                                            IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(handle) && (drag_path.size() > 1)) {
    gfx::NativeWindow window =
        browser_tracker_->GetResource(handle)->window()->GetNativeHandle();

    UINT down_message = 0;
    UINT up_message = 0;
    WPARAM wparam_flags = 0;
    if (flags & views::Event::EF_SHIFT_DOWN)
      wparam_flags |= MK_SHIFT;
    if (flags & views::Event::EF_CONTROL_DOWN)
      wparam_flags |= MK_CONTROL;
    if (flags & views::Event::EF_LEFT_BUTTON_DOWN) {
      wparam_flags |= MK_LBUTTON;
      down_message = WM_LBUTTONDOWN;
      up_message = WM_LBUTTONUP;
    }
    if (flags & views::Event::EF_MIDDLE_BUTTON_DOWN) {
      wparam_flags |= MK_MBUTTON;
      down_message = WM_MBUTTONDOWN;
      up_message = WM_MBUTTONUP;
    }
    if (flags & views::Event::EF_RIGHT_BUTTON_DOWN) {
      wparam_flags |= MK_RBUTTON;
      down_message = WM_LBUTTONDOWN;
      up_message = WM_LBUTTONUP;
    }

    Browser* browser = browser_tracker_->GetResource(handle);
    DCHECK(browser);
    HWND top_level_hwnd =
        reinterpret_cast<HWND>(browser->window()->GetNativeHandle());
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
      // Press Escape.
      ui_controls::SendKeyPress(window, base::VKEY_ESCAPE,
                               ((flags & views::Event::EF_CONTROL_DOWN)
                                == views::Event::EF_CONTROL_DOWN),
                               ((flags & views::Event::EF_SHIFT_DOWN) ==
                                views::Event::EF_SHIFT_DOWN),
                               ((flags & views::Event::EF_ALT_DOWN) ==
                                views::Event::EF_ALT_DOWN));
    }
    SendMessage(top_level_hwnd, up_message, wparam_flags,
                MAKELPARAM(end.x, end.y));

    MessageLoop::current()->PostTask(FROM_HERE, new InvokeTaskLaterTask(
        new WindowDragResponseTask(this, reply_message)));
  } else {
    AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
    Send(reply_message);
  }
}

void AutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  *view_id = -1;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(hwnd);
    DCHECK(focus_manager);
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      *view_id = focused_view->GetID();
  }
}

void AutomationProvider::GetBookmarkBarVisibility(int handle, bool* visible,
                                                  bool* animating) {
  *visible = false;
  *animating = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BrowserWindowTesting* testing =
          browser->window()->GetBrowserWindowTesting();
      BookmarkBarView* bookmark_bar = testing->GetBookmarkBarView();
      if (bookmark_bar) {
        *animating = bookmark_bar->IsAnimating();
        *visible = browser->window()->IsBookmarkBarVisible();
      }
    }
  }
}

void AutomationProvider::GetWindowBounds(int handle, gfx::Rect* bounds,
                                         bool* success) {
  *success = false;
  HWND hwnd = window_tracker_->GetResource(handle);
  if (hwnd) {
    *success = true;
    WINDOWPLACEMENT window_placement;
    GetWindowPlacement(hwnd, &window_placement);
    *bounds = window_placement.rcNormalPosition;
  }
}

void AutomationProvider::SetWindowBounds(int handle, const gfx::Rect& bounds,
                                         bool* success) {
  *success = false;
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    if (::MoveWindow(hwnd, bounds.x(), bounds.y(), bounds.width(),
                     bounds.height(), true)) {
      *success = true;
    }
  }
}

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) {
  if (window_tracker_->ContainsHandle(handle)) {
    HWND hwnd = window_tracker_->GetResource(handle);
    ::ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    *result = true;
  } else {
    *result = false;
  }
}

void AutomationProvider::ActivateWindow(int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    ::SetActiveWindow(window_tracker_->GetResource(handle));
  }
}

void AutomationProvider::IsWindowMaximized(int handle, bool* is_maximized,
                                           bool* success) {
  *success = false;

  HWND hwnd = window_tracker_->GetResource(handle);
  if (hwnd) {
    *success = true;
    WINDOWPLACEMENT window_placement;
    GetWindowPlacement(hwnd, &window_placement);
    *is_maximized = (window_placement.showCmd == SW_MAXIMIZE);
  }
}

void AutomationProvider::GetTabHWND(int handle, HWND* tab_hwnd) {
  *tab_hwnd = NULL;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    *tab_hwnd = tab->tab_contents()->GetNativeView();
  }
}

void AutomationProvider::CreateExternalTab(
    const IPC::ExternalTabSettings& settings,
    gfx::NativeWindow* tab_container_window, gfx::NativeWindow* tab_window,
    int* tab_handle) {
  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;
  scoped_refptr<ExternalTabContainer> external_tab_container =
      new ExternalTabContainer(this, automation_resource_message_filter_);

  Profile* profile = settings.is_off_the_record ?
      profile_->GetOffTheRecordProfile() : profile_;

  // When the ExternalTabContainer window is created we grab a reference on it
  // which is released when the window is destroyed.
  external_tab_container->Init(profile, settings.parent, settings.dimensions,
      settings.style, settings.load_requests_via_automation,
      settings.handle_top_level_requests, NULL, settings.initial_url);

  if (AddExternalTab(external_tab_container)) {
    TabContents* tab_contents = external_tab_container->tab_contents();
    *tab_handle = external_tab_container->tab_handle();
    *tab_container_window = external_tab_container->GetNativeView();
    *tab_window = tab_contents->GetNativeView();
  } else {
    external_tab_container->Uninitialize();
  }
}

bool AutomationProvider::AddExternalTab(ExternalTabContainer* external_tab) {
  DCHECK(external_tab != NULL);

  TabContents* tab_contents = external_tab->tab_contents();
  if (tab_contents) {
    int tab_handle = tab_tracker_->Add(&tab_contents->controller());
    external_tab->set_tab_handle(tab_handle);
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
                                         int handle, bool reverse) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->FocusThroughTabTraversal(reverse);
  }
  // This message expects no response.
}

void AutomationProvider::PrintAsync(int tab_handle) {
  NavigationController* tab = NULL;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, &tab);
  if (tab_contents) {
    if (tab_contents->PrintNow())
      return;
  }
}

ExternalTabContainer* AutomationProvider::GetExternalTabForHandle(int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    return ExternalTabContainer::GetContainerForTab(
        tab->tab_contents()->GetNativeView());
  }

  return NULL;
}

void AutomationProvider::OnTabReposition(
    int tab_handle, const IPC::Reposition_Params& params) {
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
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    if (!tab) {
      NOTREACHED();
      return;
    }

    TabContents* tab_contents = tab->tab_contents();
    if (!tab_contents || !tab_contents->delegate()) {
      NOTREACHED();
      return;
    }

    tab_contents->delegate()->ExecuteContextMenuCommand(command);
  }
}

void AutomationProvider::ConnectExternalTab(
    intptr_t cookie,
    gfx::NativeWindow* tab_container_window,
    gfx::NativeWindow* tab_window,
    int* tab_handle) {
  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;

  scoped_refptr<ExternalTabContainer> external_tab_container =
      ExternalTabContainer::RemovePendingTab(cookie);
  if (!external_tab_container.get()) {
    NOTREACHED();
    return;
  }

  if (AddExternalTab(external_tab_container)) {
    external_tab_container->Reinitialize(this,
                                         automation_resource_message_filter_);
    TabContents* tab_contents = external_tab_container->tab_contents();
    *tab_handle = external_tab_container->tab_handle();
    external_tab_container->set_tab_handle(*tab_handle);
    *tab_container_window = external_tab_container->GetNativeView();
    *tab_window = tab_contents->GetNativeView();
  } else {
    external_tab_container->Uninitialize();
  }
}

void AutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    HWND window = browser->window()->GetNativeHandle();
    *success = (::PostMessageW(window, WM_ENDSESSION, 0, 0) == TRUE);
  }
}

void AutomationProvider::SetEnableExtensionAutomation(
    int tab_handle,
    const std::vector<std::string>& functions_enabled) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(tab_handle);
  if (external_tab) {
    external_tab->SetEnableExtensionAutomation(functions_enabled);
  } else {
    // Tab must exist, and must be an external tab so that its
    // delegate has an on-empty
    // implementation of ForwardMessageToExternalHost.
    DLOG(WARNING) <<
      "SetEnableExtensionAutomation called with invalid tab handle.";
  }
}
