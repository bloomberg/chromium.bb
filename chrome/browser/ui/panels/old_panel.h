// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_OLD_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_OLD_PANEL_H_
#pragma once

#include "chrome/browser/ui/panels/panel.h"

class Browser;
class PanelBrowserWindow;

// Temporary class during the refactor to override behavior in Panel
// to provide legacy behavior. Will be deleted after refactor is complete.
class OldPanel : public Panel {
 public:
  OldPanel(Browser* browser,
           const gfx::Size& min_size, const gfx::Size& max_size);
  virtual ~OldPanel();

  // Overridden from Panel.
  virtual Browser* browser() const OVERRIDE;
  virtual BrowserWindow* browser_window() const OVERRIDE;
  virtual CommandUpdater* command_updater() OVERRIDE;
  virtual Profile* profile() const OVERRIDE;
  virtual void Initialize(const gfx::Rect& bounds, Browser* browser) OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual bool ShouldCloseWindow() OVERRIDE;
  virtual void OnWindowClosing() OVERRIDE;
  virtual void ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition) OVERRIDE;
  virtual SkBitmap GetCurrentPageIcon() const OVERRIDE;

 private:
  Browser* browser_;  // Weak pointer. Owned by native panel.

  // A BrowserWindow for the browser to interact with.
  scoped_ptr<PanelBrowserWindow> panel_browser_window_;
};

#endif  // CHROME_BROWSER_UI_PANELS_OLD_PANEL_H_
