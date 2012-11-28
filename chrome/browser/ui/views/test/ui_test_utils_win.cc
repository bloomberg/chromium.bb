// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "ui/base/win/foreground_helper.h"
#include "ui/ui_controls/ui_controls.h"
#include "ui/views/focus/focus_manager.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/test/ui_test_utils_aura.h"
#include "ui/aura/root_window.h"
#endif

namespace ui_test_utils {

namespace {

const char kSnapshotBaseName[] = "ChromiumSnapshot";
const char kSnapshotExtension[] = ".png";

FilePath GetSnapshotFileName(const FilePath& snapshot_directory) {
  base::Time::Exploded the_time;

  base::Time::Now().LocalExplode(&the_time);
  std::string filename(StringPrintf("%s%04d%02d%02d%02d%02d%02d%s",
      kSnapshotBaseName, the_time.year, the_time.month, the_time.day_of_month,
      the_time.hour, the_time.minute, the_time.second, kSnapshotExtension));

  FilePath snapshot_file = snapshot_directory.AppendASCII(filename);
  if (file_util::PathExists(snapshot_file)) {
    int index = 0;
    std::string suffix;
    FilePath trial_file;
    do {
      suffix = StringPrintf(" (%d)", ++index);
      trial_file = snapshot_file.InsertBeforeExtensionASCII(suffix);
    } while (file_util::PathExists(trial_file));
    snapshot_file = trial_file;
  }
  return snapshot_file;
}

}  // namespace

void HideNativeWindow(gfx::NativeWindow window) {
#if defined(USE_AURA)
  if (chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH) {
    HideNativeWindowAura(window);
    return;
  }
  HWND hwnd = window->GetRootWindow()->GetAcceleratedWidget();
#else
  HWND hwnd = window;
#endif
  ::ShowWindow(hwnd, SW_HIDE);
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
#if defined(USE_AURA)
  if (chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH)
    ShowAndFocusNativeWindowAura(window);
  // Always make sure the window hosting ash is visible and focused.
  HWND hwnd = window->GetRootWindow()->GetAcceleratedWidget();
#else
  HWND hwnd = window;
#endif

  ::ShowWindow(hwnd, SW_SHOW);

  if (GetForegroundWindow() != hwnd) {
    VLOG(1) << "Forcefully refocusing front window";
    ui::ForegroundHelper::SetForeground(hwnd);
  }

  // ShowWindow does not necessarily activate the window. In particular if a
  // window from another app is the foreground window then the request to
  // activate the window fails. See SetForegroundWindow for details.
  return GetForegroundWindow() == hwnd;
}

bool SaveScreenSnapshotToDirectory(const FilePath& directory,
                                   FilePath* screenshot_path) {
  bool succeeded = false;
  FilePath out_path(GetSnapshotFileName(directory));

  MONITORINFO monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  HMONITOR main_monitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
  if (GetMonitorInfo(main_monitor, &monitor_info)) {
    RECT& rect = monitor_info.rcMonitor;

    std::vector<unsigned char> png_data;
    gfx::Rect bounds(
        gfx::Size(rect.right - rect.left, rect.bottom - rect.top));
    if (chrome::internal::GrabWindowSnapshot(NULL, &png_data, bounds) &&
        png_data.size() <= INT_MAX) {
      int bytes = static_cast<int>(png_data.size());
      int written = file_util::WriteFile(
          out_path, reinterpret_cast<char*>(&png_data[0]), bytes);
      succeeded = (written == bytes);
    }
  }

  if (succeeded && screenshot_path != NULL)
    *screenshot_path = out_path;

  return succeeded;
}

bool SaveScreenSnapshotToDesktop(FilePath* screenshot_path) {
  FilePath desktop;

  return PathService::Get(base::DIR_USER_DESKTOP, &desktop) &&
      SaveScreenSnapshotToDirectory(desktop, screenshot_path);
}

}  // namespace ui_test_utils
