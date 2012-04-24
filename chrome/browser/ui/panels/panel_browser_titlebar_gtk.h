// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_

#include "chrome/browser/ui/gtk/browser_titlebar.h"

class PanelBrowserTitlebarGtk : public BrowserTitlebar {
 public:
  PanelBrowserTitlebarGtk(BrowserWindowGtk* browser_window, GtkWindow* window);
  virtual ~PanelBrowserTitlebarGtk();

 protected:
  // Overridden from BrowserTitlebar:
  virtual bool BuildButton(const std::string& button_token,
                           bool left_side) OVERRIDE;
  virtual void ShowFaviconMenu(GdkEventButton* event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PanelBrowserTitlebarGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_TITLEBAR_GTK_H_
