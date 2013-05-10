// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_

#include <list>
#include <map>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel_stack_window.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class TaskbarWindowThumbnailerWin;
namespace ui {
class LinearAnimation;
}
namespace views {
class Widget;
}

// A native window that acts as the owner of all panels in the stack, in order
// to make all panels appear as a single window on the taskbar or launcher.
class PanelStackView : public NativePanelStackWindow,
                       public views::WidgetObserver,
                       public views::WidgetDelegateView,
                       public views::WidgetFocusChangeListener,
                       public ui::AnimationDelegate {
 public:
  explicit PanelStackView(NativePanelStackWindowDelegate* delegate);
  virtual ~PanelStackView();

 protected:
  // Overridden from NativePanelStackWindow:
  virtual void Close() OVERRIDE;
  virtual void AddPanel(Panel* panel) OVERRIDE;
  virtual void RemovePanel(Panel* panel) OVERRIDE;
  virtual void MergeWith(NativePanelStackWindow* another) OVERRIDE;
  virtual bool IsEmpty() const OVERRIDE;
  virtual bool HasPanel(Panel* panel) const OVERRIDE;
  virtual void MovePanelsBy(const gfx::Vector2d& delta) OVERRIDE;
  virtual void BeginBatchUpdatePanelBounds(bool animate) OVERRIDE;
  virtual void AddPanelBoundsForBatchUpdate(
      Panel* panel, const gfx::Rect& new_bounds) OVERRIDE;
  virtual void EndBatchUpdatePanelBounds() OVERRIDE;
  virtual bool IsAnimatingPanelBounds() const OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void DrawSystemAttention(bool draw_attention) OVERRIDE;

 private:
  typedef std::list<Panel*> Panels;

  // The map value is old bounds of the panel.
  typedef std::map<Panel*, gfx::Rect> BoundsUpdates;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // Overridden from views::WidgetFocusChangeListener:
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Updates the bounds of panels as specified in batch update data.
  void UpdatePanelsBounds();

  // Computes/updates the minimum bounds that could fit all panels.
  gfx::Rect GetStackWindowBounds() const;
  void UpdateStackWindowBounds();

  views::Widget* CreateWindowWithBounds(const gfx::Rect& bounds);
  void EnsureWindowCreated();

  // Makes the stack window own the panel window such that multiple panels
  // stacked together could appear as a single window on the taskbar or
  // launcher.
  static void MakeStackWindowOwnPanelWindow(Panel* panel,
                                            PanelStackView* stack_window);

#if defined(OS_WIN)
  // Capture the thumbnail of the whole stack and provide it to live preview
  // (available since Windows 7).
  void CaptureThumbnailForLivePreview();

  // Updates the bounds of the widget window in a deferred way.
  void DeferUpdateNativeWindowBounds(HDWP defer_window_pos_info,
                                     views::Widget* window,
                                     const gfx::Rect& bounds);
#endif

  NativePanelStackWindowDelegate* delegate_;

  views::Widget* window_;  // Weak pointer, own us.

  bool is_closing_;

  // Tracks all panels that are enclosed by this window.
  Panels panels_;

  // Is the taskbar icon of the underlying window being flashed in order to
  // draw the user's attention?
  bool is_drawing_attention_;

#if defined(OS_WIN)
  // Used to provide custom taskbar thumbnail for Windows 7 and later.
  scoped_ptr<TaskbarWindowThumbnailerWin> thumbnailer_;
#endif

  // For batch bounds update.
  bool animate_bounds_updates_;
  bool bounds_updates_started_;
  BoundsUpdates bounds_updates_;

  // Used to animate the bounds changes at a synchronized pace.
  scoped_ptr<ui::LinearAnimation> bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(PanelStackView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_
