// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <commctrl.h>

#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "chrome/common/chrome_constants.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/win/hwnd_util.h"
#include "win8/util/win8_util.h"

static const UINT kStatusIconMessage = WM_APP + 1;

namespace {
// |kBaseIconId| is 2 to avoid conflicts with plugins that hard-code id 1.
const UINT kBaseIconId = 2;

UINT ReservedIconId(StatusTray::StatusIconType type) {
  return kBaseIconId + static_cast<UINT>(type);
}
}  // namespace

StatusTrayWin::StatusTrayWin()
    : next_icon_id_(1),
      atom_(0),
      instance_(NULL),
      window_(NULL) {
  // Register our window class
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      chrome::kStatusTrayWindowClass,
      &base::win::WrappedWindowProc<StatusTrayWin::WndProcStatic>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  atom_ = RegisterClassEx(&window_class);
  CHECK(atom_);

  // If the taskbar is re-created after we start up, we have to rebuild all of
  // our icons.
  taskbar_created_message_ = RegisterWindowMessage(TEXT("TaskbarCreated"));

  // Create an offscreen window for handling messages for the status icons. We
  // create a hidden WS_POPUP window instead of an HWND_MESSAGE window, because
  // only top-level windows such as popups can receive broadcast messages like
  // "TaskbarCreated".
  window_ = CreateWindow(MAKEINTATOM(atom_),
                         0, WS_POPUP, 0, 0, 0, 0, 0, 0, instance_, 0);
  gfx::CheckWindowCreated(window_);
  gfx::SetWindowUserData(window_, this);
}

LRESULT CALLBACK StatusTrayWin::WndProcStatic(HWND hwnd,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam) {
  StatusTrayWin* msg_wnd = reinterpret_cast<StatusTrayWin*>(
      GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (msg_wnd)
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  else
    return ::DefWindowProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK StatusTrayWin::WndProc(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  if (message == taskbar_created_message_) {
    // We need to reset all of our icons because the taskbar went away.
    for (StatusIcons::const_iterator i(status_icons().begin());
         i != status_icons().end(); ++i) {
      StatusIconWin* win_icon = static_cast<StatusIconWin*>(*i);
      win_icon->ResetIcon();
    }
    return TRUE;
  } else if (message == kStatusIconMessage) {
    StatusIconWin* win_icon = NULL;

    // Find the selected status icon.
    for (StatusIcons::const_iterator i(status_icons().begin());
         i != status_icons().end();
         ++i) {
      StatusIconWin* current_win_icon = static_cast<StatusIconWin*>(*i);
      if (current_win_icon->icon_id() == wparam) {
        win_icon = current_win_icon;
        break;
      }
    }

    // It is possible for this procedure to be called with an obsolete icon
    // id.  In that case we should just return early before handling any
    // actions.
    if (!win_icon)
      return TRUE;

    switch (lparam) {
      case TB_INDETERMINATE:
        win_icon->HandleBalloonClickEvent();
        return TRUE;

      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_CONTEXTMENU:
        // Walk our icons, find which one was clicked on, and invoke its
        // HandleClickEvent() method.
        gfx::Point cursor_pos(
            gfx::Screen::GetNativeScreen()->GetCursorScreenPoint());
        win_icon->HandleClickEvent(cursor_pos, lparam == WM_LBUTTONDOWN);
        return TRUE;
    }
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

StatusTrayWin::~StatusTrayWin() {
  if (window_)
    DestroyWindow(window_);

  if (atom_)
    UnregisterClass(MAKEINTATOM(atom_), instance_);
}

StatusIcon* StatusTrayWin::CreatePlatformStatusIcon(
    StatusTray::StatusIconType type,
    const gfx::ImageSkia& image,
    const string16& tool_tip) {
  UINT next_icon_id;
  if (type == StatusTray::OTHER_ICON)
    next_icon_id = NextIconId();
  else
    next_icon_id = ReservedIconId(type);

  StatusIcon* icon = NULL;
  if (win8::IsSingleWindowMetroMode())
    icon = new StatusIconMetro(next_icon_id);
  else
    icon = new StatusIconWin(next_icon_id, window_, kStatusIconMessage);

  icon->SetImage(image);
  icon->SetToolTip(tool_tip);
  return icon;
}

UINT StatusTrayWin::NextIconId() {
  UINT icon_id = next_icon_id_++;
  return kBaseIconId + static_cast<UINT>(NAMED_STATUS_ICON_COUNT) + icon_id;
}

StatusTray* StatusTray::Create() {
  return new StatusTrayWin();
}
