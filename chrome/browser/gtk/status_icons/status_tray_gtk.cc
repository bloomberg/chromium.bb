// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/status_icons/status_tray_gtk.h"

#include "chrome/browser/gtk/status_icons/status_icon_gtk.h"

StatusTrayGtk::StatusTrayGtk() {
}

StatusTrayGtk::~StatusTrayGtk() {
}

StatusIcon* StatusTrayGtk::CreateStatusIcon() {
  return new StatusIconGtk();
}

StatusTray* StatusTray::Create() {
  return new StatusTrayGtk();
}
