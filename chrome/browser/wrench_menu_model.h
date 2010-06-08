// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WRENCH_MENU_MODEL_H_
#define CHROME_BROWSER_WRENCH_MENU_MODEL_H_

#include <set>
#include <vector>

#include "app/menus/simple_menu_model.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/user_data_manager.h"

class Browser;
class EncodingMenuModel;

class ToolsMenuModel : public menus::SimpleMenuModel {
 public:
  explicit ToolsMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                          Browser* browser);
  virtual ~ToolsMenuModel();

 private:
  void Build(Browser* browser);

  scoped_ptr<EncodingMenuModel> encoding_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ToolsMenuModel);
};

// A menu model that builds the contents of the wrench menu.
class WrenchMenuModel : public menus::SimpleMenuModel {
 public:
  explicit WrenchMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                           Browser* browser);
  virtual ~WrenchMenuModel();

  // Overridden from menus::SimpleMenuModel:
  virtual bool IsLabelDynamicAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool HasIcons() const { return true; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const;

 protected:
  // Adds the cut/copy/paste items to the menu. The default implementation adds
  // three real menu items, while platform specific subclasses add their own
  // native monstrosities.
  virtual void CreateCutCopyPaste();

  // Adds the zoom/fullscreen items to the menu. Like CreateCutCopyPaste().
  virtual void CreateZoomFullscreen();

 private:
  void Build();

  string16 GetSyncMenuLabel() const;
  string16 GetAboutEntryMenuLabel() const;
  bool IsDynamicItem(int index) const;

  // Profile names that are in profiles_menu_contents_. This is used to
  // detect profile change.
  std::vector<std::wstring> known_profiles_;

  // Tools menu.
  scoped_ptr<ToolsMenuModel> tools_menu_model_;

  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(WrenchMenuModel);
};

#endif  // CHROME_BROWSER_WRENCH_MENU_MODEL_H_
