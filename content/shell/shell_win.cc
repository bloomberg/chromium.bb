// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include <windows.h>
#include <commctrl.h>

#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/win/resource_util.h"
#include "content/shell/resource.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_chromium_resources.h"
#include "net/base/net_module.h"
#include "ui/base/win/hwnd_util.h"

namespace {

static const wchar_t kWindowTitle[] = L"Content Shell";
static const wchar_t kWindowClass[] = L"CONTENT_SHELL";

static const int kButtonWidth = 72;
static const int kURLBarHeight = 24;

static const int kMaxURLLength = 1024;

static base::StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::win::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                              &data_size)
      ? base::StringPiece(static_cast<char*>(data_ptr), data_size)
      : base::StringPiece();
}

}  // namespace

namespace content {

HINSTANCE Shell::instance_handle_;

void Shell::PlatformInitialize() {
  instance_handle_ = ::GetModuleHandle(NULL);

  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);
  RegisterWindowClass();
}

base::StringPiece Shell::PlatformResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}

void Shell::PlatformCleanUp() {
  // When the window is destroyed, tell the Edit field to forget about us,
  // otherwise we will crash.
  ui::SetWindowProc(edit_window_, default_edit_wnd_proc_);
  ui::SetWindowUserData(edit_window_, NULL);
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  int id;
  switch (control) {
    case BACK_BUTTON:
      id = IDC_NAV_BACK;
      break;
    case FORWARD_BUTTON:
      id = IDC_NAV_FORWARD;
      break;
    case STOP_BUTTON:
      id = IDC_NAV_STOP;
      break;
    default:
      NOTREACHED() << "Unknown UI control";
      return;
  }
  EnableWindow(GetDlgItem(main_window_, id), is_enabled);
}

void Shell::PlatformCreateWindow() {
  main_window_ = CreateWindow(kWindowClass, kWindowTitle,
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                              NULL, NULL, instance_handle_, NULL);
  ui::SetWindowUserData(main_window_, this);

  HWND hwnd;
  int x = 0;

  hwnd = CreateWindow(L"BUTTON", L"Back",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      main_window_, (HMENU) IDC_NAV_BACK, instance_handle_, 0);
  x += kButtonWidth;

  hwnd = CreateWindow(L"BUTTON", L"Forward",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      main_window_, (HMENU) IDC_NAV_FORWARD, instance_handle_,
                      0);
  x += kButtonWidth;

  hwnd = CreateWindow(L"BUTTON", L"Reload",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      main_window_, (HMENU) IDC_NAV_RELOAD, instance_handle_,
                      0);
  x += kButtonWidth;

  hwnd = CreateWindow(L"BUTTON", L"Stop",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, kButtonWidth, kURLBarHeight,
                      main_window_, (HMENU) IDC_NAV_STOP, instance_handle_, 0);
  x += kButtonWidth;

  // This control is positioned by PlatformResizeSubViews.
  edit_window_ = CreateWindow(L"EDIT", 0,
                              WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                              ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                              x, 0, 0, 0, main_window_, 0, instance_handle_, 0);

  default_edit_wnd_proc_ = ui::SetWindowProc(edit_window_, Shell::EditWndProc);
  ui::SetWindowUserData(edit_window_, this);

  ShowWindow(main_window_, SW_SHOW);
}

void Shell::PlatformSizeTo(int width, int height) {
  RECT rc, rw;
  GetClientRect(main_window_, &rc);
  GetWindowRect(main_window_, &rw);

  int client_width = rc.right - rc.left;
  int window_width = rw.right - rw.left;
  window_width = (window_width - client_width) + width;

  int client_height = rc.bottom - rc.top;
  int window_height = rw.bottom - rw.top;
  window_height = (window_height - client_height) + height;

  // Add space for the url bar.
  window_height += kURLBarHeight;

  SetWindowPos(main_window_, NULL, 0, 0, window_width, window_height,
               SWP_NOMOVE | SWP_NOZORDER);
}

void Shell::PlatformResizeSubViews() {
  RECT rc;
  GetClientRect(main_window_, &rc);

  int x = kButtonWidth * 4;
  MoveWindow(edit_window_, x, 0, rc.right - x, kURLBarHeight, TRUE);

  MoveWindow(GetContentView(), 0, kURLBarHeight, rc.right,
             rc.bottom - kURLBarHeight, TRUE);
}

ATOM Shell::RegisterWindowClass() {
  WNDCLASSEX wcex = {
      sizeof(WNDCLASSEX),
      CS_HREDRAW | CS_VREDRAW,
      Shell::WndProc,
      0,
      0,
      instance_handle_,
      NULL,
      LoadCursor(NULL, IDC_ARROW),
      0,
      MAKEINTRESOURCE(IDC_CONTENTSHELL),
      kWindowClass,
      NULL,
    };
  return RegisterClassEx(&wcex);
}

LRESULT CALLBACK Shell::WndProc(HWND hwnd, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  Shell* shell = static_cast<Shell*>(ui::GetWindowUserData(hwnd));

  switch (message) {
    case WM_COMMAND: {
      int id = LOWORD(wParam);
      switch (id) {
        case IDM_EXIT:
          DestroyWindow(hwnd);
          break;
        case IDC_NAV_BACK:
          shell->GoBackOrForward(-1);
          break;
        case IDC_NAV_FORWARD:
          shell->GoBackOrForward(1);
          break;
        case IDC_NAV_RELOAD:
        case IDC_NAV_STOP: {
          if (id == IDC_NAV_RELOAD) {
            shell->Reload();
          } else {
            shell->Stop();
          }
          break;
        }
        break;
      }
      break;
    }
    case WM_DESTROY: {
      delete shell;
      if (!shell_count_)
        MessageLoop::current()->Quit();
      return 0;
    }

    case WM_SIZE: {
      if (shell->GetContentView())
        shell->PlatformResizeSubViews();
      return 0;
    }
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK Shell::EditWndProc(HWND hwnd, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
  Shell* shell = static_cast<Shell*>(ui::GetWindowUserData(hwnd));

  switch (message) {
    case WM_CHAR:
      if (wParam == VK_RETURN) {
        wchar_t str[kMaxURLLength + 1];  // Leave room for adding a NULL;
        *(str) = kMaxURLLength;
        LRESULT str_len = SendMessage(hwnd, EM_GETLINE, 0, (LPARAM)str);
        if (str_len > 0) {
          str[str_len] = 0;  // EM_GETLINE doesn't NULL terminate.
          shell->LoadURL(GURL(str));
        }

        return 0;
      }
  }

  return CallWindowProc(shell->default_edit_wnd_proc_, hwnd, message, wParam,
                        lParam);
}

}  // namespace content
