// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"

#include "ui/base/models/menu_model.h"

scoped_ptr<ui::MenuModel> CreateMultiUserContextMenu(aura::Window* window) {
  scoped_ptr<ui::MenuModel> menu_model;
  return menu_model.Pass();
}

void ExecuteVisitDesktopCommand(int command_id, aura::Window* window) {}
