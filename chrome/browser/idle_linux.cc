// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/idle_query_linux.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/x/x11_util.h"

namespace {

class ScreensaverWindowFinder : public ui::EnumerateWindowsDelegate {
 public:
  ScreensaverWindowFinder()
      : exists_(false) {}

  bool exists() const { return exists_; }

 protected:
  virtual bool ShouldStopIterating(XID window) {
    if (!ui::IsWindowVisible(window) || !IsScreensaverWindow(window))
      return false;
    exists_ = true;
    return true;
  }

 private:
  bool IsScreensaverWindow(XID window) const {
    // It should occupy the full screen.
    if (!ui::IsX11WindowFullScreen(window))
      return false;

    // For xscreensaver, the window should have _SCREENSAVER_VERSION property.
    if (ui::PropertyExists(window, "_SCREENSAVER_VERSION"))
      return true;

    // For all others, like gnome-screensaver, the window's WM_CLASS property
    // should contain "screensaver".
    std::string value;
    if (!ui::GetStringProperty(window, "WM_CLASS", &value))
      return false;

    return value.find("screensaver") != std::string::npos;
  }

  bool exists_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverWindowFinder);
};

bool ScreensaverWindowExists() {
  ScreensaverWindowFinder finder;
  gtk_util::EnumerateTopLevelWindows(&finder);
  return finder.exists();
}

}

IdleState CalculateIdleState(unsigned int idle_threshold) {
  // Usually the screensaver is used to lock the screen, so we do not need to
  // check if the workstation is locked.
  gdk_error_trap_push();
  bool result = ScreensaverWindowExists();
  bool got_error = gdk_error_trap_pop();
  if (result && !got_error)
    return IDLE_STATE_LOCKED;

  browser_sync::IdleQueryLinux idle_query;
  unsigned int idle_time = idle_query.IdleTime();
  if (idle_time >= idle_threshold)
    return IDLE_STATE_IDLE;
  return IDLE_STATE_ACTIVE;
}
