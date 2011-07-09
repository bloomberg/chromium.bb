// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_H_
#pragma once

#include <map>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/models/menu_model.h"
#include "views/controls/menu/menu_model_adapter.h"

class Profile;

class AvatarMenu : public base::RefCounted<AvatarMenu>,
                   public views::MenuModelAdapter {
 public:
  AvatarMenu(ui::MenuModel* model, Profile* profile);
  virtual ~AvatarMenu();

  // Shows the menu relative to the specified view.
  void RunMenu(views::MenuButton* host);

 private:
  // The views menu.
  scoped_ptr<views::MenuItemView> root_;

  // Profile the menu is being shown for.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_H_
