// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PANELS_PANEL_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PANELS_PANEL_COCOA_H_

#import <Foundation/Foundation.h>
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "ui/gfx/rect.h"

class Panel;
@class PanelWindowControllerCocoa;

// An implememtation of the native panel in Cocoa.
// Bridges between C++ and the Cocoa NSWindow. Cross-platform code will
// interact with this object when it needs to manipulate the window.
class PanelCocoa : public NativePanel {
 public:
  PanelCocoa(Panel* panel, const gfx::Rect& bounds, bool always_on_top);
  virtual ~PanelCocoa();

  // Overridden from NativePanel
  virtual void ShowPanel() override;
  virtual void ShowPanelInactive() override;
  virtual gfx::Rect GetPanelBounds() const override;
  virtual void SetPanelBounds(const gfx::Rect& bounds) override;
  virtual void SetPanelBoundsInstantly(const gfx::Rect& bounds) override;
  virtual void ClosePanel() override;
  virtual void ActivatePanel() override;
  virtual void DeactivatePanel() override;
  virtual bool IsPanelActive() const override;
  virtual void PreventActivationByOS(bool prevent_activation) override;
  virtual gfx::NativeWindow GetNativePanelWindow() override;
  virtual void UpdatePanelTitleBar() override;
  virtual void UpdatePanelLoadingAnimations(bool should_animate) override;
  virtual void PanelWebContentsFocused(content::WebContents* contents) override;
  virtual void PanelCut() override;
  virtual void PanelCopy() override;
  virtual void PanelPaste() override;
  virtual void DrawAttention(bool draw_attention) override;
  virtual bool IsDrawingAttention() const override;
  virtual void HandlePanelKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  virtual void FullScreenModeChanged(bool is_full_screen) override;
  virtual bool IsPanelAlwaysOnTop() const override;
  virtual void SetPanelAlwaysOnTop(bool on_top) override;
  virtual void UpdatePanelMinimizeRestoreButtonVisibility() override;
  virtual void SetWindowCornerStyle(panel::CornerStyle corner_style) override;
  virtual void PanelExpansionStateChanging(
      Panel::ExpansionState old_state,
      Panel::ExpansionState new_state) override;
  virtual void AttachWebContents(content::WebContents* contents) override;
  virtual void DetachWebContents(content::WebContents* contents) override;

  // These sizes are in screen coordinates.
  virtual gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const override;
  virtual gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const override;
  virtual int TitleOnlyHeight() const override;

  virtual void MinimizePanelBySystem() override;
  virtual bool IsPanelMinimizedBySystem() const override;
  virtual bool IsPanelShownOnActiveDesktop() const override;
  virtual void ShowShadow(bool show) override;
  virtual NativePanelTesting* CreateNativePanelTesting() override;

  Panel* panel() const;
  void DidCloseNativeWindow();

  bool IsClosed() const;

  // PanelStackWindowCocoa might want to update the stored bounds directly since
  // it has already taken care of updating the window bounds directly.
  void set_cached_bounds_directly(const gfx::Rect& bounds) { bounds_ = bounds; }

 private:
  friend class CocoaNativePanelTesting;
  friend class PanelCocoaTest;
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, CreateClose);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, NativeBounds);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, TitlebarViewCreate);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, TitlebarViewSizing);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, TitlebarViewClose);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, MenuItems);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, KeyEvent);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, ThemeProvider);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, SetTitle);
  FRIEND_TEST_ALL_PREFIXES(PanelCocoaTest, ActivatePanel);

  void setBoundsInternal(const gfx::Rect& bounds, bool animate);

  scoped_ptr<Panel> panel_;
  PanelWindowControllerCocoa* controller_;  // Weak, owns us.

  // These use platform-independent screen coordinates, with (0,0) at
  // top-left of the primary screen. They have to be converted to Cocoa
  // screen coordinates before calling Cocoa API.
  gfx::Rect bounds_;

  // True if the panel should always stay on top of other windows.
  bool always_on_top_;

  bool is_shown_;  // Panel is hidden on creation, Show() changes that forever.
  NSInteger attention_request_id_;  // identifier from requestUserAttention.

  // Indicates how the window corner should be rendered, rounded or not.
  panel::CornerStyle corner_style_;

  DISALLOW_COPY_AND_ASSIGN(PanelCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_PANELS_PANEL_COCOA_H_
