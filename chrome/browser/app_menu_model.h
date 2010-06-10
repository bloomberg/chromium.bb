// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MENU_MODEL_H_
#define CHROME_BROWSER_APP_MENU_MODEL_H_

#include <set>
#include <vector>

#include "app/menus/simple_menu_model.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class Browser;

// A menu model that builds the contents of the app menu.
class AppMenuModel : public menus::SimpleMenuModel {
 public:
  explicit AppMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                        Browser* browser);
  virtual ~AppMenuModel();

  // Overridden from menus::SimpleMenuModel:
  virtual bool IsLabelDynamicAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool HasIcons() const { return true; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const;

 private:
  void Build();

  string16 GetSyncMenuLabel() const;
  string16 GetAboutEntryMenuLabel() const;
  bool IsDynamicItem(int index) const;

  // Profile names that are in profiles_menu_contents_. This is used to
  // detect profile change.
  std::vector<std::wstring> known_profiles_;

  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(AppMenuModel);
};

#endif  // CHROME_BROWSER_APP_MENU_MODEL_H_
