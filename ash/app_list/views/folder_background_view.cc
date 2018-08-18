// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/folder_background_view.h"

#include "ash/app_list/views/app_list_folder_view.h"

namespace app_list {

FolderBackgroundView::FolderBackgroundView(AppListFolderView* folder_view)
    : folder_view_(folder_view) {}

FolderBackgroundView::~FolderBackgroundView() = default;

bool FolderBackgroundView::OnMousePressed(const ui::MouseEvent& event) {
  HandleClickOrTap();
  return true;
}

void FolderBackgroundView::OnGestureEvent(ui::GestureEvent* event) {
  // A fix for the current folder close animation should be implemented to allow
  // for a folder to close while pages are changing. Until this, we should
  // always close the folder before movement.
  // https://crbug.com/875133
  HandleClickOrTap();
  event->SetHandled();
}

void FolderBackgroundView::HandleClickOrTap() {
  folder_view_->CloseFolderPage();
}

}  // namespace app_list
