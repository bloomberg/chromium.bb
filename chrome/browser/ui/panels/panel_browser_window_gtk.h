// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_

#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "ui/base/animation/animation_delegate.h"

class Panel;
class PanelBoundsAnimation;
class PanelDragGtk;
class NativePanelTestingGtk;

class PanelBrowserWindowGtk : public BrowserWindowGtk,
                              public NativePanel,
                              public ui::AnimationDelegate {
 public:
  PanelBrowserWindowGtk(Browser* browser, Panel* panel,
                        const gfx::Rect& bounds);
  virtual ~PanelBrowserWindowGtk();

  // BrowserWindowGtk override
  virtual void Init() OVERRIDE;

  // BrowserWindow overrides
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual TitleDecoration GetWindowTitle(std::string* title) const OVERRIDE;

  // Overrides BrowserWindowGtk::NotificationObserver::Observe
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  // BrowserWindowGtk overrides
  virtual bool GetWindowEdge(int x, int y, GdkWindowEdge* edge) OVERRIDE;
  virtual bool HandleTitleBarLeftMousePress(
      GdkEventButton* event,
      guint32 last_click_time,
      gfx::Point last_click_position) OVERRIDE;
  virtual bool HandleWindowEdgeLeftMousePress(
      GtkWindow* window,
      GdkWindowEdge edge,
      GdkEventButton* event) OVERRIDE;
  virtual void SaveWindowPosition() OVERRIDE;
  virtual void SetGeometryHints() OVERRIDE;
  virtual bool UseCustomFrame() OVERRIDE;
  virtual void OnSizeChanged(int width, int height) OVERRIDE;
  virtual void DrawCustomFrame(cairo_t* cr, GtkWidget* widget,
                               GdkEventExpose* event) OVERRIDE;
  virtual void DrawPopupFrame(cairo_t* cr, GtkWidget* widget,
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
      const NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandlePanelKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void FullScreenModeChanged(bool is_full_screen) OVERRIDE;
  virtual Browser* GetPanelBrowser() const OVERRIDE;
  virtual void DestroyPanelBrowser() OVERRIDE;
  virtual gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const OVERRIDE;
  virtual gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const OVERRIDE;
  virtual int TitleOnlyHeight() const OVERRIDE;
  virtual gfx::Size IconOnlySize() const OVERRIDE;
  virtual void EnsurePanelFullyVisible() OVERRIDE;
  virtual void SetPanelAppIconVisibility(bool visible) OVERRIDE;
  virtual void SetPanelAlwaysOnTop(bool on_top) OVERRIDE;
  virtual void EnableResizeByMouse(bool enable) OVERRIDE;

 private:
  friend class NativePanelTestingGtk;

  void StartBoundsAnimation(const gfx::Rect& from_bounds,
                            const gfx::Rect& to_bounds);
  bool IsAnimatingBounds() const;

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Creates helper for handling drags if not already created.
  void EnsureDragHelperCreated();

  void SetBoundsInternal(const gfx::Rect& bounds, bool animate);
  void ResizeWindow(int width, int height);

  void DrawAttentionFrame(cairo_t* cr, GtkWidget* widget,
                          GdkEventExpose* event);
  GdkRectangle GetTitlebarRectForDrawAttention() const;

  CHROMEGTK_CALLBACK_1(PanelBrowserWindowGtk, gboolean,
                       OnTitlebarButtonReleaseEvent, GdkEventButton*);

  scoped_ptr<Panel> panel_;
  gfx::Rect bounds_;

  scoped_ptr<PanelDragGtk> drag_helper_;

  // Size of window frame. Empty until the window has been allocated and sized.
  gfx::Size frame_size_;

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
  scoped_ptr<PanelBoundsAnimation> bounds_animator_;
  gfx::Rect animation_start_bounds_;

  // This records the bounds set on the last animation progress notification.
  // We need this for the case where a new bounds animation starts before the
  // current one completes. In this case, we want to start the new animation
  // from where the last one left.
  gfx::Rect last_animation_progressed_bounds_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserWindowGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_WINDOW_GTK_H_
