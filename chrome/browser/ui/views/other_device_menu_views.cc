// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/other_device_menu_views.h"

#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

OtherDeviceMenuViews::OtherDeviceMenuViews(ui::SimpleMenuModel* menu_model)
    : menu_model_(menu_model) {
}

OtherDeviceMenuViews::~OtherDeviceMenuViews() {
}

void OtherDeviceMenuViews::ShowMenu(gfx::NativeWindow window,
                                   const gfx::Point& location) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget)
    return;

  views::MenuModelAdapter menu_model_adapter(menu_model_);
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));

  if (menu_runner_->RunMenuAt(widget, NULL, gfx::Rect(location, gfx::Size()),
                              views::MenuItemView::TOPLEFT, 0) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }
}

// OtherDeviceMenuController ---------------------------------------------------

// static
OtherDeviceMenu* OtherDeviceMenu::Create(ui::SimpleMenuModel* menu_model) {
  return new OtherDeviceMenuViews(menu_model);
}
