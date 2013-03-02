// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PANELS_PANEL_TITLEBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_PANELS_PANEL_TITLEBAR_GTK_H_

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/titlebar_throb_animation.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/skia_util.h"

class CustomDrawButton;
class PanelGtk;

namespace content {
class WebContents;
}

class PanelTitlebarGtk {
 public:
  explicit PanelTitlebarGtk(PanelGtk* panel_gtk);
  virtual ~PanelTitlebarGtk();

  void UpdateTextColor();
  void UpdateMinimizeRestoreButtonVisibility();

  // When a panel appears in the same position as the one of the panel being
  // closed and the cursor stays in the close button, the close button appears
  // not to be clickable. This is because neither "enter-notify-event" nor
  // "clicked" event for the new panel gets fired if the mouse does not move.
  // This creates a bad experience when a user has multiple panels of the same
  // size (which is typical) and tries closing them all by repeatedly clicking
  // in the same place on the screen.
  //
  // Opened a gtk bug for this -
  //   https://bugzilla.gnome.org/show_bug.cgi?id=667841
  void SendEnterNotifyToCloseButtonIfUnderMouse();

  void Init();
  void UpdateTitleAndIcon();
  void UpdateThrobber(content::WebContents* web_contents);
  GtkWidget* widget() const;

 private:
  friend class GtkNativePanelTesting;

  void BuildButtons();
  CustomDrawButton* CreateButton(panel::TitlebarButtonType button_type);
  void GetButtonResources(panel::TitlebarButtonType button_type,
                          int* normal_image_id,
                          int* pressed_image_id,
                          int* hover_image_id,
                          int* tooltip_id) const;
  GtkWidget* GetButtonHBox();

  // Callback for minimize/restore/close buttons.
  CHROMEGTK_CALLBACK_0(PanelTitlebarGtk, void, OnButtonClicked);

  CustomDrawButton* close_button() const { return close_button_.get(); }
  CustomDrawButton* minimize_button() const { return minimize_button_.get(); }
  CustomDrawButton* restore_button() const { return restore_button_.get(); }

  SkColor GetTextColor() const;

  // Pointers to the native panel window that owns us and its GtkWindow.
  PanelGtk* panel_gtk_;

  // The container widget the holds the hbox which contains the whole titlebar.
  GtkWidget* container_;

  // VBoxes that holds the minimize/restore/close buttons box.
  GtkWidget* titlebar_right_buttons_vbox_;

  // HBoxes that contains the actual min/max/close buttons.
  GtkWidget* titlebar_right_buttons_hbox_;

  // The icon and page title.
  GtkWidget* icon_;
  GtkWidget* title_;

  // The buttons.
  scoped_ptr<CustomDrawButton> close_button_;
  scoped_ptr<CustomDrawButton> minimize_button_;
  scoped_ptr<CustomDrawButton> restore_button_;

  TitlebarThrobAnimation throbber_;

  DISALLOW_COPY_AND_ASSIGN(PanelTitlebarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_PANELS_PANEL_TITLEBAR_GTK_H_
