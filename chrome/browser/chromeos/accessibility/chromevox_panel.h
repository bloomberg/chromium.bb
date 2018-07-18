// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/accessibility/accessibility_panel.h"

// Displays spoken feedback UI controls for the ChromeVox component extension
class ChromeVoxPanel : public AccessibilityPanel {
 public:
  explicit ChromeVoxPanel(content::BrowserContext* browser_context);
  ~ChromeVoxPanel() override;

  class ChromeVoxPanelWebContentsObserver;

 private:
  // Methods indirectly invoked by the component extension.
  void EnterFullscreen();
  void ExitFullscreen();
  void Focus();

  // Sends a request to the ash window manager.
  void SetAccessibilityPanelFullscreen(bool fullscreen);

  std::string GetUrlForContent();

  std::unique_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
