// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_

#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/panels/native_panel.h"

class Panel;

class PanelBrowserWindowGtk : public BrowserWindowGtk,
                              public NativePanel {
 public:
  PanelBrowserWindowGtk(Browser* browser, Panel* panel,
                        const gfx::Rect& bounds);
  virtual ~PanelBrowserWindowGtk();

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

  // Overridden from NativePanel:
  virtual void ShowPanel() OVERRIDE;
  virtual gfx::Rect GetPanelBounds() const OVERRIDE;
  virtual void SetPanelBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void MinimizePanel() OVERRIDE;
  virtual void RestorePanel() OVERRIDE;
  virtual void ClosePanel() OVERRIDE;
  virtual void ActivatePanel() OVERRIDE;
  virtual void DeactivatePanel() OVERRIDE;
  virtual bool IsPanelActive() const OVERRIDE;
  virtual gfx::NativeWindow GetNativePanelHandle() OVERRIDE;
  virtual void UpdatePanelTitleBar() OVERRIDE;
  virtual void ShowTaskManagerForPanel() OVERRIDE;
  virtual void NotifyPanelOnUserChangedTheme() OVERRIDE;
  virtual void FlashPanelFrame() OVERRIDE;
  virtual void DestroyPanelBrowser() OVERRIDE;

 private:
  void SetBoundsImpl();

  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;
  DISALLOW_COPY_AND_ASSIGN(PanelBrowserWindowGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
