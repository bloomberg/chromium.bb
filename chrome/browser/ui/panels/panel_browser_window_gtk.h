// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_

#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/x/work_area_watcher_x_observer.h"

class Panel;
class PanelSettingsMenuModel;
class NativePanelTestingGtk;

namespace ui {

class SlideAnimation;

}

class PanelBrowserWindowGtk : public BrowserWindowGtk,
                              public MenuGtk::Delegate,
                              public MessageLoopForUI::Observer,
                              public NativePanel,
                              public ui::AnimationDelegate,
                              public ui::WorkAreaWatcherXObserver {
  friend class NativePanelTestingGtk;
 public:
  PanelBrowserWindowGtk(Browser* browser, Panel* panel,
                        const gfx::Rect& bounds);
  virtual ~PanelBrowserWindowGtk();

  // BrowserWindowGtk override
  virtual void Init() OVERRIDE;

  // BrowserWindow overrides
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void ShowSettingsMenu(GtkWidget* widget,
                                GdkEventButton* event) OVERRIDE;
  virtual TitleDecoration GetWindowTitle(std::string* title) const OVERRIDE;

  // ui::WorkAreaWatcherXObserver override
  virtual void WorkAreaChanged() OVERRIDE;

 protected:
  // BrowserWindowGtk overrides
  virtual bool GetWindowEdge(int x, int y, GdkWindowEdge* edge) OVERRIDE;
  virtual bool HandleTitleBarLeftMousePress(
      GdkEventButton* event,
      guint32 last_click_time,
      gfx::Point last_click_position) OVERRIDE;
  virtual void SaveWindowPosition() OVERRIDE;
  virtual void SetGeometryHints() OVERRIDE;
  virtual bool UseCustomFrame() OVERRIDE;
  virtual void OnSizeChanged(int width, int height) OVERRIDE;
  virtual void DrawCustomFrame(cairo_t* cr, GtkWidget* widget,
                               GdkEventExpose* event) OVERRIDE;
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;
  // 'focus-in-event' handler.
  virtual void HandleFocusIn(GtkWidget* widget,
                             GdkEventFocus* event) OVERRIDE;

  // Overridden from NativePanel:
  virtual void ShowPanel() OVERRIDE;
  virtual void ShowPanelInactive() OVERRIDE;
  virtual gfx::Rect GetPanelBounds() const OVERRIDE;
  virtual void SetPanelBounds(const gfx::Rect& bounds) OVERRIDE;
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
  virtual void PanelTabContentsFocused(TabContents* tab_contents) OVERRIDE;
  virtual void PanelCut() OVERRIDE;
  virtual void PanelCopy() OVERRIDE;
  virtual void PanelPaste() OVERRIDE;
  virtual void DrawAttention() OVERRIDE;
  virtual bool IsDrawingAttention() const OVERRIDE;
  virtual bool PreHandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual Browser* GetPanelBrowser() const OVERRIDE;
  virtual void DestroyPanelBrowser() OVERRIDE;
  virtual gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const OVERRIDE;
  virtual gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const OVERRIDE;
  virtual int TitleOnlyHeight() const OVERRIDE;
  virtual gfx::Size IconOnlySize() const OVERRIDE;
  virtual void EnsurePanelFullyVisible() OVERRIDE;

 private:
  void StartBoundsAnimation(const gfx::Rect& current_bounds);
  bool IsAnimatingBounds() const;

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE;
  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  void CreateDragWidget();
  void DestroyDragWidget();
  void EndDrag(bool canceled);
  void CleanupDragDrop();

  GdkRectangle GetTitlebarRectForDrawAttention() const;

  CHROMEGTK_CALLBACK_1(PanelBrowserWindowGtk, gboolean,
                       OnTitlebarButtonPressEvent, GdkEventButton*);
  CHROMEGTK_CALLBACK_1(PanelBrowserWindowGtk, gboolean,
                       OnTitlebarButtonReleaseEvent, GdkEventButton*);

  // drag-begin is emitted when the drag is started. We connect so that we can
  // set the drag icon to a transparent pixbuf.
  CHROMEGTK_CALLBACK_1(PanelBrowserWindowGtk, void, OnDragBegin,
                       GdkDragContext*);

  // drag-failed is emitted when the drag is finished.  In our case the signal
  // does not imply failure as we don't use the drag-n-drop API to transfer drop
  // data.
  CHROMEGTK_CALLBACK_2(PanelBrowserWindowGtk, gboolean, OnDragFailed,
                       GdkDragContext*, GtkDragResult);

  // When a drag is ending, a fake button release event is passed to the drag
  // widget to fake letting go of the mouse button.  We need a callback for
  // this event because it is the only way to catch drag end events when the
  // user presses space or return.
  CHROMEGTK_CALLBACK_1(PanelBrowserWindowGtk, gboolean, OnDragButtonReleased,
                       GdkEventButton*);

  // A copy of the last button press event, used to initiate a drag.
  GdkEvent* last_mouse_down_;

  // A GtkInivisible used to track the drag event.  GtkInvisibles are of the
  // type GInitiallyUnowned, but the widget initialization code sinks the
  // reference, so we can't use an OwnedWidgetGtk here.
  GtkWidget* drag_widget_;

  // Used to destroy the drag widget after a return to the message loop.
  ScopedRunnableMethodFactory<PanelBrowserWindowGtk>
      destroy_drag_widget_factory_;

  // Due to a bug in GTK+, we need to force the end of a drag when we get a
  // mouse release event on the the dragged widget, otherwise, we don't know
  // when the drag has ended when the user presses space or enter.  We queue
  // a task to end the drag and only run it if GTK+ didn't send us the
  // drag-failed event.
  base::WeakPtrFactory<PanelBrowserWindowGtk> drag_end_factory_;

  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;

  scoped_ptr<PanelSettingsMenuModel> settings_menu_model_;
  scoped_ptr<MenuGtk> settings_menu_;

  // False until the window has been allocated and sized.
  bool window_size_known_;

  // Indicates that the panel is currently drawing attention.
  bool is_drawing_attention_;

  // Disable ExpansionState changes on mouse click for a short duration.
  // This is needed in case the window gains focus as result of mouseDown while
  // being already expanded and drawing attention - in this case, we don't
  // want to minimize it on subsequent mouseUp.
  // We use time interval because the window may gain focus in various ways
  // (via keyboard for example) which are not distinguishable at this point.
  // Apparently this disable interval is not affecting the user in other cases.
  base::Time disableMinimizeUntilTime_;

  // Used to animate the bounds change.
  scoped_ptr<ui::SlideAnimation> bounds_animator_;
  gfx::Rect animation_start_bounds_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserWindowGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
