// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_COCOA_H_

#import <Foundation/Foundation.h>
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "ui/gfx/rect.h"

class Browser;
class Panel;
@class PanelWindowControllerCocoa;

// An implementation of BrowserWindow for native Panel in Cocoa.
// Bridges between C++ and the Cocoa NSWindow. Cross-platform code will
// interact with this object when it needs to manipulate the window.

class PanelBrowserWindowCocoa : public NativePanel {
 public:
  PanelBrowserWindowCocoa(Browser* browser, Panel* panel,
                          const gfx::Rect& bounds);
  virtual ~PanelBrowserWindowCocoa();

  // Overridden from NativePanel
  virtual void ShowPanel() OVERRIDE;
  virtual void ShowPanelInactive() OVERRIDE;
  virtual gfx::Rect GetPanelBounds() const OVERRIDE;
  virtual void SetPanelBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void OnPanelExpansionStateChanged(
      Panel::ExpansionState expansion_state) OVERRIDE;
  virtual bool ShouldBringUpPanelTitlebar(int mouse_x,
                                          int mouse_y) const OVERRIDE;
  virtual void ClosePanel() OVERRIDE;
  virtual void ActivatePanel() OVERRIDE;
  virtual void DeactivatePanel() OVERRIDE;
  virtual bool IsPanelActive() const OVERRIDE;
  virtual gfx::NativeWindow GetNativePanelHandle() OVERRIDE;
  virtual void UpdatePanelTitleBar() OVERRIDE;
  virtual void ShowTaskManagerForPanel() OVERRIDE;
  virtual FindBar* CreatePanelFindBar() OVERRIDE;
  virtual void NotifyPanelOnUserChangedTheme() OVERRIDE;
  virtual void DrawAttention() OVERRIDE;
  virtual bool IsDrawingAttention() const OVERRIDE;
  virtual bool PreHandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual Browser* GetPanelBrowser() const OVERRIDE;
  virtual void DestroyPanelBrowser() OVERRIDE;

  Panel* panel() { return panel_.get(); }
  Browser* browser() const { return browser_.get(); }

 private:
  friend class PanelBrowserWindowCocoaTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserWindowCocoaTest, CreateClose);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserWindowCocoaTest, NativeBounds);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserWindowCocoaTest, TitlebarViewCreate);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserWindowCocoaTest, TitlebarViewSizing);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserWindowCocoaTest, TitlebarViewClose);

  bool isClosed();

  scoped_ptr<Browser> browser_;
  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;
  PanelWindowControllerCocoa* controller_;  // Weak, owns us.
  bool is_shown_;  // Panel is hidden on creation, Show() changes that forever.
  bool has_find_bar_; // Find bar should only be created once per panel.

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserWindowCocoa);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_COCOA_H_
