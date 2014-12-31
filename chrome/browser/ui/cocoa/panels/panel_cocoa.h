// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PANELS_PANEL_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PANELS_PANEL_COCOA_H_

#import <Foundation/Foundation.h>
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "ui/gfx/geometry/rect.h"

class Panel;
@class PanelWindowControllerCocoa;

// An implememtation of the native panel in Cocoa.
// Bridges between C++ and the Cocoa NSWindow. Cross-platform code will
// interact with this object when it needs to manipulate the window.
class PanelCocoa : public NativePanel {
 public:
  PanelCocoa(Panel* panel, const gfx::Rect& bounds, bool always_on_top);
  ~PanelCocoa() override;

  // Overridden from NativePanel
  void ShowPanel() override;
  void ShowPanelInactive() override;
  gfx::Rect GetPanelBounds() const override;
  void SetPanelBounds(const gfx::Rect& bounds) override;
  void SetPanelBoundsInstantly(const gfx::Rect& bounds) override;
  void ClosePanel() override;
  void ActivatePanel() override;
  void DeactivatePanel() override;
  bool IsPanelActive() const override;
  void PreventActivationByOS(bool prevent_activation) override;
  gfx::NativeWindow GetNativePanelWindow() override;
  void UpdatePanelTitleBar() override;
  void UpdatePanelLoadingAnimations(bool should_animate) override;
  void PanelWebContentsFocused(content::WebContents* contents) override;
  void PanelCut() override;
  void PanelCopy() override;
  void PanelPaste() override;
  void DrawAttention(bool draw_attention) override;
  bool IsDrawingAttention() const override;
  void HandlePanelKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void FullScreenModeChanged(bool is_full_screen) override;
  bool IsPanelAlwaysOnTop() const override;
  void SetPanelAlwaysOnTop(bool on_top) override;
  void UpdatePanelMinimizeRestoreButtonVisibility() override;
  void SetWindowCornerStyle(panel::CornerStyle corner_style) override;
  void PanelExpansionStateChanging(Panel::ExpansionState old_state,
                                   Panel::ExpansionState new_state) override;
  void AttachWebContents(content::WebContents* contents) override;
  void DetachWebContents(content::WebContents* contents) override;

  // These sizes are in screen coordinates.
  gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const override;
  gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const override;
  int TitleOnlyHeight() const override;

  void MinimizePanelBySystem() override;
  bool IsPanelMinimizedBySystem() const override;
  bool IsPanelShownOnActiveDesktop() const override;
  void ShowShadow(bool show) override;
  NativePanelTesting* CreateNativePanelTesting() override;

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
