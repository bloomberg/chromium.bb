// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_linux.h"

#ifndef OS_CHROMEOS
#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"
#include "ui/linux_ui/linux_ui.h"

StatusTrayLinux::StatusTrayLinux() {
}

StatusTrayLinux::~StatusTrayLinux() {
}

StatusIcon* StatusTrayLinux::CreatePlatformStatusIcon(
    StatusIconType type,
    const gfx::ImageSkia& image,
    const string16& tool_tip) {
  return StatusIconLinuxWrapper::CreateWrappedStatusIcon(image, tool_tip);
}

StatusTray* StatusTray::Create() {
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();

  // Only create a status tray if we can actually create status icons.
  if (linux_ui && linux_ui->IsStatusIconSupported())
    return new StatusTrayLinux();
  return NULL;
}
#else
StatusTray* StatusTray::Create() {
  return NULL;
}
#endif
