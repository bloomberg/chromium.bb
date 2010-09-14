// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/win_util.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>

#include "app/l10n_util.h"
#include "app/l10n_util_win.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_handle.h"
#include "base/scoped_handle_win.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "gfx/codec/png_codec.h"
#include "gfx/gdi_util.h"

// Ensure that we pick up this link library.
#pragma comment(lib, "dwmapi.lib")

namespace win_util {

const int kAutoHideTaskbarThicknessPx = 2;

namespace {

const wchar_t kShell32[] = L"shell32.dll";
const char kSHGetPropertyStoreForWindow[] = "SHGetPropertyStoreForWindow";

// Define the type of SHGetPropertyStoreForWindow is SHGPSFW.
typedef DECLSPEC_IMPORT HRESULT (STDAPICALLTYPE *SHGPSFW)(HWND hwnd,
                                                          REFIID riid,
                                                          void** ppv);
}  // namespace

std::wstring FormatSystemTime(const SYSTEMTIME& time,
                              const std::wstring& format) {
  // If the format string is empty, just use the default format.
  LPCTSTR format_ptr = NULL;
  if (!format.empty())
    format_ptr = format.c_str();

  int buffer_size = GetTimeFormat(LOCALE_USER_DEFAULT, NULL, &time, format_ptr,
                                  NULL, 0);

  std::wstring output;
  GetTimeFormat(LOCALE_USER_DEFAULT, NULL, &time, format_ptr,
                WriteInto(&output, buffer_size), buffer_size);

  return output;
}

std::wstring FormatSystemDate(const SYSTEMTIME& date,
                              const std::wstring& format) {
  // If the format string is empty, just use the default format.
  LPCTSTR format_ptr = NULL;
  if (!format.empty())
    format_ptr = format.c_str();

  int buffer_size = GetDateFormat(LOCALE_USER_DEFAULT, NULL, &date, format_ptr,
                                  NULL, 0);

  std::wstring output;
  GetDateFormat(LOCALE_USER_DEFAULT, NULL, &date, format_ptr,
                WriteInto(&output, buffer_size), buffer_size);

  return output;
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

bool ShouldUseVistaFrame() {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return false;
  // If composition is not enabled, we behave like on XP.
  BOOL f;
  DwmIsCompositionEnabled(&f);
  return !!f;
}

// Open an item via a shell execute command. Error code checking and casting
// explanation: http://msdn2.microsoft.com/en-us/library/ms647732.aspx
bool OpenItemViaShell(const FilePath& full_path) {
  HINSTANCE h = ::ShellExecuteW(
      NULL, NULL, full_path.value().c_str(), NULL,
      full_path.DirName().value().c_str(), SW_SHOWNORMAL);

  LONG_PTR error = reinterpret_cast<LONG_PTR>(h);
  if (error > 32)
    return true;

  if ((error == SE_ERR_NOASSOC))
    return OpenItemWithExternalApp(full_path.value());

  return false;
}

bool OpenItemViaShellNoZoneCheck(const FilePath& full_path) {
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_DDEWAIT;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = NULL;
  sei.lpFile = full_path.value().c_str();
  if (::ShellExecuteExW(&sei))
    return true;
  LONG_PTR error = reinterpret_cast<LONG_PTR>(sei.hInstApp);
  if ((error == SE_ERR_NOASSOC))
    return OpenItemWithExternalApp(full_path.value());
  return false;
}

// Show the Windows "Open With" dialog box to ask the user to pick an app to
// open the file with.
bool OpenItemWithExternalApp(const std::wstring& full_path) {
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_FLAG_DDEWAIT;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = L"openas";
  sei.lpFile = full_path.c_str();
  return (TRUE == ::ShellExecuteExW(&sei));
}

// Adjust the window to fit, returning true if the window was resized or moved.
static bool AdjustWindowToFit(HWND hwnd, const RECT& bounds) {
  // Get the monitor.
  HMONITOR hmon = MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST);
  if (!hmon) {
    NOTREACHED() << "Unable to find default monitor";
    // No monitor available.
    return false;
  }

  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hmon, &mi);
  gfx::Rect window_rect(bounds);
  gfx::Rect monitor_rect(mi.rcWork);
  gfx::Rect new_window_rect = window_rect.AdjustToFit(monitor_rect);
  if (!new_window_rect.Equals(window_rect)) {
    // Window doesn't fit on monitor, move and possibly resize.
    SetWindowPos(hwnd, 0, new_window_rect.x(), new_window_rect.y(),
                 new_window_rect.width(), new_window_rect.height(),
                 SWP_NOACTIVATE | SWP_NOZORDER);
    return true;
  } else {
    return false;
  }
}

void AdjustWindowToFit(HWND hwnd) {
  // Get the window bounds.
  RECT r;
  GetWindowRect(hwnd, &r);
  AdjustWindowToFit(hwnd, r);
}

void CenterAndSizeWindow(HWND parent, HWND window, const SIZE& pref,
                         bool pref_is_client) {
  DCHECK(window && pref.cx > 0 && pref.cy > 0);
  // Calculate the ideal bounds.
  RECT window_bounds;
  RECT center_bounds = {0};
  if (parent) {
    // If there is a parent, center over the parents bounds.
    ::GetWindowRect(parent, &center_bounds);
  } else {
    // No parent. Center over the monitor the window is on.
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(monitor, &mi);
      center_bounds = mi.rcWork;
    } else {
      NOTREACHED() << "Unable to get default monitor";
    }
  }
  window_bounds.left = center_bounds.left +
                       (center_bounds.right - center_bounds.left - pref.cx) / 2;
  window_bounds.right = window_bounds.left + pref.cx;
  window_bounds.top = center_bounds.top +
                      (center_bounds.bottom - center_bounds.top - pref.cy) / 2;
  window_bounds.bottom = window_bounds.top + pref.cy;

  // If we're centering a child window, we are positioning in client
  // coordinates, and as such we need to offset the target rectangle by the
  // position of the parent window.
  if (::GetWindowLong(window, GWL_STYLE) & WS_CHILD) {
    DCHECK(parent && ::GetParent(window) == parent);
    POINT topleft = { window_bounds.left, window_bounds.top };
    ::MapWindowPoints(HWND_DESKTOP, parent, &topleft, 1);
    window_bounds.left = topleft.x;
    window_bounds.top = topleft.y;
    window_bounds.right = window_bounds.left + pref.cx;
    window_bounds.bottom = window_bounds.top + pref.cy;
  }

  // Get the WINDOWINFO for window. We need the style to calculate the size
  // for the window.
  WINDOWINFO win_info = {0};
  win_info.cbSize = sizeof(WINDOWINFO);
  GetWindowInfo(window, &win_info);

  // Calculate the window size needed for the content size.

  if (!pref_is_client ||
      AdjustWindowRectEx(&window_bounds, win_info.dwStyle, FALSE,
                         win_info.dwExStyle)) {
    if (!AdjustWindowToFit(window, window_bounds)) {
      // The window fits, reset the bounds.
      SetWindowPos(window, 0, window_bounds.left, window_bounds.top,
                   window_bounds.right - window_bounds.left,
                   window_bounds.bottom - window_bounds.top,
                   SWP_NOACTIVATE | SWP_NOZORDER);
    }  // else case, AdjustWindowToFit set the bounds for us.
  } else {
    NOTREACHED() << "Unable to adjust window to fit";
  }
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

bool DoesWindowBelongToActiveWindow(HWND window) {
  DCHECK(window);
  HWND top_window = ::GetAncestor(window, GA_ROOT);
  if (!top_window)
    return false;

  HWND active_top_window = ::GetAncestor(::GetForegroundWindow(), GA_ROOT);
  return (top_window == active_top_window);
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

void SetChildBounds(HWND child_window, HWND parent_window,
                    HWND insert_after_window, const gfx::Rect& bounds,
                    int padding, unsigned long flags) {
  DCHECK(IsWindow(child_window));

  // First figure out the bounds of the parent.
  RECT parent_rect = {0};
  if (parent_window) {
    GetClientRect(parent_window, &parent_rect);
  } else {
    // If there is no parent, we consider the bounds of the monitor the window
    // is on to be the parent bounds.

    // If the child_window isn't visible yet and we've been given a valid,
    // visible insert after window, use that window to locate the correct
    // monitor instead.
    HWND window = child_window;
    if (!IsWindowVisible(window) && IsWindow(insert_after_window) &&
        IsWindowVisible(insert_after_window))
      window = insert_after_window;

    POINT window_point = { bounds.x(), bounds.y() };
    HMONITOR monitor = MonitorFromPoint(window_point,
                                        MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(monitor, &mi);
      parent_rect = mi.rcWork;
    } else {
      NOTREACHED() << "Unable to get default monitor";
    }
  }

  gfx::Rect actual_bounds = bounds;
  EnsureRectIsVisibleInRect(gfx::Rect(parent_rect), &actual_bounds, padding);

  SetWindowPos(child_window, insert_after_window, actual_bounds.x(),
               actual_bounds.y(), actual_bounds.width(),
               actual_bounds.height(), flags);
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
  ScopedHDC mem_hdc(CreateCompatibleDC(window_hdc));

  // Create a DIB that's the same size as the window.
  RECT content_rect = {0, 0, 0, 0};
  ::GetWindowRect(window_handle, &content_rect);
  content_rect.right++;  // Match what PrintWindow wants.
  int width = content_rect.right - content_rect.left;
  int height = content_rect.bottom - content_rect.top;
  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(width, height, &hdr);
  unsigned char *bit_ptr = NULL;
  ScopedBitmap bitmap(CreateDIBSection(mem_hdc,
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

bool IsReservedName(const std::wstring& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const wchar_t* const known_devices[] = {
    L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5",
    L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4",
    L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9", L"clock$"
  };
  std::wstring filename_lower = StringToLowerASCII(filename);

  for (int i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(std::wstring(known_devices[i]) + L".") == 0)
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

bool IsShellIntegratedExtension(const std::wstring& extension) {
  std::wstring extension_lower = StringToLowerASCII(extension);

  static const wchar_t* const integrated_extensions[] = {
    // See <http://msdn.microsoft.com/en-us/library/ms811694.aspx>.
    L"local",
    // Right-clicking on shortcuts can be magical.
    L"lnk",
  };

  for (int i = 0; i < arraysize(integrated_extensions); ++i) {
    if (extension_lower == integrated_extensions[i])
      return true;
  }

  // See <http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html>.
  // That vulnerability report is not exactly on point, but files become magical
  // if their end in a CLSID.  Here we block extensions that look like CLSIDs.
  if (extension_lower.size() > 0 && extension_lower.at(0) == L'{' &&
      extension_lower.at(extension_lower.length() - 1) == L'}')
    return true;

  return false;
}

// In addition to passing the RTL flags to ::MessageBox if we are running in an
// RTL locale, we need to make sure that LTR strings are rendered correctly by
// adding the appropriate Unicode directionality marks.
int MessageBox(HWND hwnd,
               const std::wstring& text,
               const std::wstring& caption,
               UINT flags) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoMessageBox))
    return IDOK;

  UINT actual_flags = flags;
  if (base::i18n::IsRTL())
    actual_flags |= MB_RIGHT | MB_RTLREADING;

  std::wstring localized_text;
  const wchar_t* text_ptr = text.c_str();
  if (base::i18n::AdjustStringForLocaleDirection(text, &localized_text))
    text_ptr = localized_text.c_str();

  std::wstring localized_caption;
  const wchar_t* caption_ptr = caption.c_str();
  if (base::i18n::AdjustStringForLocaleDirection(caption, &localized_caption))
    caption_ptr = localized_caption.c_str();

  return ::MessageBox(hwnd, text_ptr, caption_ptr, actual_flags);
}

gfx::Font GetWindowTitleFont() {
  NONCLIENTMETRICS ncm;
  win_util::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
}

void SetAppIdForWindow(const std::wstring& app_id, HWND hwnd) {
  // This functionality is only available on Win7+.
  if (win_util::GetWinVersion() < win_util::WINVERSION_WIN7)
    return;

  // Load Shell32.dll into memory.
  // TODO(brg): Remove this mechanism when the Win7 SDK is available in trunk.
  std::wstring shell32_filename(kShell32);
  FilePath shell32_filepath(shell32_filename);
  base::NativeLibrary shell32_library = base::LoadNativeLibrary(
      shell32_filepath);

  if (!shell32_library)
    return;

  // Get the function pointer for SHGetPropertyStoreForWindow.
  void* function = base::GetFunctionPointerFromNativeLibrary(
      shell32_library,
      kSHGetPropertyStoreForWindow);

  if (!function) {
    base::UnloadNativeLibrary(shell32_library);
    return;
  }

  // Set the application's name.
  ScopedComPtr<IPropertyStore> pps;
  SHGPSFW SHGetPropertyStoreForWindow = static_cast<SHGPSFW>(function);
  HRESULT result = SHGetPropertyStoreForWindow(
      hwnd, __uuidof(*pps), reinterpret_cast<void**>(pps.Receive()));
  if (S_OK == result) {
    SetAppIdForPropertyStore(pps, app_id.c_str());
  }

  // Cleanup.
  base::UnloadNativeLibrary(shell32_library);
}

}  // namespace win_util
