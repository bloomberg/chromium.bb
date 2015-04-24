// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_VIEW_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_subclass.h"
#endif

class AutoKeepAlive;
class Panel;
class PanelBoundsAnimation;
class PanelFrameView;
class TaskbarWindowThumbnailerWin;

namespace views {
class WebView;
}

class PanelView : public NativePanel,
                  public views::WidgetObserver,
                  public views::WidgetDelegateView,
#if defined(OS_WIN)
                  public ui::HWNDMessageFilter,
#endif
                  public gfx::AnimationDelegate {
 public:
  // The size of inside area used for mouse resizing.
  static const int kResizeInsideBoundsSize = 5;

  PanelView(Panel* panel, const gfx::Rect& bounds, bool always_on_top);
  ~PanelView() override;

  // Overridden from NativePanel:
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

  // Overridden from views::View:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  // Return true if the mouse event is handled.
  // |mouse_location| is in screen coordinate system.
  bool OnTitlebarMousePressed(const gfx::Point& mouse_location);
  bool OnTitlebarMouseDragged(const gfx::Point& mouse_location);
  bool OnTitlebarMouseReleased(panel::ClickModifier modifier);
  bool OnTitlebarMouseCaptureLost();

  PanelFrameView* GetFrameView() const;
  bool IsAnimatingBounds() const;

  Panel* panel() const { return panel_.get(); }
  views::Widget* window() const { return window_; }
  bool force_to_paint_as_inactive() const {
    return force_to_paint_as_inactive_;
  }

  // PanelStackView might want to update the stored bounds directly since it
  // has already taken care of updating the window bounds directly.
  void set_cached_bounds_directly(const gfx::Rect& bounds) { bounds_ = bounds; }

 private:
  enum MouseDraggingState {
    NO_DRAGGING,
    DRAGGING_STARTED,
    DRAGGING_ENDED
  };

  // Overridden from views::WidgetDelegate:
  void OnDisplayChanged() override;
  void OnWorkAreaChanged() override;
  bool WillProcessWorkAreaChange() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  void WindowClosing() override;
  void DeleteDelegate() override;
  void OnWindowBeginUserBoundsChange() override;
  void OnWindowEndUserBoundsChange() override;

  // Overridden from views::View:
  void Layout() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // Overridden from ui::HWNDMessageFilter:
#if defined(OS_WIN)
  bool FilterMessage(HWND hwnd,
                     UINT message,
                     WPARAM w_param,
                     LPARAM l_param,
                     LRESULT* l_result) override;
#endif

  // Overridden from AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  void UpdateLoadingAnimations(bool should_animate);
  void UpdateWindowTitle();
  void UpdateWindowIcon();
  void SetBoundsInternal(const gfx::Rect& bounds, bool animate);
  bool EndDragging(bool cancelled);

  // Sets the bounds of the underlying window to |new_bounds|. Note that this
  // might update the window style to work around the minimum overlapped
  // window height limitation.
  void SetWidgetBounds(const gfx::Rect& new_bounds);

#if defined(OS_WIN)
  // Sets |attribute_value_to_set| and/or clears |attribute_value_to_reset| for
  // the attibute denoted by |attribute_index|. This is used to update the style
  // or extended style for the native window.
  void UpdateWindowAttribute(int attribute_index,
                             int attribute_value_to_set,
                             int attribute_value_to_reset,
                             bool update_frame);
#endif

  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;

  // The window that holds all panel views. Lifetime managed by native widget.
  // See widget.h.
  views::Widget* window_;

  // Close gets called more than once, so use this to do one-time clean up once.
  bool window_closed_;

  // The view hosting the web contents. Will be destroyed when child views
  // of this class are destroyed.
  views::WebView* web_view_;

  // True if the panel should always stay on top of other windows.
  bool always_on_top_;

  // Is the panel receiving the focus?
  bool focused_;

  // True if the user is resizing the panel.
  bool user_resizing_;

#if defined(OS_WIN)
  // True if the user is resizing the interior edge of a stack.
  bool user_resizing_interior_stacked_panel_edge_;

  // The original full size of the resizing panel before the resizing states.
  gfx::Size original_full_size_of_resizing_panel_;

  // The original full size of the panel below the resizing panel before the
  // resizing starts.
  gfx::Size original_full_size_of_panel_below_resizing_panel_;
#endif


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

  // Should we force to paint the panel as inactive? This is needed when we need
  // to capture the screenshot before an active panel goes minimized.
  bool force_to_paint_as_inactive_;

  // The last view that had focus in the panel. This is saved so that focus can
  // be restored properly when a drag ends.
  views::View* old_focused_view_;

  scoped_ptr<AutoKeepAlive> keep_alive_;

#if defined(OS_WIN)
  // Used to provide custom taskbar thumbnail for Windows 7 and later.
  scoped_ptr<TaskbarWindowThumbnailerWin> thumbnailer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PanelView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_VIEW_H_
