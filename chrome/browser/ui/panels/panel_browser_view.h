// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"

class Browser;
class NativePanelTestingWin;
class Panel;
class PanelBoundsAnimation;
class PanelBrowserFrameView;

// A browser view that implements Panel specific behavior.
class PanelBrowserView : public BrowserView,
                         public content::NotificationObserver,
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
  // |mouse_location| is in screen coordinates.
  bool OnTitlebarMousePressed(const gfx::Point& mouse_location);
  bool OnTitlebarMouseDragged(const gfx::Point& mouse_location);
  bool OnTitlebarMouseReleased(panel::ClickModifier modifier);
  bool OnTitlebarMouseCaptureLost();

 private:
  friend class NativePanelTestingWin;
  friend class PanelBrowserViewTest;

  enum MouseDraggingState {
    NO_DRAGGING,
    DRAGGING_STARTED,
    DRAGGING_ENDED
  };

  // Overridden from BrowserView:
  virtual void Init() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual bool IsPanel() const OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Overridden from NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual void OnDisplayChanged() OVERRIDE;
  virtual void OnWorkAreaChanged() OVERRIDE;
  virtual bool WillProcessWorkAreaChange() const OVERRIDE;
  virtual void OnWindowBeginUserBoundsChange() OVERRIDE;
  virtual void OnWindowEndUserBoundsChange() OVERRIDE;

  // Overridden from views::Widget::Observer
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // Overridden from NativePanel:
  virtual void ShowPanel() OVERRIDE;
  virtual void ShowPanelInactive() OVERRIDE;
  virtual gfx::Rect GetPanelBounds() const OVERRIDE;
  virtual void SetPanelBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetPanelBoundsInstantly(const gfx::Rect& bounds) OVERRIDE;
  virtual void ClosePanel() OVERRIDE;
  virtual void ActivatePanel() OVERRIDE;
  virtual void DeactivatePanel() OVERRIDE;
  virtual bool IsPanelActive() const OVERRIDE;
  virtual void PreventActivationByOS(bool prevent_activation) OVERRIDE;
  virtual gfx::NativeWindow GetNativePanelHandle() OVERRIDE;
  virtual void UpdatePanelTitleBar() OVERRIDE;
  virtual void UpdatePanelLoadingAnimations(bool should_animate) OVERRIDE;
  virtual void ShowTaskManagerForPanel() OVERRIDE;
  virtual FindBar* CreatePanelFindBar() OVERRIDE;
  virtual void NotifyPanelOnUserChangedTheme() OVERRIDE;
  virtual void PanelWebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual void PanelCut() OVERRIDE;
  virtual void PanelCopy() OVERRIDE;
  virtual void PanelPaste() OVERRIDE;
  virtual void DrawAttention(bool draw_attention) OVERRIDE;
  virtual bool IsDrawingAttention() const OVERRIDE;
  virtual bool PreHandlePanelKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void FullScreenModeChanged(bool is_full_screen) OVERRIDE;
  virtual void HandlePanelKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const OVERRIDE;
  virtual gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const OVERRIDE;
  virtual int TitleOnlyHeight() const OVERRIDE;
  virtual Browser* GetPanelBrowser() const OVERRIDE;
  virtual void DestroyPanelBrowser() OVERRIDE;
  virtual void EnsurePanelFullyVisible() OVERRIDE;
  virtual void SetPanelAlwaysOnTop(bool on_top) OVERRIDE;
  virtual void EnableResizeByMouse(bool enable) OVERRIDE;
  virtual void UpdatePanelMinimizeRestoreButtonVisibility() OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  bool EndDragging(bool cancelled);

  void SetBoundsInternal(const gfx::Rect& bounds, bool animate);

  void ShowOrHidePanelAppIcon(bool show);

  bool IsAnimatingBounds() const;

#if defined(OS_WIN) && !defined(USE_AURA)
  // Sets or clears the bitwise |attribute_value| for the attibute denoted by
  // |attribute_index|. This is used to update the style or extended style
  // for the native window.
  void UpdateWindowAttribute(int attribute_index,
                             int attribute_value,
                             bool to_set);
#endif

  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;

  // Is the panel being closed? Do not use it when it is closed.
  bool closed_;

  // Is the panel receiving the focus?
  bool focused_;

  // Is the mouse button currently down?
  bool mouse_pressed_;

  // Location the mouse was pressed at or dragged to last time when we process
  // the mouse event. Used in drag-and-drop.
  // This point is represented in the screen coordinate system.
  gfx::Point last_mouse_location_;

  // Is the titlebar currently being dragged?  That is, has the cursor
  // moved more than kDragThreshold away from its starting position?
  MouseDraggingState mouse_dragging_state_;

  // Used to animate the bounds change.
  scoped_ptr<PanelBoundsAnimation> bounds_animator_;
  gfx::Rect animation_start_bounds_;

  // Is the panel in highlighted state to draw people's attention?
  bool is_drawing_attention_;

  // The last view that had focus in the panel. This is saved so that focus can
  // be restored properly when a drag ends.
  views::View* old_focused_view_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
