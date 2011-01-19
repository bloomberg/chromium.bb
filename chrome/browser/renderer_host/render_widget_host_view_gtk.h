// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"

class RenderWidgetHost;
class GtkIMContextWrapper;
class GtkKeyBindingsHandler;
#if !defined(TOOLKIT_VIEWS)
class MenuGtk;
#endif
struct NativeWebKeyboardEvent;

#if defined(OS_CHROMEOS)
namespace views {
class TooltipWindowGtk;
}
#endif  // defined(OS_CHROMEOS)

typedef struct _GtkClipboard GtkClipboard;
typedef struct _GtkSelectionData GtkSelectionData;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewGtk : public RenderWidgetHostView,
                                public ui::AnimationDelegate {
 public:
  explicit RenderWidgetHostViewGtk(RenderWidgetHost* widget);
  ~RenderWidgetHostViewGtk();

  // Initialize this object for use as a drawing area.
  void InitAsChild();

  // RenderWidgetHostView implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos);
  virtual void InitAsFullscreen(RenderWidgetHostView* parent_host_view);
  virtual RenderWidgetHost* GetRenderWidgetHost() const;
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void SetSize(const gfx::Size& size);
  virtual gfx::NativeView GetNativeView();
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves);
  virtual void Focus();
  virtual void Blur();
  virtual bool HasFocus();
  virtual void Show();
  virtual void Hide();
  virtual bool IsShowing();
  virtual gfx::Rect GetViewBounds() const;
  virtual void UpdateCursor(const WebCursor& cursor);
  virtual void SetIsLoading(bool is_loading);
  virtual void ImeUpdateTextInputState(WebKit::WebTextInputType type,
                                       const gfx::Rect& caret_rect);
  virtual void ImeCancelComposition();
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects);
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code);
  virtual void Destroy();
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) {}
  virtual void SetTooltipText(const std::wstring& tooltip_text);
  virtual void SelectionChanged(const std::string& text);
  virtual void ShowingContextMenu(bool showing);
  virtual BackingStore* AllocBackingStore(const gfx::Size& size);
  virtual void SetBackground(const SkBitmap& background);
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id);
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id);
  virtual void SetVisuallyDeemphasized(const SkColor* color, bool animate);
  virtual bool ContainsNativeView(gfx::NativeView native_view) const;
  virtual void AcceleratedCompositingActivated(bool activated);

  // ui::AnimationDelegate implementation.
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);
  virtual void AnimationCanceled(const ui::Animation* animation);

  gfx::NativeView native_view() const { return view_.get(); }

  // If the widget is aligned with an edge of the monitor its on and the user
  // attempts to drag past that edge we track the number of times it has
  // occurred, so that we can force the widget to scroll when it otherwise
  // would be unable to.
  void ModifyEventForEdgeDragging(GtkWidget* widget, GdkEventMotion* event);
  void Paint(const gfx::Rect&);

  // Called by GtkIMContextWrapper to forward a keyboard event to renderer.
  // Before calling RenderWidgetHost::ForwardKeyboardEvent(), this method
  // calls GtkKeyBindingsHandler::Match() against the event and send matched
  // edit commands to renderer by calling
  // RenderWidgetHost::ForwardEditCommandsForNextKeyEvent().
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

#if !defined(TOOLKIT_VIEWS)
  // Appends the input methods context menu to the specified |menu| object as a
  // submenu.
  void AppendInputMethodsContextMenu(MenuGtk* menu);
#endif

 private:
  friend class RenderWidgetHostViewGtkWidget;

  // Returns whether the widget needs an input grab (GTK+ and X) to work
  // properly.
  bool NeedsInputGrab();

  // Returns whether this render view is a popup (<select> dropdown or
  // autocomplete window).
  bool IsPopup();

  // Update the display cursor for the render view.
  void ShowCurrentCursor();

  // Helper method for InitAsPopup() and InitAsFullscreen().
  void DoInitAsPopup(
      RenderWidgetHostView* parent_host_view,
      GtkWindowType window_type,
      const gfx::Rect& pos,  // Ignored if is_fullscreen is true.
      bool is_fullscreen);

  // The model object.
  RenderWidgetHost* host_;

  // The native UI widget.
  OwnedWidgetGtk view_;

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

  // Variables used only for popups --------------------------------------------
  // Our parent widget.
  RenderWidgetHostView* parent_host_view_;
  // The native view of our parent, equivalent to
  // parent_host_view_->GetNativeView().
  GtkWidget* parent_;
  // We ignore the first mouse release on popups.  This allows the popup to
  // stay open.
  bool is_popup_first_mouse_release_;

  // Whether or not this widget was focused before shadowed by another widget.
  // Used in OnGrabNotify() handler to track the focused state correctly.
  bool was_focused_before_grab_;

  // True if we are responsible for creating an X grab. This will only be used
  // for <select> dropdowns. It should be true for most such cases, but false
  // for extension popups.
  bool do_x_grab_;

  // A convenience wrapper object for GtkIMContext;
  scoped_ptr<GtkIMContextWrapper> im_context_;

  // A convenience object for handling editor key bindings defined in gtk
  // keyboard theme.
  scoped_ptr<GtkKeyBindingsHandler> key_bindings_handler_;

  // Helper class that lets us allocate plugin containers and move them.
  webkit::npapi::GtkPluginContainerManager plugin_container_manager_;

  // The size that we want the renderer to be.  We keep this in a separate
  // variable because resizing in GTK+ is async.
  gfx::Size requested_size_;

  // The number of times the user has dragged against horizontal edge  of the
  // monitor (if the widget is aligned with that edge). Negative values
  // indicate the left edge, positive the right.
  int dragged_at_horizontal_edge_;

  // The number of times the user has dragged against vertical edge  of the
  // monitor (if the widget is aligned with that edge). Negative values
  // indicate the top edge, positive the bottom.
  int dragged_at_vertical_edge_;

#if defined(OS_CHROMEOS)
  // Custimized tooltip window.
  scoped_ptr<views::TooltipWindowGtk> tooltip_window_;
#endif  // defined(OS_CHROMEOS)
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GTK_H_
