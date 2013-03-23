// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "chrome/app/chrome_dll_resource.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Maximum length of "Your desktop is shared with ..." message in UTF-16
// characters.
const size_t kMaxSharingWithTextLength = 100;

const wchar_t kShellTrayWindowName[] = L"Shell_TrayWnd";
const int kWindowBorderRadius = 14;

} // namespace anonymous

class ScreenCaptureNotificationUIWin : public ScreenCaptureNotificationUI {
 public:
  ScreenCaptureNotificationUIWin();
  virtual ~ScreenCaptureNotificationUIWin();

  // ScreenCaptureNotificationUI interface.
  virtual bool Show(const base::Closure& stop_callback,
                    const string16& title) OVERRIDE;

 private:
  static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wparam,
                                     LPARAM lparam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // Creates the dialog window.
  bool BeginDialog(const string16& title);

  // Closes the dialog and invokes the disconnect callback, if set.
  void EndDialog();

  // Trys to position the dialog window above the taskbar.
  void SetDialogPosition();

  // Applies localization string and resizes the dialog.
  bool UpdateStrings(const string16& title);

  base::Closure stop_callback_;
  HWND hwnd_;
  base::win::ScopedGDIObject<HPEN> border_pen_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIWin);
};

static int GetControlTextWidth(HWND control) {
  RECT rect = {0, 0, 0, 0};
  WCHAR text[256];
  int result = GetWindowText(control, text, arraysize(text));
  if (result) {
    base::win::ScopedGetDC dc(control);
    base::win::ScopedSelectObject font(
        dc, (HFONT)SendMessage(control, WM_GETFONT, 0, 0));
    DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_SINGLELINE);
  }
  return rect.right;
}

ScreenCaptureNotificationUIWin::ScreenCaptureNotificationUIWin()
    : hwnd_(NULL),
      border_pen_(CreatePen(PS_SOLID, 5,
                            RGB(0.13 * 255, 0.69 * 255, 0.11 * 255))) {
}

ScreenCaptureNotificationUIWin::~ScreenCaptureNotificationUIWin() {
  stop_callback_.Reset();
  EndDialog();
}

bool ScreenCaptureNotificationUIWin::Show(
    const base::Closure& stop_callback,
    const string16& title) {
  DCHECK(stop_callback_.is_null());
  DCHECK(!stop_callback.is_null());

  stop_callback_ = stop_callback;

  if (BeginDialog(title)) {
    return true;
  } else {
    stop_callback_ = stop_callback;
    EndDialog();
    return false;
  }
}

INT_PTR CALLBACK ScreenCaptureNotificationUIWin::DialogProc(
    HWND hwnd, UINT message,
    WPARAM wparam, LPARAM lparam) {
  LONG_PTR self = NULL;
  if (message == WM_INITDIALOG) {
    self = lparam;

    // Store |this| to the window's user data.
    SetLastError(ERROR_SUCCESS);
    LONG_PTR result = SetWindowLongPtr(hwnd, DWLP_USER, self);
    if (result == 0 && GetLastError() != ERROR_SUCCESS)
      reinterpret_cast<ScreenCaptureNotificationUIWin*>(self)->EndDialog();
  } else {
    self = GetWindowLongPtr(hwnd, DWLP_USER);
  }

  if (self) {
    return reinterpret_cast<ScreenCaptureNotificationUIWin*>(self)->
        OnDialogMessage(hwnd, message, wparam, lparam);
  }
  return FALSE;
}

BOOL ScreenCaptureNotificationUIWin::OnDialogMessage(HWND hwnd, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
  switch (message) {
    // Ignore close messages.
    case WM_CLOSE:
      return TRUE;

    // Handle the Stop button.
    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDC_SCREEN_CAPTURE_STOP:
          EndDialog();
          return TRUE;
      }
      return FALSE;

    // Ensure we don't try to use the HWND anymore.
    case WM_DESTROY:
      hwnd_ = NULL;

      // Ensure that the stop callback is invoked even if somehow our window
      // gets destroyed.
      EndDialog();

      return TRUE;

    // Ensure the dialog stays visible if the work area dimensions change.
    case WM_SETTINGCHANGE:
      if (wparam == SPI_SETWORKAREA)
        SetDialogPosition();
      return TRUE;

    // Ensure the dialog stays visible if the display dimensions change.
    case WM_DISPLAYCHANGE:
      SetDialogPosition();
      return TRUE;

    // Let the window be draggable by its client area by responding
    // that the entire window is the title bar.
    case WM_NCHITTEST:
      SetWindowLongPtr(hwnd, DWLP_MSGRESULT, HTCAPTION);
      return TRUE;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd_, &ps);
      RECT rect;
      GetClientRect(hwnd_, &rect);
      {
        base::win::ScopedSelectObject border(hdc, border_pen_);
        base::win::ScopedSelectObject brush(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rect.left, rect.top, rect.right - 1, rect.bottom - 1,
                  kWindowBorderRadius, kWindowBorderRadius);
      }
      EndPaint(hwnd_, &ps);
      return TRUE;
    }
  }
  return FALSE;
}

bool ScreenCaptureNotificationUIWin::BeginDialog(const string16& title) {
  DCHECK(!hwnd_);

  // TODO(sergeyu): Currently this code relies on resources for the dialog. Fix
  // it to work without resources.

  // Load the dialog resource so that we can modify the RTL flags if necessary.
  HMODULE module = base::GetModuleFromAddress(&DialogProc);
  HRSRC dialog_resource = FindResource(
      module, MAKEINTRESOURCE(IDD_SCREEN_CAPTURE_NOTIFICATION), RT_DIALOG);
  if (!dialog_resource)
    return false;

  HGLOBAL dialog_template = LoadResource(module, dialog_resource);
  if (!dialog_template)
    return false;

  DLGTEMPLATE* dialog_pointer =
      reinterpret_cast<DLGTEMPLATE*>(LockResource(dialog_template));
  if (!dialog_pointer)
    return false;

  // The actual resource type is DLGTEMPLATEEX, but this is not defined in any
  // standard headers, so we treat it as a generic pointer and manipulate the
  // correct offsets explicitly.
  scoped_array<unsigned char> rtl_dialog_template;
  if (base::i18n::IsRTL()) {
    unsigned long dialog_template_size =
        SizeofResource(module, dialog_resource);
    rtl_dialog_template.reset(new unsigned char[dialog_template_size]);
    memcpy(rtl_dialog_template.get(), dialog_pointer, dialog_template_size);
    DWORD* rtl_dwords = reinterpret_cast<DWORD*>(rtl_dialog_template.get());
    rtl_dwords[2] |= (WS_EX_LAYOUTRTL | WS_EX_RTLREADING);
    dialog_pointer = reinterpret_cast<DLGTEMPLATE*>(rtl_dwords);
  }

  hwnd_ = CreateDialogIndirectParam(module, dialog_pointer, NULL,
                                    DialogProc, reinterpret_cast<LPARAM>(this));
  if (!hwnd_)
    return false;

  if (!UpdateStrings(title))
    return false;

  SetDialogPosition();
  ShowWindow(hwnd_, SW_SHOW);
  return IsWindowVisible(hwnd_) != FALSE;
}

void ScreenCaptureNotificationUIWin::EndDialog() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }

  if (!stop_callback_.is_null()) {
    stop_callback_.Run();
    stop_callback_.Reset();
  }
}

void ScreenCaptureNotificationUIWin::SetDialogPosition() {
  // Try to center the window above the task-bar. If that fails, use the
  // primary monitor. If that fails (very unlikely), use the default position.
  HWND taskbar = FindWindow(kShellTrayWindowName, NULL);
  HMONITOR monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  RECT window_rect;
  if (GetMonitorInfo(monitor, &monitor_info) &&
      GetWindowRect(hwnd_, &window_rect)) {
    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    int top = monitor_info.rcWork.bottom - window_height;
    int left = (monitor_info.rcWork.right + monitor_info.rcWork.left -
        window_width) / 2;
    SetWindowPos(hwnd_, NULL, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }
}

bool ScreenCaptureNotificationUIWin::UpdateStrings(const string16& title) {
  string16 window_title =
      l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TITLE,
                                 title);
  if (!SetWindowText(hwnd_, window_title.c_str()))
    return false;

  // Localize the disconnect button text and measure length of the old and new
  // labels.
  HWND stop_button = GetDlgItem(hwnd_, IDC_SCREEN_CAPTURE_STOP);
  if (!stop_button)
    return false;
  int button_old_required_width = GetControlTextWidth(stop_button);
  string16 button_label =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  if (!SetWindowText(stop_button, button_label.c_str()))
    return false;
  int button_new_required_width = GetControlTextWidth(stop_button);
  int button_width_delta =
      button_new_required_width - button_old_required_width;

  // Format and truncate "Your desktop is shared with ..." message.

  string16 text = l10n_util::GetStringFUTF16(
      IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT, title);
  if (text.length() > kMaxSharingWithTextLength)
    text.erase(kMaxSharingWithTextLength);

  // Set localized and truncated "Your desktop is shared with ..." message and
  // measure length of the old and new text.
  HWND sharing_with_label = GetDlgItem(hwnd_, IDC_SCREEN_CAPTURE_TEXT);
  if (!sharing_with_label)
    return false;
  int label_old_required_width = GetControlTextWidth(sharing_with_label);
  if (!SetWindowText(sharing_with_label, text.c_str()))
    return false;
  int label_new_required_width = GetControlTextWidth(sharing_with_label);
  int label_width_delta = label_new_required_width - label_old_required_width;

  // Reposition the controls such that the label lies to the left of the
  // disconnect button (assuming LTR layout). The dialog template determines
  // the controls' spacing; update their size to fit the localized content.
  RECT label_rect;
  if (!GetClientRect(sharing_with_label, &label_rect))
    return false;
  if (!SetWindowPos(sharing_with_label, NULL, 0, 0,
                    label_rect.right + label_width_delta, label_rect.bottom,
                    SWP_NOMOVE | SWP_NOZORDER)) {
    return false;
  }

  // Reposition the disconnect button as well.
  RECT button_rect;
  if (!GetWindowRect(stop_button, &button_rect))
    return false;
  int button_width = button_rect.right - button_rect.left;
  int button_height = button_rect.bottom - button_rect.top;
  SetLastError(ERROR_SUCCESS);
  int result = MapWindowPoints(HWND_DESKTOP, hwnd_,
                               reinterpret_cast<LPPOINT>(&button_rect), 2);
  if (!result && GetLastError() != ERROR_SUCCESS)
    return false;
  if (!SetWindowPos(stop_button, NULL,
                    button_rect.left + label_width_delta, button_rect.top,
                    button_width + button_width_delta, button_height,
                    SWP_NOZORDER)) {
    return false;
  }

  // Resize the whole window to fit the resized controls.
  RECT window_rect;
  if (!GetWindowRect(hwnd_, &window_rect))
    return false;
  int width = (window_rect.right - window_rect.left) +
      button_width_delta + label_width_delta;
  int height = window_rect.bottom - window_rect.top;
  if (!SetWindowPos(hwnd_, NULL, 0, 0, width, height,
                    SWP_NOMOVE | SWP_NOZORDER)) {
    return false;
  }

  // Make the corners of the disconnect window rounded.
  HRGN rgn = CreateRoundRectRgn(0, 0, width, height, kWindowBorderRadius,
                                kWindowBorderRadius);
  if (!rgn)
    return false;
  if (!SetWindowRgn(hwnd_, rgn, TRUE))
    return false;

  return true;
}

scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create() {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUIWin());
}
