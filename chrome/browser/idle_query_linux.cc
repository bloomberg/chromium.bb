// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle_query_linux.h"

#include <X11/extensions/scrnsaver.h>
#include "ui/base/x/x11_util.h"

namespace browser {

class IdleData {
 public:
  IdleData() {
    int event_base;
    int error_base;
    if (XScreenSaverQueryExtension(ui::GetXDisplay(), &event_base,
                                   &error_base)) {
      mit_info = XScreenSaverAllocInfo();
    } else {
      mit_info = NULL;
    }
  }

  ~IdleData() {
    if (mit_info)
      XFree(mit_info);
  }

  XScreenSaverInfo *mit_info;
};

IdleQueryLinux::IdleQueryLinux() : idle_data_(new IdleData()) {}

IdleQueryLinux::~IdleQueryLinux() {}

int IdleQueryLinux::IdleTime() {
  if (!idle_data_->mit_info)
    return 0;

  if (XScreenSaverQueryInfo(ui::GetXDisplay(),
                            RootWindow(ui::GetXDisplay(), 0),
                            idle_data_->mit_info)) {
    return (idle_data_->mit_info->idle) / 1000;
  } else {
    return 0;
  }
}

}  // namespace browser
