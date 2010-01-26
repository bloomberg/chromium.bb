// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/engine/idle_query_linux.h"

#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

namespace browser_sync {

class IdleData {
 public:
  IdleData() {
    int event_base;
    int error_base;
    display = XOpenDisplay(NULL);
    if (XScreenSaverQueryExtension(display, &event_base, &error_base)) {
      mit_info = XScreenSaverAllocInfo();
    } else {
      mit_info = NULL;
    }
  }

  ~IdleData() {
    if (display) {
      XCloseDisplay(display);
      display = NULL;
    }
    if (mit_info) {
      XFree(mit_info);
    }
  }

  XScreenSaverInfo *mit_info;
  Display *display;
};

IdleQueryLinux::IdleQueryLinux() : idle_data_(new IdleData()) {
}

IdleQueryLinux::~IdleQueryLinux() {
}

int IdleQueryLinux::IdleTime() {
  if (!idle_data_->mit_info || !idle_data_->display) {
    return 0;
  }

  if (XScreenSaverQueryInfo(idle_data_->display,
                            RootWindow(idle_data_->display, 0),
                            idle_data_->mit_info)) {
    return (idle_data_->mit_info->idle) / 1000;
  } else {
    return 0;
  }
}
}  // namespace browser_sync
