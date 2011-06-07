// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_

#include "chrome/browser/ui/gtk/browser_window_gtk.h"

class Panel;

class PanelBrowserWindowGtk : public BrowserWindowGtk {
 public:
  PanelBrowserWindowGtk(Browser* browser, Panel* panel);
  virtual ~PanelBrowserWindowGtk() {}

  // BrowserWindowGtk overrides
  virtual void Init() OVERRIDE;

  // BrowserWindow overrides
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;

 protected:
  // BrowserWindowGtk overrides
  virtual bool GetWindowEdge(int x, int y, GdkWindowEdge* edge) OVERRIDE;
  virtual bool HandleTitleBarLeftMousePress(
      GdkEventButton* event,
      guint32 last_click_time,
      gfx::Point last_click_position) OVERRIDE;
  virtual void SaveWindowPosition() OVERRIDE;
  virtual void SetGeometryHints() OVERRIDE;
  virtual bool UseCustomFrame() OVERRIDE;

 private:
  void SetBoundsImpl();

  scoped_ptr<Panel> panel_;
  DISALLOW_COPY_AND_ASSIGN(PanelBrowserWindowGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
