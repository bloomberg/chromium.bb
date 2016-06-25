// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_empty.h"

#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

class EmptyBackground : public views::Background {
 public:
  EmptyBackground() {}
  ~EmptyBackground() override {}

 private:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {}

  DISALLOW_COPY_AND_ASSIGN(EmptyBackground);
};
}

namespace ash {

TrayEmpty::TrayEmpty(SystemTray* system_tray) : SystemTrayItem(system_tray) {}

TrayEmpty::~TrayEmpty() {}

views::View* TrayEmpty::CreateTrayView(LoginStatus status) {
  return NULL;
}

views::View* TrayEmpty::CreateDefaultView(LoginStatus status) {
  if (status == LoginStatus::NOT_LOGGED_IN)
    return NULL;

  views::View* view = new views::View;
  view->set_background(new EmptyBackground());
  view->SetBorder(views::Border::CreateEmptyBorder(10, 0, 0, 0));
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  view->SetPaintToLayer(true);
  view->layer()->SetFillsBoundsOpaquely(false);
  return view;
}

views::View* TrayEmpty::CreateDetailedView(LoginStatus status) {
  return NULL;
}

void TrayEmpty::DestroyTrayView() {}

void TrayEmpty::DestroyDefaultView() {}

void TrayEmpty::DestroyDetailedView() {}

void TrayEmpty::UpdateAfterLoginStatusChange(LoginStatus status) {}

}  // namespace ash
