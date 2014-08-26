// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_finder_win.h"

#include <shellapi.h>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/message_window.h"
#include "base/win/metro.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"


namespace {

const int kTimeoutInSeconds = 20;

// The following is copied from net/base/escape.cc. We don't want to depend on
// net here because this gets compiled into chrome.exe to facilitate
// fast-rendezvous (see https://codereview.chromium.org/14617003/).

// TODO(koz): Move these functions out of net/base/escape.cc into base/escape.cc
// so we can depend on it directly.

// BEGIN COPY from net/base/escape.cc

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
struct Charmap {
  bool Contains(unsigned char c) const {
    return ((map[c >> 5] & (1 << (c & 31))) != 0);
  }

  uint32 map[8];
};

const char kHexString[] = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20.
std::string Escape(const std::string& text, const Charmap& charmap,
                   bool use_plus) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
static const Charmap kQueryCharmap = {{
  0xffffffffL, 0xfc00987dL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};

std::string EscapeQueryParamValue(const std::string& text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

// END COPY from net/base/escape.cc

}  // namespace

namespace chrome {

HWND FindRunningChromeWindow(const base::FilePath& user_data_dir) {
  return base::win::MessageWindow::FindWindow(user_data_dir.value());
}

NotifyChromeResult AttemptToNotifyRunningChrome(HWND remote_window,
                                                bool fast_start) {
  DCHECK(remote_window);
  static const char kSearchUrl[] =
      "http://www.google.com/search?q=%s&sourceid=chrome&ie=UTF-8";
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(remote_window, &process_id);
  if (!thread_id || !process_id)
    return NOTIFY_FAILED;

#if !defined(USE_AURA)
  if (base::win::IsMetroProcess()) {
    // Interesting corner case. We are launched as a metro process but we
    // found another chrome running. Since metro enforces single instance then
    // the other chrome must be desktop chrome and this must be a search charm
    // activation. This scenario is unique; other cases should be properly
    // handled by the delegate_execute which will not activate a second chrome.
    base::string16 terms;
    base::win::MetroLaunchType launch = base::win::GetMetroLaunchParams(&terms);
    if (launch != base::win::METRO_SEARCH) {
      LOG(WARNING) << "In metro mode, but and launch is " << launch;
    } else {
      std::string query = EscapeQueryParamValue(base::UTF16ToUTF8(terms), true);
      std::string url = base::StringPrintf(kSearchUrl, query.c_str());
      SHELLEXECUTEINFOA sei = { sizeof(sei) };
      sei.fMask = SEE_MASK_FLAG_LOG_USAGE;
      sei.nShow = SW_SHOWNORMAL;
      sei.lpFile = url.c_str();
      OutputDebugStringA(sei.lpFile);
      sei.lpDirectory = "";
      ::ShellExecuteExA(&sei);
    }
    return NOTIFY_SUCCESS;
  }

  base::win::ScopedHandle process_handle;
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      base::OpenProcessHandleWithAccess(
          process_id, PROCESS_QUERY_INFORMATION,
          process_handle.Receive())) {
    // Receive() causes the process handle to be set in the destructor of the
    // temporary receiver object, which does not happen until after the if
    // statement is complete.  So IsProcessImmersive() should only be checked
    // as part of a separate if statement.
    if (base::win::IsProcessImmersive(process_handle.Get()))
      chrome::ActivateMetroChrome();
  }
#endif

  CommandLine command_line(*CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(
      switches::kOriginalProcessStartTime,
      base::Int64ToString(
          base::CurrentProcessInfo::CreationTime().ToInternalValue()));

  if (fast_start)
    command_line.AppendSwitch(switches::kFastStart);

  // Send the command line to the remote chrome window.
  // Format is "START\0<<<current directory>>>\0<<<commandline>>>".
  std::wstring to_send(L"START\0", 6);  // want the NULL in the string.
  base::FilePath cur_dir;
  if (!base::GetCurrentDirectory(&cur_dir))
    return NOTIFY_FAILED;
  to_send.append(cur_dir.value());
  to_send.append(L"\0", 1);  // Null separator.
  to_send.append(command_line.GetCommandLineString());
  to_send.append(L"\0", 1);  // Null separator.

  // Allow the current running browser window to make itself the foreground
  // window (otherwise it will just flash in the taskbar).
  ::AllowSetForegroundWindow(process_id);

  COPYDATASTRUCT cds;
  cds.dwData = 0;
  cds.cbData = static_cast<DWORD>((to_send.length() + 1) * sizeof(wchar_t));
  cds.lpData = const_cast<wchar_t*>(to_send.c_str());
  DWORD_PTR result = 0;
  if (::SendMessageTimeout(remote_window,
                           WM_COPYDATA,
                           NULL,
                           reinterpret_cast<LPARAM>(&cds),
                           SMTO_ABORTIFHUNG,
                           kTimeoutInSeconds * 1000,
                           &result)) {
    return result ? NOTIFY_SUCCESS : NOTIFY_FAILED;
  }

  // It is possible that the process owning this window may have died by now.
  if (!::IsWindow(remote_window))
    return NOTIFY_FAILED;

  // If the window couldn't be notified but still exists, assume it is hung.
  return NOTIFY_WINDOW_HUNG;
}

}  // namespace chrome
