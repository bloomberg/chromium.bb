// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/win/win_util.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/win_util.h"
#include "gfx/codec/png_codec.h"
#include "gfx/font.h"
#include "gfx/gdi_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"

namespace app {
namespace win {

const int kAutoHideTaskbarThicknessPx = 2;

string16 FormatSystemTime(const SYSTEMTIME& time, const string16& format) {
  // If the format string is empty, just use the default format.
  LPCTSTR format_ptr = NULL;
  if (!format.empty())
    format_ptr = format.c_str();

  int buffer_size = GetTimeFormat(LOCALE_USER_DEFAULT, NULL, &time, format_ptr,
                                  NULL, 0);

  string16 output;
  GetTimeFormat(LOCALE_USER_DEFAULT, NULL, &time, format_ptr,
                WriteInto(&output, buffer_size), buffer_size);

  return output;
}

string16 FormatSystemDate(const SYSTEMTIME& date, const string16& format) {
  // If the format string is empty, just use the default format.
  LPCTSTR format_ptr = NULL;
  if (!format.empty())
    format_ptr = format.c_str();

  int buffer_size = GetDateFormat(LOCALE_USER_DEFAULT, NULL, &date, format_ptr,
                                  NULL, 0);

  string16 output;
  GetDateFormat(LOCALE_USER_DEFAULT, NULL, &date, format_ptr,
                WriteInto(&output, buffer_size), buffer_size);

  return output;
}

bool ConvertToLongPath(const string16& short_path,
                       string16* long_path) {
  wchar_t long_path_buf[MAX_PATH];
  DWORD return_value = GetLongPathName(short_path.c_str(), long_path_buf,
                                       MAX_PATH);
  if (return_value != 0 && return_value < MAX_PATH) {
    *long_path = long_path_buf;
    return true;
  }

  return false;
}

bool IsDoubleClick(const POINT& origin,
                   const POINT& current,
                   DWORD elapsed_time) {
  // The CXDOUBLECLK and CYDOUBLECLK system metrics describe the width and
  // height of a rectangle around the origin position, inside of which clicks
  // within the double click time are considered double clicks.
  return (elapsed_time <= GetDoubleClickTime()) &&
      (abs(current.x - origin.x) <= (GetSystemMetrics(SM_CXDOUBLECLK) / 2)) &&
      (abs(current.y - origin.y) <= (GetSystemMetrics(SM_CYDOUBLECLK) / 2));
}

bool IsDrag(const POINT& origin, const POINT& current) {
  // The CXDRAG and CYDRAG system metrics describe the width and height of a
  // rectangle around the origin position, inside of which motion is not
  // considered a drag.
  return (abs(current.x - origin.x) > (GetSystemMetrics(SM_CXDRAG) / 2)) ||
         (abs(current.y - origin.y) > (GetSystemMetrics(SM_CYDRAG) / 2));
}

bool EdgeHasTopmostAutoHideTaskbar(UINT edge, HMONITOR monitor) {
  APPBARDATA taskbar_data = { 0 };
  taskbar_data.cbSize = sizeof APPBARDATA;
  taskbar_data.uEdge = edge;
  HWND taskbar = reinterpret_cast<HWND>(SHAppBarMessage(ABM_GETAUTOHIDEBAR,
                                                        &taskbar_data));
  return ::IsWindow(taskbar) && (monitor != NULL) &&
      (MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL) == monitor) &&
      (GetWindowLong(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST);
}

HANDLE GetSectionFromProcess(HANDLE section, HANDLE process, bool read_only) {
  HANDLE valid_section = NULL;
  DWORD access = STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ;
  if (!read_only)
    access |= FILE_MAP_WRITE;
  DuplicateHandle(process, section, GetCurrentProcess(), &valid_section, access,
                  FALSE, 0);
  return valid_section;
}

HANDLE GetSectionForProcess(HANDLE section, HANDLE process, bool read_only) {
  HANDLE valid_section = NULL;
  DWORD access = STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ;
  if (!read_only)
    access |= FILE_MAP_WRITE;
  DuplicateHandle(GetCurrentProcess(), section, process, &valid_section, access,
                  FALSE, 0);
  return valid_section;
}

void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding) {
  DCHECK(child_rect);

  // We use padding here because it allows some of the original web page to
  // bleed through around the edges.
  int twice_padding = padding * 2;

  // FIRST, clamp width and height so we don't open child windows larger than
  // the containing parent.
  if (child_rect->width() > (parent_rect.width() + twice_padding))
    child_rect->set_width(std::max(0, parent_rect.width() - twice_padding));
  if (child_rect->height() > parent_rect.height() + twice_padding)
    child_rect->set_height(std::max(0, parent_rect.height() - twice_padding));

  // SECOND, clamp x,y position to padding,padding so we don't position child
  // windows in hyperspace.
  // TODO(mpcomplete): I don't see what the second check in each 'if' does that
  // isn't handled by the LAST set of 'ifs'.  Maybe we can remove it.
  if (child_rect->x() < parent_rect.x() ||
      child_rect->x() > parent_rect.right()) {
    child_rect->set_x(parent_rect.x() + padding);
  }
  if (child_rect->y() < parent_rect.y() ||
      child_rect->y() > parent_rect.bottom()) {
    child_rect->set_y(parent_rect.y() + padding);
  }

  // LAST, nudge the window back up into the client area if its x,y position is
  // within the parent bounds but its width/height place it off-screen.
  if (child_rect->bottom() > parent_rect.bottom())
    child_rect->set_y(parent_rect.bottom() - child_rect->height() - padding);
  if (child_rect->right() > parent_rect.right())
    child_rect->set_x(parent_rect.right() - child_rect->width() - padding);
}

gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect) {
  RECT p_rect = rect.ToRECT();
  HMONITOR monitor = MonitorFromRect(&p_rect, MONITOR_DEFAULTTONEAREST);
  if (monitor) {
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);
    return gfx::Rect(mi.rcWork);
  }
  NOTREACHED();
  return gfx::Rect();
}

bool IsNumPadDigit(int key_code, bool extended_key) {
  if (key_code >= VK_NUMPAD0 && key_code <= VK_NUMPAD9)
    return true;

  // Check for num pad keys without NumLock.
  // Note: there is no easy way to know if a the key that was pressed comes from
  //       the num pad or the rest of the keyboard.  Investigating how
  //       TranslateMessage() generates the WM_KEYCHAR from an
  //       ALT + <NumPad sequences> it appears it looks at the extended key flag
  //       (which is on if the key pressed comes from one of the 3 clusters to
  //       the left of the numeric keypad).  So we use it as well.
  return !extended_key &&
            ((key_code >= VK_PRIOR && key_code <= VK_DOWN) ||  // All keys but 5
                                                               // and 0.
            (key_code == VK_CLEAR) ||  // Key 5.
            (key_code == VK_INSERT));  // Key 0.
}

void GrabWindowSnapshot(HWND window_handle,
                        std::vector<unsigned char>* png_representation) {
  // Create a memory DC that's compatible with the window.
  HDC window_hdc = GetWindowDC(window_handle);
  base::win::ScopedHDC mem_hdc(CreateCompatibleDC(window_hdc));

  // Create a DIB that's the same size as the window.
  RECT content_rect = {0, 0, 0, 0};
  ::GetWindowRect(window_handle, &content_rect);
  content_rect.right++;  // Match what PrintWindow wants.
  int width = content_rect.right - content_rect.left;
  int height = content_rect.bottom - content_rect.top;
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(width, height, &hdr);
  unsigned char *bit_ptr = NULL;
  base::win::ScopedBitmap bitmap(
      CreateDIBSection(mem_hdc,
                       reinterpret_cast<BITMAPINFO*>(&hdr),
                       DIB_RGB_COLORS,
                       reinterpret_cast<void **>(&bit_ptr),
                       NULL, 0));

  SelectObject(mem_hdc, bitmap);
  // Clear the bitmap to white (so that rounded corners on windows
  // show up on a white background, and strangely-shaped windows
  // look reasonable). Not capturing an alpha mask saves a
  // bit of space.
  PatBlt(mem_hdc, 0, 0, width, height, WHITENESS);
  // Grab a copy of the window
  // First, see if PrintWindow is defined (it's not in Windows 2000).
  typedef BOOL (WINAPI *PrintWindowPointer)(HWND, HDC, UINT);
  PrintWindowPointer print_window =
      reinterpret_cast<PrintWindowPointer>(
          GetProcAddress(GetModuleHandle(L"User32.dll"), "PrintWindow"));

  // If PrintWindow is defined, use it.  It will work on partially
  // obscured windows, and works better for out of process sub-windows.
  // Otherwise grab the bits we can get with BitBlt; it's better
  // than nothing and will work fine in the average case (window is
  // completely on screen).
  if (print_window)
    (*print_window)(window_handle, mem_hdc, 0);
  else
    BitBlt(mem_hdc, 0, 0, width, height, window_hdc, 0, 0, SRCCOPY);

  // We now have a copy of the window contents in a DIB, so
  // encode it into a useful format for posting to the bug report
  // server.
  gfx::PNGCodec::Encode(bit_ptr, gfx::PNGCodec::FORMAT_BGRA,
                        width, height, width * 4, true,
                        png_representation);

  ReleaseDC(window_handle, window_hdc);
}

bool IsWindowActive(HWND hwnd) {
  WINDOWINFO info;
  return ::GetWindowInfo(hwnd, &info) &&
         ((info.dwWindowStatus & WS_ACTIVECAPTION) != 0);
}

bool IsReservedName(const string16& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const wchar_t* const known_devices[] = {
    L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5",
    L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4",
    L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9", L"clock$"
  };
  string16 filename_lower = StringToLowerASCII(filename);

  for (int i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(string16(known_devices[i]) + L".") == 0)
      return true;
  }

  static const wchar_t* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    L"desktop.ini",
    L"thumbs.db",
  };

  for (int i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }

  return false;
}

// In addition to passing the RTL flags to ::MessageBox if we are running in an
// RTL locale, we need to make sure that LTR strings are rendered correctly by
// adding the appropriate Unicode directionality marks.
int MessageBox(HWND hwnd,
               const string16& text,
               const string16& caption,
               UINT flags) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoMessageBox))
    return IDOK;

  UINT actual_flags = flags;
  if (base::i18n::IsRTL())
    actual_flags |= MB_RIGHT | MB_RTLREADING;

  string16 localized_text = text;
  base::i18n::AdjustStringForLocaleDirection(&localized_text);
  const wchar_t* text_ptr = localized_text.c_str();

  string16 localized_caption = caption;
  base::i18n::AdjustStringForLocaleDirection(&localized_caption);
  const wchar_t* caption_ptr = localized_caption.c_str();

  return ::MessageBox(hwnd, text_ptr, caption_ptr, actual_flags);
}

gfx::Font GetWindowTitleFont() {
  NONCLIENTMETRICS ncm;
  base::win::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  base::win::ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
}

}  // namespace win
}  // namespace app
