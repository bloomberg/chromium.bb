// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon.h"

#include "ui/base/models/menu_model.h"

StatusIcon::StatusIcon()
{
}

StatusIcon::~StatusIcon() {
}

void StatusIcon::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StatusIcon::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool StatusIcon::HasObservers() {
  return observers_.size() > 0;
}

void StatusIcon::DispatchClickEvent() {
  FOR_EACH_OBSERVER(Observer, observers_, OnClicked());
}

void StatusIcon::SetContextMenu(ui::MenuModel* menu) {
  context_menu_contents_.reset(menu);
  UpdatePlatformContextMenu(menu);
}
