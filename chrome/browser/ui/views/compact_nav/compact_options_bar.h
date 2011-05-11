// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_OPTIONS_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_OPTIONS_BAR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/command_updater.h"
#include "ui/base/models/simple_menu_model.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class BrowserView;
class WrenchMenu;

namespace views {
class MenuButton;
class MenuListener;
}  // namespace views

// This class provides a small options bar that includes browser actions and
// the wrench menu button.
class CompactOptionsBar : public views::View,
                          public views::ViewMenuDelegate,
                          public ui::AcceleratorProvider,
                          public views::ButtonListener {
 public:
  explicit CompactOptionsBar(BrowserView* browser_view);
  virtual ~CompactOptionsBar();

  // Must be called before anything else, but after adding this view to the
  // widget.
  void Init();

  // Add a listener to receive a callback when the menu opens.
  void AddMenuListener(views::MenuListener* listener);

  // Remove a menu listener.
  void RemoveMenuListener(views::MenuListener* listener);

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // Overridden from ui::AcceleratorProvider:
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Load the images for the back and forward buttons.
  void LoadImages();

  BrowserView* browser_view_;

  bool initialized_;

  // Button and models for the wrench/app menu.
  views::MenuButton* app_menu_;
  scoped_ptr<ui::SimpleMenuModel> wrench_menu_model_;
  scoped_refptr<WrenchMenu> wrench_menu_;
  std::vector<views::MenuListener*> menu_listeners_;

  // TODO(stevet): Add the BrowserActionsContainer.

  DISALLOW_COPY_AND_ASSIGN(CompactOptionsBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPACT_NAV_COMPACT_OPTIONS_BAR_H_
