// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
#pragma once

#include "chrome/browser/ui/panels/panel.h"
#include "ui/gfx/native_widget_types.h"

class FindBar;
class NativePanelTesting;
class TabContents;

namespace gfx {
class Rect;
}  // namespace gfx

// An interface for a class that implements platform-specific behavior for panel
// windows. We use this interface for two reasons:
// 1. We don't want to use BrowserWindow as the interface between shared panel
//    code and platform-specific code. BrowserWindow has a lot of methods, most
//    of which we don't use and simply stub out with NOTIMPLEMENTED().
// 2. We need some additional methods that BrowserWindow doesn't provide, such
//    as MinimizePanel() and RestorePanel().
// Note that even though we don't use BrowserWindow directly, Windows and GTK+
// still use the BrowserWindow interface as part of their implementation so we
// use Panel in all the method names to avoid collisions.
class NativePanel {
  friend class Panel;
  friend class PanelBrowserTest;

 protected:
  virtual ~NativePanel() {}

  virtual void ShowPanel() = 0;
  virtual void ShowPanelInactive() = 0;
  virtual gfx::Rect GetPanelBounds() const = 0;
  virtual void SetPanelBounds(const gfx::Rect& bounds) = 0;
  virtual void SetPanelBoundsInstantly(const gfx::Rect& bounds) = 0;
  virtual void ClosePanel() = 0;
  virtual void ActivatePanel() = 0;
  virtual void DeactivatePanel() = 0;
  virtual bool IsPanelActive() const = 0;
  virtual gfx::NativeWindow GetNativePanelHandle() = 0;
  virtual void UpdatePanelTitleBar() = 0;
  virtual void UpdatePanelLoadingAnimations(bool should_animate) = 0;
  virtual void ShowTaskManagerForPanel() = 0;
  virtual FindBar* CreatePanelFindBar() = 0;
  virtual void NotifyPanelOnUserChangedTheme() = 0;
  virtual void PanelTabContentsFocused(TabContents* tab_contents) = 0;
  virtual void PanelCut() = 0;
  virtual void PanelCopy() = 0;
  virtual void PanelPaste() = 0;
  virtual void DrawAttention() = 0;
  virtual bool IsDrawingAttention() const = 0;
  virtual bool PreHandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) = 0;
  virtual void HandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event) = 0;
  virtual void FullScreenModeChanged(bool is_full_screen) = 0;

  virtual Browser* GetPanelBrowser() const = 0;
  virtual void DestroyPanelBrowser() = 0;

  // Returns the exterior size of the panel window given the client content
  // size and vice versa.
  virtual gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const = 0;
  virtual gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const = 0;

  virtual int TitleOnlyHeight() const = 0;

  // Returns the size of the iconified panel. This is the size we use to draw
  // the panel put in the overflow area.
  virtual gfx::Size IconOnlySize() const = 0;

  // Brings the panel to the top of the z-order without activating it. This
  // will make sure that the panel is not obscured by other top-most windows.
  virtual void EnsurePanelFullyVisible() = 0;
};

// A NativePanel utility interface used for accessing elements of the
// native panel used only by test automation.
class NativePanelTesting {
 public:
  static NativePanelTesting* Create(NativePanel* native_panel);
  virtual ~NativePanelTesting() {}

  virtual void PressLeftMouseButtonTitlebar(const gfx::Point& point) = 0;
  virtual void ReleaseMouseButtonTitlebar() = 0;
  virtual void DragTitlebar(int delta_x, int delta_y) = 0;
  virtual void CancelDragTitlebar() = 0;
  virtual void FinishDragTitlebar() = 0;

  // Verifies, on a deepest possible level, if the Panel is showing the "Draw
  // Attention" effects to the user. May include checking colors etc.
  virtual bool VerifyDrawingAttention() const = 0;
  // Verifies, on a deepest possible level, if the native panel is really
  // active, i.e. the titlebar is painted per its active state.
  virtual bool VerifyActiveState(bool is_active) = 0;
  virtual void WaitForWindowCreationToComplete() const { }

  virtual bool IsWindowSizeKnown() const = 0;
  virtual bool IsAnimatingBounds() const = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
