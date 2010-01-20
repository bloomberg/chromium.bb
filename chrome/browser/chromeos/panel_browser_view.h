// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PANEL_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_PANEL_BROWSER_VIEW_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/views/frame/browser_view.h"

class Browser;

namespace chromeos {

class PanelController;

// A browser view that implements Panel specific behavior.
class PanelBrowserView : public BrowserView {
 public:
  explicit PanelBrowserView(Browser* browser);

  // BrowserViews overrides.
  virtual void Init();
  virtual void Show();
  virtual void Close();
  virtual void UpdateTitleBar();
  virtual void ActivationChanged(bool activated);

 private:
  // Controls interactions with the window manager for popup panels.
  scoped_ptr<chromeos::PanelController> panel_controller_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PANEL_BROWSER_VIEW_H_
