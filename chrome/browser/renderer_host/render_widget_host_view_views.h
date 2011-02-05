// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_VIEWS_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_VIEWS_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/native/native_view_host.h"
#include "views/event.h"
#include "views/view.h"
#include "webkit/glue/webcursor.h"

class RenderWidgetHost;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewViews : public RenderWidgetHostView,
                                  public views::View {
 public:
  explicit RenderWidgetHostViewViews(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewViews();

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

  // On some systems, there can be two native views, where an outer native view
  // contains the inner native view (e.g. when using GTK+). This returns the
  // inner view. This can return NULL when it's not attached to a view.
  gfx::NativeView GetInnerNativeView() const;

  virtual void Paint(gfx::Canvas* canvas);

  // Overridden from views::View.
  gfx::NativeCursor GetCursorForPoint(views::Event::EventType type,
                                      const gfx::Point& point);

  // Views mouse events, overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual void OnMouseMoved(const views::MouseEvent& e);
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMouseWheel(const views::MouseWheelEvent& e);

  // Views keyboard events, overridden from views::View.
  virtual bool OnKeyPressed(const views::KeyEvent &e);
  virtual bool OnKeyReleased(const views::KeyEvent &e);

  virtual void DidGainFocus();
  virtual void WillLoseFocus();

  // Forwards a keyboard event to renderer.
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Views touch events, overridden from views::View.
  virtual View::TouchStatus OnTouchEvent(const views::TouchEvent& e);

 private:
  friend class RenderWidgetHostViewViewsWidget;

  // Returns whether the widget needs an input grab to work
  // properly.
  bool NeedsInputGrab();

  // Returns whether this render view is a popup (<select> dropdown or
  // autocomplete window).
  bool IsPopup();

  // Update the display cursor for the render view.
  void ShowCurrentCursor();

  // Translate a views::MouseEvent into a WebKit::WebMouseEvent.
  WebKit::WebMouseEvent WebMouseEventFromViewsEvent(
      const views::MouseEvent& event);

  // The model object.
  RenderWidgetHost* host_;

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

  // The native cursor.
  gfx::NativeCursor native_cursor_;

  // Whether we are showing a context menu.
  bool is_showing_context_menu_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks tab_switch_paint_time_;

  // If true, fade the render widget when painting it.
  bool visually_deemphasized_;

  // The size that we want the renderer to be.
  gfx::Size requested_size_;

  // The touch-event. Its touch-points are updated as necessary. A new
  // touch-point is added from an ET_TOUCH_PRESSED event, and a touch-point is
  // removed from the list on an ET_TOUCH_RELEASED event.
  WebKit::WebTouchEvent touch_event_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewViews);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_VIEWS_H_
