// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_VIEWS_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_VIEWS_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/compositor/compositor_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/events/event.h"
#include "ui/views/touchui/touch_selection_controller.h"
#include "views/controls/native/native_view_host.h"
#include "views/view.h"
#include "webkit/glue/webcursor.h"

#if defined(TOUCH_UI)
namespace ui {
enum TouchStatus;
}
#endif

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
class AcceleratedSurfaceContainerLinux;
#endif

class RenderWidgetHost;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewViews : public RenderWidgetHostView,
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
                                  public ui::CompositorObserver,
#endif
                                  public views::TouchSelectionClientView,
                                  public ui::TextInputClient,
                                  public content::NotificationObserver {
 public:
  // Internal class name.
  static const char kViewClassName[];

  explicit RenderWidgetHostViewViews(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewViews();

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
#if defined(OS_POSIX) || defined(USE_AURA)
  virtual void GetDefaultScreenInfo(WebKit::WebScreenInfo* results);
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetRootWindowBounds() OVERRIDE;
#endif
#if defined(TOOLKIT_USES_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
#endif
  virtual void SetVisuallyDeemphasized(const SkColor* color,
                                       bool animate) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() OVERRIDE;
#endif
  virtual gfx::PluginWindowHandle GetCompositingSurface() OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

  // Overridden from views::TouchSelectionClientView.
  virtual void SelectRect(const gfx::Point& start,
                          const gfx::Point& end) OVERRIDE;

  // Overriden from content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ui::SimpleMenuModel::Delegate.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Overridden from views::View.
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
#if defined(TOUCH_UI)
  virtual ui::TouchStatus OnTouchEvent(const views::TouchEvent& event) OVERRIDE;
#endif
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const views::MouseWheelEvent& event) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;

  // Overridden from ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() OVERRIDE;
  virtual bool HasCompositionText() OVERRIDE;
  virtual bool GetTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetCompositionTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetSelectionRange(ui::Range* range) OVERRIDE;
  virtual bool SetSelectionRange(const ui::Range& range) OVERRIDE;
  virtual bool DeleteRange(const ui::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const ui::Range& range,
                                string16* text) OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  virtual void AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      uint64 surface_id,
      int32 route_id,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceRelease(uint64 surface_id) OVERRIDE;

  // CompositorObserver implementation:
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE;
#endif

 protected:
  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // Overridden from RenderWidgetHostView
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;

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

  // Confirm existing composition text in the webpage and ask the input method
  // to cancel its ongoing composition sesstion.
  void FinishImeCompositionSession();

  // Updates the touch-selection controller (e.g. when the selection/focus
  // changes).
  void UpdateTouchSelectionController();

#if defined(TOOLKIT_USES_GTK)
  // Returns true if the RWHV is ready to paint the content.
  bool IsReadyToPaint();
#endif

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

  // The current text input type.
  ui::TextInputType text_input_type_;

  // Rectangles before and after the selection.
  gfx::Rect selection_start_rect_;
  gfx::Rect selection_end_rect_;

  // Indicates if there is onging composition text.
  bool has_composition_text_;

  string16 tooltip_text_;

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  std::vector< base::Callback<void(void)> > on_compositing_ended_callbacks_;
#endif

  scoped_ptr<views::TouchSelectionController> touch_selection_controller_;
  base::WeakPtrFactory<RenderWidgetHostViewViews> weak_factory_;

#if defined(TOUCH_UI)
  // used to register for keyboard visiblity notificatons.
  content::NotificationRegistrar registrar_;
  gfx::Rect keyboard_rect_;
#endif

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  std::map<uint64, scoped_refptr<AcceleratedSurfaceContainerLinux> >
      accelerated_surface_containers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewViews);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_VIEWS_H_
