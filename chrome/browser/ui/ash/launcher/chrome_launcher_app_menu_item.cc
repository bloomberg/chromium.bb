// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"

ChromeLauncherAppMenuItem::ChromeLauncherAppMenuItem(const string16 title,
                                                     const gfx::Image* icon)
    : title_(title),
      icon_(icon ? gfx::Image(*icon) : gfx::Image()) {
}

ChromeLauncherAppMenuItem::~ChromeLauncherAppMenuItem() {
}

bool ChromeLauncherAppMenuItem::IsActive() const {
  return false;
}

bool ChromeLauncherAppMenuItem::IsEnabled() const {
  return false;
}

void ChromeLauncherAppMenuItem::Execute() {
}
