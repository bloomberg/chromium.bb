// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/animation/animation_delegate.h"

class Browser;
class Panel;
class NativePanelTestingWin;
class PanelBrowserFrameView;
namespace ui {
class SlideAnimation;
}

// A browser view that implements Panel specific behavior.
class PanelBrowserView : public BrowserView,
                         public NativePanel,
                         public ui::AnimationDelegate {
 public:
  PanelBrowserView(Browser* browser, Panel* panel, const gfx::Rect& bounds);
  virtual ~PanelBrowserView();

  Panel* panel() const { return panel_.get(); }
  bool closed() const { return closed_; }
  bool focused() const { return focused_; }

  PanelBrowserFrameView* GetFrameView() const;

  // Called from frame view when titlebar receives a mouse event.
  // Return true if the event is handled.
  bool OnTitlebarMousePressed(const gfx::Point& location);
  bool OnTitlebarMouseDragged(const gfx::Point& location);
  bool OnTitlebarMouseReleased();
  bool OnTitlebarMouseCaptureLost();

 private:
  friend class NativePanelTestingWin;
  friend class PanelBrowserViewTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, CreatePanel);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, ShowOrHideSettingsButton);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, SetBoundsAnimation);

  enum MouseDraggingState {
    NO_DRAGGING,
    DRAGGING_STARTED,
    DRAGGING_ENDED
  };

  // Overridden from BrowserView:
  virtual void Init() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator)
      OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual void OnDisplayChanged() OVERRIDE;
  virtual void OnWorkAreaChanged() OVERRIDE;
  virtual bool WillProcessWorkAreaChange() const OVERRIDE;

  // Overridden from views::Widget::Observer
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // Overridden from NativePanel:
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
  virtual void UpdatePanelLoadingAnimations(bool should_animate) OVERRIDE;
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
  virtual gfx::Size GetNonClientAreaExtent() const OVERRIDE;
  virtual int GetRestoredHeight() const OVERRIDE;
  virtual void SetRestoredHeight(int height) OVERRIDE;
  virtual Browser* GetPanelBrowser() const OVERRIDE;
  virtual void DestroyPanelBrowser() OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  bool EndDragging(bool cancelled);

  void StopDrawingAttention();

  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;

  // Stores the full height of the panel so we can restore it after it's
  // been minimized.
  int restored_height_;

  // Is the panel being closed? Do not use it when it is closed.
  bool closed_;

  // Is the panel receiving the focus?
  bool focused_;

  // Is the mouse button currently down?
  bool mouse_pressed_;

  // Location the mouse was pressed at. Used to detect drag and drop.
  gfx::Point mouse_pressed_point_;

  // Timestamp when the mouse was pressed. Used to detect long click.
  base::TimeTicks mouse_pressed_time_;

  // Is the titlebar currently being dragged?  That is, has the cursor
  // moved more than kDragThreshold away from its starting position?
  MouseDraggingState mouse_dragging_state_;

  // Used to animate the bounds change.
  scoped_ptr<ui::SlideAnimation> bounds_animator_;
  gfx::Rect animation_start_bounds_;

  // Is the panel in highlighted state to draw people's attention?
  bool is_drawing_attention_;

  // Timestamp to prevent minimizing the panel when the user clicks the titlebar
  // to clear the attension state.
  base::TimeTicks attention_cleared_time_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
