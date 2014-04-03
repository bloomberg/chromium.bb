// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_empty.h"

#include "ui/views/layout/box_layout.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/view.h"

namespace {

class EmptyBackground : public views::Background {
 public:
  EmptyBackground() {}
  virtual ~EmptyBackground() {}

 private:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(EmptyBackground);
};

}

namespace ash {

TrayEmpty::TrayEmpty(SystemTray* system_tray)
    : SystemTrayItem(system_tray) {
}

TrayEmpty::~TrayEmpty() {}

views::View* TrayEmpty::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayEmpty::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  views::View* view = new views::View;
  view->set_background(new EmptyBackground());
  view->SetBorder(views::Border::CreateEmptyBorder(10, 0, 0, 0));
  view->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
        0, 0, 0));
  view->SetPaintToLayer(true);
  view->SetFillsBoundsOpaquely(false);
  return view;
}

views::View* TrayEmpty::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayEmpty::DestroyTrayView() {}

void TrayEmpty::DestroyDefaultView() {}

void TrayEmpty::DestroyDetailedView() {}

void TrayEmpty::UpdateAfterLoginStatusChange(user::LoginStatus status) {}

}  // namespace ash
