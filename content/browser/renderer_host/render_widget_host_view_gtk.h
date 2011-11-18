// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/content_export.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"

class RenderWidgetHost;
class GtkIMContextWrapper;
struct NativeWebKeyboardEvent;

#if defined(OS_CHROMEOS)
namespace ui {
class TooltipWindowGtk;
}
#else
class GtkKeyBindingsHandler;
#endif  // defined(OS_CHROMEOS)

typedef struct _GtkClipboard GtkClipboard;
typedef struct _GtkSelectionData GtkSelectionData;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewGtk : public RenderWidgetHostView,
                                               public ui::AnimationDelegate {
 public:
  explicit RenderWidgetHostViewGtk(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewGtk();

  // Initialize this object for use as a drawing area.
  void InitAsChild();

  // RenderWidgetHostView implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void DidBecomeSelected() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(ui::TextInputType type,
                                     bool can_compose_inline) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) {}
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const string16& text,
                                size_t offset,
                                const ui::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(const gfx::Rect& start_rect,
                                      const gfx::Rect& end_rect) OVERRIDE;
  virtual void ShowingContextMenu(bool showing) OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
  virtual void SetVisuallyDeemphasized(const SkColor* color,
                                       bool animate) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetRootWindowBounds() OVERRIDE;
  virtual gfx::PluginWindowHandle GetCompositingSurface() OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

  // ui::AnimationDelegate implementation.
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  gfx::NativeView native_view() const { return view_.get(); }

  // If the widget is aligned with an edge of the monitor its on and the user
  // attempts to drag past that edge we track the number of times it has
  // occurred, so that we can force the widget to scroll when it otherwise
  // would be unable to.
  void ModifyEventForEdgeDragging(GtkWidget* widget, GdkEventMotion* event);

  // Mouse events always provide a movementX/Y which needs to be computed.
  // Also, mouse lock requires knowledge of last unlocked cursor coordinates.
  // State is stored on the host view to do this, and the mouse event modified.
  void ModifyEventMovementAndCoords(WebKit::WebMouseEvent* event);

  void Paint(const gfx::Rect&);

  // Called by GtkIMContextWrapper to forward a keyboard event to renderer.
  // On Linux (not ChromeOS):
  // Before calling RenderWidgetHost::ForwardKeyboardEvent(), this method
  // calls GtkKeyBindingsHandler::Match() against the event and send matched
  // edit commands to renderer by calling
  // RenderWidgetHost::ForwardEditCommandsForNextKeyEvent().
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

  GdkEventButton* last_mouse_down() const {
    return last_mouse_down_;
  }

#if !defined(TOOLKIT_VIEWS)
  // Builds a submenu containing all the gtk input method commands.
  GtkWidget* BuildInputMethodsGtkMenu();
#endif

 private:
  friend class RenderWidgetHostViewGtkWidget;

  CHROMEGTK_CALLBACK_1(RenderWidgetHostViewGtk,
                       gboolean,
                       OnWindowStateEvent,
                       GdkEventWindowState*);

  CHROMEGTK_CALLBACK_0(RenderWidgetHostViewGtk,
                       void,
                       OnDestroy);

  // Returns whether the widget needs an input grab (GTK+ and X) to work
  // properly.
  bool NeedsInputGrab();

  // Returns whether this render view is a popup (<select> dropdown or
  // autocomplete window).
  bool IsPopup() const;

  // Do initialization needed by all InitAs*() methods.
  void DoSharedInit();

  // Do initialization needed just by InitAsPopup() and InitAsFullscreen().
  // We move and resize |window| to |bounds| and show it and its contents.
  void DoPopupOrFullscreenInit(GtkWindow* window, const gfx::Rect& bounds);

  // Update the display cursor for the render view.
  void ShowCurrentCursor();

  void set_last_mouse_down(GdkEventButton* event);

  gfx::Point GetWidgetCenter();

  // The model object.
  RenderWidgetHost* host_;

  // The native UI widget.
  ui::OwnedWidgetGtk view_;

  // This is true when we are currently painting and thus should handle extra
  // paint requests by expanding the invalid rect rather than actually
  // painting.
  bool about_to_validate_and_paint_;

  // This is the rectangle which we'll paint.
  gfx::Rect invalid_rect_;

  // Whether or not this widget is hidden.
  bool is_hidden_;

  // Whether we are currently loading.
  bool is_loading_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Whether we are showing a context menu.
  bool is_showing_context_menu_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks tab_switch_paint_time_;

  // A color we use to shade the entire render view. If 100% transparent, we do
  // not shade the render view.
  SkColor overlay_color_;

  // The animation used for the abovementioned shade effect. The animation's
  // value affects the alpha we use for |overlay_color_|.
  ui::SlideAnimation overlay_animation_;

  // The native view of our parent widget.  Used only for popups.
  GtkWidget* parent_;

  // We ignore the first mouse release on popups so the popup will remain open.
  bool is_popup_first_mouse_release_;

  // Whether or not this widget's input context was focused before being
  // shadowed by another widget. Used in OnGrabNotify() handler to track the
  // focused state correctly.
  bool was_imcontext_focused_before_grab_;

  // True if we are responsible for creating an X grab. This will only be used
  // for <select> dropdowns. It should be true for most such cases, but false
  // for extension popups.
  bool do_x_grab_;

  // Is the widget fullscreen?
  bool is_fullscreen_;

  // Used to record the last position of the mouse.
  // While the mouse is locked, they store the last known position just as mouse
  // lock was entered.
  // Relative to the upper-left corner of the view.
  gfx::Point unlocked_mouse_position_;
  // Relative to the upper-left corner of the screen.
  gfx::Point unlocked_global_mouse_position_;
  // Last hidden cursor position. Relative to screen.
  gfx::Point global_mouse_position_;
  // Indicates when mouse motion is valid after the widget has moved.
  bool mouse_has_been_warped_to_new_center_;

  // For full-screen windows we have a OnDestroy handler that we need to remove,
  // so we keep it ID here.
  unsigned long destroy_handler_id_;

  // A convenience wrapper object for GtkIMContext;
  scoped_ptr<GtkIMContextWrapper> im_context_;

#if !defined(OS_CHROMEOS)
  // A convenience object for handling editor key bindings defined in gtk
  // keyboard theme.
  scoped_ptr<GtkKeyBindingsHandler> key_bindings_handler_;
#endif

  // Helper class that lets us allocate plugin containers and move them.
  webkit::npapi::GtkPluginContainerManager plugin_container_manager_;

  // The size that we want the renderer to be.  We keep this in a separate
  // variable because resizing in GTK+ is async.
  gfx::Size requested_size_;

  // The latest reported center of the widget, use GetWidgetCenter() to access.
  gfx::Point widget_center_;
  // If the window moves the widget_center will not be valid until we recompute.
  bool widget_center_valid_;

  // The number of times the user has dragged against horizontal edge  of the
  // monitor (if the widget is aligned with that edge). Negative values
  // indicate the left edge, positive the right.
  int dragged_at_horizontal_edge_;

  // The number of times the user has dragged against vertical edge  of the
  // monitor (if the widget is aligned with that edge). Negative values
  // indicate the top edge, positive the bottom.
  int dragged_at_vertical_edge_;

  gfx::PluginWindowHandle compositing_surface_;

  // The event for the last mouse down we handled. We need this for context
  // menus and drags.
  GdkEventButton* last_mouse_down_;

  string16 selection_text_;
  size_t selection_text_offset_;
  ui::Range selection_range_;

#if defined(OS_CHROMEOS)
  // Custimized tooltip window.
  scoped_ptr<ui::TooltipWindowGtk> tooltip_window_;
#endif  // defined(OS_CHROMEOS)

  ui::GtkSignalRegistrar signals_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
