// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"

ChromeLauncherAppMenuItem::ChromeLauncherAppMenuItem(
    const base::string16 title,
    const gfx::Image* icon,
    bool has_leading_separator)
    : title_(title),
      icon_(icon ? gfx::Image(*icon) : gfx::Image()),
      has_leading_separator_(has_leading_separator) {
}

ChromeLauncherAppMenuItem::~ChromeLauncherAppMenuItem() {
}

bool ChromeLauncherAppMenuItem::IsActive() const {
  return false;
}

bool ChromeLauncherAppMenuItem::IsEnabled() const {
  return false;
}

void ChromeLauncherAppMenuItem::Execute(int event_flags) {
}
