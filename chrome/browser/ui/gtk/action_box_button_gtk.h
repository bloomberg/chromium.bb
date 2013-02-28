// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ACTION_BOX_BUTTON_GTK_H_
#define CHROME_BROWSER_UI_GTK_ACTION_BOX_BUTTON_GTK_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/toolbar/action_box_button_controller.h"
#include "ui/base/gtk/gtk_signal.h"

class ActionBoxMenuModel;
class Browser;
class CustomDrawButton;

typedef struct _GtkWidget GtkWidget;

// This class displays the action box button with an associated menu. This is
// where extension actions and the bookmark star live.
class ActionBoxButtonGtk : public MenuGtk::Delegate,
                           public ActionBoxButtonController::Delegate {
 public:
  explicit ActionBoxButtonGtk(Browser* browser);
  virtual ~ActionBoxButtonGtk();

  // MenuGtk::Delegate implementation.
  virtual void StoppedShowing() OVERRIDE;
  virtual bool AlwaysShowIconForCmd(int command_id) const OVERRIDE;
  virtual GtkWidget* GetImageForCommandId(int command_id) const OVERRIDE;

  GtkWidget* widget();

  ActionBoxButtonController* action_box_button_controller() {
    return &controller_;
  }

 private:
  // ActionBoxButtonController::Delegate implementation.
  virtual void ShowMenu(scoped_ptr<ActionBoxMenuModel> model) OVERRIDE;

  // Show the action box menu.
  CHROMEGTK_CALLBACK_1(ActionBoxButtonGtk, gboolean, OnButtonPress,
                       GdkEventButton*);

  ActionBoxButtonController controller_;

  scoped_ptr<CustomDrawButton> button_;

  // The browser to which we will send commands.
  Browser* browser_;

  // The model and menu displayed when the button is clicked. The menu is
  // recreated every time it is displayed.
  scoped_ptr<ActionBoxMenuModel> model_;
  scoped_ptr<MenuGtk> menu_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxButtonGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ACTION_BOX_BUTTON_GTK_H_
