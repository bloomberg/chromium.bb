// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_

#include "chrome/browser/ui/gtk/browser_titlebar.h"

class PanelBrowserWindowGtk;

class PanelBrowserTitlebarGtk : public BrowserTitlebar {
 public:
  PanelBrowserTitlebarGtk(PanelBrowserWindowGtk* browser_window,
                          GtkWindow* window);
  virtual ~PanelBrowserTitlebarGtk();

  void UpdateMinimizeRestoreButtonVisibility();

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

  PanelBrowserWindowGtk* browser_window_;

  // All other buttons, including close and minimize buttons, are defined in
  // the base class BrowserTitlebar. This is indeed our restore button. But
  // we name it differently to avoid the confusion with restore_button defined
  // in the base class and used for unmaximize purpose.
  scoped_ptr<CustomDrawButton> unminimize_button_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserTitlebarGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_
