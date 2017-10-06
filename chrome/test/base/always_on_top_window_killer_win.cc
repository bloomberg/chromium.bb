// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/always_on_top_window_killer_win.h"

#include <Windows.h>

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"

namespace {

const char kDialogFoundBeforeTest[] =
    "There is an always on top dialog on the desktop. This was most likely "
    "caused by a previous test and may cause this test to fail. Trying to "
    "close it.";

const char kDialogFoundPostTest[] =
    "There is an always on top dialog on the desktop after running this test. "
    "This was most likely caused by this test and may cause future tests to "
    "fail, trying to close it.";

const char kWindowFoundBeforeTest[] =
    "There is an always on top window on the desktop before running the test. "
    "This may have been caused by a previous test and may cause this test to "
    "fail, class-name=";

const char kWindowFoundPostTest[] =
    "There is an always on top window on the desktop after running the test. "
    "This may have been caused by this test or a previous test and may cause "
    "flake, class-name=";

BOOL CALLBACK AlwaysOnTopWindowProc(HWND hwnd, LPARAM l_param) {
  const BOOL kContinueIterating = TRUE;

  if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
    return kContinueIterating;

  const LONG ex_styles = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (ex_styles & WS_EX_TOPMOST) {
    wchar_t class_name_chars[512];
    if (GetClassName(hwnd, class_name_chars, arraysize(class_name_chars))) {
      const std::wstring class_name(class_name_chars);
      const RunType run_type = *reinterpret_cast<RunType*>(l_param);
      // "#32770" is used for system dialogs, such as happens if a child
      // process triggers an assert().
      if (class_name == L"#32770") {
        LOG(ERROR) << (run_type == RunType::BEFORE_TEST ? kDialogFoundBeforeTest
                                                        : kDialogFoundPostTest);
        // We don't own the dialog, so we can't destroy it. CloseWindow()
        // results in iconifying the window. An alternative may be to focus it,
        // then send return and wait for close. As we reboot machines running
        // interactive ui tests at least every 12 hours we're going with the
        // simple for now.
        CloseWindow(hwnd);
      } else if (class_name != L"Button" && class_name != L"Shell_TrayWnd") {
        // 'Button' is the start button, and 'Shell_TrayWnd' the taskbar.
        //
        // These windows may be problematic as well, but in theory tests should
        // not be creating an always on top window that outlives the test. Log
        // the window in case there are problems.
        DWORD process_id = 0;
        DWORD thread_id = GetWindowThreadProcessId(hwnd, &process_id);

        base::Process process = base::Process::Open(process_id);
        base::string16 process_path(MAX_PATH, L'\0');
        if (process.IsValid()) {
          // It's possible that the actual process owning |hwnd| has gone away
          // and that a new process using the same PID has appeared. If this
          // turns out to be an issue, we could fetch the process start time
          // here and compare it with the time just before getting |thread_id|
          // above. This is likely overkill for diagnostic purposes.
          DWORD str_len = process_path.size();
          if (!::QueryFullProcessImageName(process.Handle(), 0,
                                           &process_path[0], &str_len) ||
              str_len >= MAX_PATH) {
            str_len = 0;
          }
          process_path.resize(str_len);
        }
        LOG(ERROR) << (run_type == RunType::BEFORE_TEST ? kWindowFoundBeforeTest
                                                        : kWindowFoundPostTest)
                   << class_name << " process_id=" << process_id
                   << " thread_id=" << thread_id
                   << " process_path=" << process_path;
        return kContinueIterating;
      }
    }
  }
  return kContinueIterating;
}

}  // namespace

void KillAlwaysOnTopWindows(RunType run_type) {
  EnumWindows(AlwaysOnTopWindowProc, reinterpret_cast<LPARAM>(&run_type));
}
