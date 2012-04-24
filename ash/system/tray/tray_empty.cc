// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_empty.h"

#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

TrayEmpty::TrayEmpty() {}

TrayEmpty::~TrayEmpty() {}

views::View* TrayEmpty::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayEmpty::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  views::View* view = new views::View;
  view->set_background(views::Background::CreateSolidBackground(
        SkColorSetARGB(0, 0, 0, 0)));
  view->set_border(views::Border::CreateEmptyBorder(10, 0, 0, 0));
  view->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
        0, 0, 0));
  return view;
}

views::View* TrayEmpty::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayEmpty::DestroyTrayView() {}

void TrayEmpty::DestroyDefaultView() {}

void TrayEmpty::DestroyDetailedView() {}

void TrayEmpty::UpdateAfterLoginStatusChange(user::LoginStatus status) {}

}  // namespace internal
}  // namespace ash
