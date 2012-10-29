// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/other_device_menu_cocoa.h"

OtherDeviceMenuCocoa::OtherDeviceMenuCocoa(ui::SimpleMenuModel* menu_model)
    : menu_model_(menu_model) {
}

OtherDeviceMenuCocoa::~OtherDeviceMenuCocoa() {
}

// TODO(jeremycho): Implement this.
void OtherDeviceMenuCocoa::ShowMenu(gfx::NativeWindow window,
                                    const gfx::Point& location) {
}

// OtherDeviceMenuController ---------------------------------------------------

// static
OtherDeviceMenu* OtherDeviceMenu::Create(ui::SimpleMenuModel* menu_model) {
  return new OtherDeviceMenuCocoa(menu_model);
}
