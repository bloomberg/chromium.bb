// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"

#include "base/logging.h"

StatusIconLinuxDbus::StatusIconLinuxDbus(const gfx::ImageSkia& image,
                                         const base::string16& tool_tip) {}

StatusIconLinuxDbus::~StatusIconLinuxDbus() = default;

void StatusIconLinuxDbus::SetImage(const gfx::ImageSkia& image) {
  NOTIMPLEMENTED();
}

void StatusIconLinuxDbus::SetToolTip(const base::string16& tool_tip) {
  NOTIMPLEMENTED();
}

void StatusIconLinuxDbus::UpdatePlatformContextMenu(ui::MenuModel* model) {
  NOTIMPLEMENTED();
}
