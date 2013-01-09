// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_BUILDER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

class Browser;
class EncodingMenuModel;
class ZoomMenuModel;

namespace ui {
class AcceleratorProvider;
class MenuModel;
class SimpleMenuModel;
}

// SystemMenuModelBuilder is responsible for building and owning the system menu
// model.
class SystemMenuModelBuilder {
 public:
  SystemMenuModelBuilder(ui::AcceleratorProvider* provider, Browser* browser);
  ~SystemMenuModelBuilder();

  // Populates the menu.
  void Init();

  // Returns the menu model. SystemMenuModelBuilder owns the returned model.
  ui::MenuModel* menu_model() { return menu_model_.get(); }

 private:
  Browser* browser() { return menu_delegate_.browser(); }

  // Creates and returns the MenuModel. This does not populate the menu.
  // |needs_trailing_separator| is set to true if the menu needs a separator
  // after all items have been added.
  ui::SimpleMenuModel* CreateMenuModel(bool* needs_trailing_separator);

  // Populates |model| with the appropriate contents.
  void BuildMenu(ui::SimpleMenuModel* model);
  void BuildSystemMenuForBrowserWindow(ui::SimpleMenuModel* model);
  void BuildSystemMenuForAppOrPopupWindow(ui::SimpleMenuModel* model);

  // Adds items for toggling the frame type (if necessary).
  void AddFrameToggleItems(ui::SimpleMenuModel* model);

  SystemMenuModelDelegate menu_delegate_;
  scoped_ptr<ui::MenuModel> menu_model_;
  scoped_ptr<ZoomMenuModel> zoom_menu_contents_;
  scoped_ptr<EncodingMenuModel> encoding_menu_contents_;

  DISALLOW_COPY_AND_ASSIGN(SystemMenuModelBuilder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_BUILDER_H_
