// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_

#include "chrome/browser/ui/gtk/browser_titlebar.h"
#include "ui/gfx/skia_util.h"

class PanelBrowserWindowGtk;

class PanelBrowserTitlebarGtk : public BrowserTitlebar {
 public:
  PanelBrowserTitlebarGtk(PanelBrowserWindowGtk* browser_window,
                          GtkWindow* window);
  virtual ~PanelBrowserTitlebarGtk();

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

  // Overridden from BrowserTitlebar:
  virtual void UpdateButtonBackground(CustomDrawButton* button) OVERRIDE;
  virtual void UpdateTitleAndIcon() OVERRIDE;
  virtual void UpdateTextColor() OVERRIDE;

 protected:
  // Overridden from BrowserTitlebar:
  virtual bool BuildButton(const std::string& button_token,
                           bool left_side) OVERRIDE;
  virtual void GetButtonResources(const std::string& button_name,
                                  int* normal_image_id,
                                  int* pressed_image_id,
                                  int* hover_image_id,
                                  int* tooltip_id) const OVERRIDE;
  virtual int GetButtonOuterPadding() const OVERRIDE;
  virtual int GetButtonSpacing() const OVERRIDE;
  virtual void HandleButtonClick(GtkWidget* button) OVERRIDE;
  virtual void ShowFaviconMenu(GdkEventButton* event) OVERRIDE;

 private:
  friend class NativePanelTestingGtk;

  CustomDrawButton* unminimize_button() const {
    return unminimize_button_.get();
  }

  SkColor GetTextColor() const;

  PanelBrowserWindowGtk* browser_window_;

  // All other buttons, including close and minimize buttons, are defined in
  // the base class BrowserTitlebar. This is indeed our restore button. But
  // we name it differently to avoid the confusion with restore_button defined
  // in the base class and used for unmaximize purpose.
  scoped_ptr<CustomDrawButton> unminimize_button_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserTitlebarGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_
