// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include <algorithm>
#include <string>

#include "app/keyboard_code_conversion_gtk.h"
#include "app/l10n_util.h"
#include "app/x11_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/renderer_host/backing_store_x.h"
#include "chrome/browser/renderer_host/gpu_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/video_layer_x.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "views/event.h"
#include "views/widget/widget.h"

static const int kMaxWindowWidth = 4000;
static const int kMaxWindowHeight = 4000;
static const char* kRenderWidgetHostViewKey = "__RENDER_WIDGET_HOST_VIEW__";

using WebKit::WebInputEventFactory;
using WebKit::WebMouseWheelEvent;

namespace {

int WebInputEventFlagsFromViewsEvent(const views::Event& event) {
  int modifiers = 0;

  if (event.IsShiftDown())
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (event.IsControlDown())
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (event.IsAltDown())
    modifiers |= WebKit::WebInputEvent::AltKey;

  return modifiers;
}

}  // namespace

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewViews(widget);
}

RenderWidgetHostViewViews::RenderWidgetHostViewViews(RenderWidgetHost* host)
    : host_(host),
      enable_gpu_rendering_(false),
      about_to_validate_and_paint_(false),
      is_hidden_(false),
      is_loading_(false),
      is_showing_context_menu_(false),
      visually_deemphasized_(false) {
  SetFocusable(true);
  host_->set_view(this);

  // Enable experimental out-of-process GPU rendering.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  enable_gpu_rendering_ =
      command_line->HasSwitch(switches::kEnableGPURendering);
}

RenderWidgetHostViewViews::~RenderWidgetHostViewViews() {
}

void RenderWidgetHostViewViews::InitAsChild() {
  Show();
}

RenderWidgetHost* RenderWidgetHostViewViews::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewViews::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  // TODO(anicolao): figure out cases where popups occur and implement
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::InitAsFullscreen(
    RenderWidgetHostView* parent_host_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  if (tab_switch_paint_time_.is_null())
    tab_switch_paint_time_ = base::TimeTicks::Now();
  is_hidden_ = false;
  host_->WasRestored();
}

void RenderWidgetHostViewViews::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  GetRenderWidgetHost()->WasHidden();
}

void RenderWidgetHostViewViews::SetSize(const gfx::Size& size) {
  // This is called when webkit has sent us a Move message.
  int width = std::min(size.width(), kMaxWindowWidth);
  int height = std::min(size.height(), kMaxWindowHeight);
  if (requested_size_.width() != width ||
      requested_size_.height() != height) {
    requested_size_ = gfx::Size(width, height);
    SetBounds(GetViewBounds());
    host_->WasResized();
  }
}

void RenderWidgetHostViewViews::MovePluginWindows(
    const std::vector<webkit_glue::WebPluginGeometry>& moves) {
  // TODO(anicolao): NIY
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::Focus() {
  RequestFocus();
}

bool RenderWidgetHostViewViews::HasFocus() {
  return View::HasFocus();
}

void RenderWidgetHostViewViews::Show() {
  SetVisible(true);
}

void RenderWidgetHostViewViews::Hide() {
  SetVisible(false);
}

void RenderWidgetHostViewViews::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  host_->Blur();
}


bool RenderWidgetHostViewViews::IsShowing() {
  return IsVisible();
}

gfx::Rect RenderWidgetHostViewViews::GetViewBounds() const {
  return bounds();
}

void RenderWidgetHostViewViews::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType()) {
    return;
  }

  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewViews::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
}

void RenderWidgetHostViewViews::ImeUpdateTextInputState(
    WebKit::WebTextInputType type,
    const gfx::Rect& caret_rect) {
  // TODO(bryeung): im_context_->UpdateInputMethodState(type, caret_rect);
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::ImeCancelComposition() {
  // TODO(bryeung): im_context_->CancelComposition();
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (is_hidden_)
    return;

  // TODO(darin): Implement the equivalent of Win32's ScrollWindowEX.  Can that
  // be done using XCopyArea?  Perhaps similar to
  // BackingStore::ScrollBackingStore?
  if (about_to_validate_and_paint_)
    invalid_rect_ = invalid_rect_.Union(scroll_rect);
  else
    SchedulePaint(scroll_rect, false);

  for (size_t i = 0; i < copy_rects.size(); ++i) {
    // Avoid double painting.  NOTE: This is only relevant given the call to
    // Paint(scroll_rect) above.
    gfx::Rect rect = copy_rects[i].Subtract(scroll_rect);
    if (rect.IsEmpty())
      continue;

    if (about_to_validate_and_paint_)
      invalid_rect_ = invalid_rect_.Union(rect);
    else
      SchedulePaint(rect, false);
  }
}

void RenderWidgetHostViewViews::RenderViewGone() {
  Destroy();
}

void RenderWidgetHostViewViews::Destroy() {
  // TODO(anicolao): deal with any special popup cleanup
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::SetTooltipText(const std::wstring& tip) {
  // TODO(anicolao): decide if we want tooltips for touch (none specified
  // right now/might want a press-and-hold display)
  // NOTIMPLEMENTED(); ... too annoying, it triggers for every mousemove
}

void RenderWidgetHostViewViews::SelectionChanged(const std::string& text) {
  // TODO(anicolao): deal with the clipboard without GTK
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

bool RenderWidgetHostViewViews::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

bool RenderWidgetHostViewViews::IsPopup() {
  return popup_type_ != WebKit::WebPopupTypeNone;
}

BackingStore* RenderWidgetHostViewViews::AllocBackingStore(
    const gfx::Size& size) {
  if (enable_gpu_rendering_) {
    // Use a special GPU accelerated backing store.
    if (!gpu_view_host_.get()) {
      // Here we lazily make the GpuViewHost. This must be allocated when we
      // have a native view realized, which happens sometime after creation
      // when our owner puts us in the parent window.
      DCHECK(GetNativeView());
      XID window_xid = x11_util::GetX11WindowFromGtkWidget(GetNativeView());
      gpu_view_host_.reset(new GpuViewHost(host_, window_xid));
    }
    return gpu_view_host_->CreateBackingStore(size);
  }

  return new BackingStoreX(host_, size,
                           x11_util::GetVisualFromGtkWidget(native_view()),
                           gtk_widget_get_visual(native_view())->depth);
}

gfx::NativeView RenderWidgetHostViewViews::native_view() const {
  return GetWidget()->GetNativeView();
}

VideoLayer* RenderWidgetHostViewViews::AllocVideoLayer(const gfx::Size& size) {
  if (enable_gpu_rendering_) {
    // TODO(scherkus): is it possible for a video layer to be allocated before a
    // backing store?
    DCHECK(gpu_view_host_.get())
        << "AllocVideoLayer() called before AllocBackingStore()";
    return gpu_view_host_->CreateVideoLayer(size);
  }

  return new VideoLayerX(host_, size,
                         x11_util::GetVisualFromGtkWidget(native_view()),
                         gtk_widget_get_visual(native_view())->depth);
}

void RenderWidgetHostViewViews::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->routing_id(), background));
}

void RenderWidgetHostViewViews::Paint(gfx::Canvas* canvas) {
  if (enable_gpu_rendering_) {
    // When we're proxying painting, we don't actually display the web page
    // ourselves.
    if (gpu_view_host_.get())
      gpu_view_host_->OnWindowPainted();

    // Erase the background. This will prevent a flash of black when resizing
    // or exposing the window. White is usually better than black.
    return;
  }

  // Don't do any painting if the GPU process is rendering directly
  // into the View.
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost();
  if (render_widget_host->is_gpu_rendering_active()) {
    return;
  }

  GdkWindow* window = native_view()->window;
  DCHECK(!about_to_validate_and_paint_);

  // TODO(anicolao): get the damage somehow
  //invalid_rect_ = damage_rect;
  invalid_rect_ = bounds();
  about_to_validate_and_paint_ = true;
  BackingStoreX* backing_store = static_cast<BackingStoreX*>(
      host_->GetBackingStore(true));
  // Calling GetBackingStore maybe have changed |invalid_rect_|...
  about_to_validate_and_paint_ = false;

  gfx::Rect paint_rect = gfx::Rect(0, 0, kMaxWindowWidth, kMaxWindowHeight);
  paint_rect = paint_rect.Intersect(invalid_rect_);

  if (backing_store) {
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (window) {
      if (!visually_deemphasized_) {
        // In the common case, use XCopyArea. We don't draw more than once, so
        // we don't need to double buffer.
        backing_store->XShowRect(
            paint_rect, x11_util::GetX11WindowFromGtkWidget(native_view()));

        // Paint the video layer using XCopyArea.
        // TODO(scherkus): implement VideoLayerX::CairoShow() for grey
        // blending.
        VideoLayerX* video_layer = static_cast<VideoLayerX*>(
            host_->video_layer());
        if (video_layer)
          video_layer->XShow(
              x11_util::GetX11WindowFromGtkWidget(native_view()));
      } else {
        // If the grey blend is showing, we make two drawing calls. Use double
        // buffering to prevent flicker. Use CairoShowRect because XShowRect
        // shortcuts GDK's double buffering.
        GdkRectangle rect = { paint_rect.x(), paint_rect.y(),
                              paint_rect.width(), paint_rect.height() };
        gdk_window_begin_paint_rect(window, &rect);

        backing_store->CairoShowRect(paint_rect, GDK_DRAWABLE(window));

        cairo_t* cr = gdk_cairo_create(window);
        gdk_cairo_rectangle(cr, &rect);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
        cairo_fill(cr);
        cairo_destroy(cr);

        gdk_window_end_paint(window);
      }
    }
    if (!whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      whiteout_start_time_ = base::TimeTicks();
    }
    if (!tab_switch_paint_time_.is_null()) {
      base::TimeDelta tab_switch_paint_duration = base::TimeTicks::Now() -
          tab_switch_paint_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration",
          tab_switch_paint_duration);
      // Reset tab_switch_paint_time_ to 0 so future tab selections are
      // recorded.
      tab_switch_paint_time_ = base::TimeTicks();
    }
  } else {
    if (window)
      gdk_window_clear(window);
    if (whiteout_start_time_.is_null())
      whiteout_start_time_ = base::TimeTicks::Now();
  }
}

bool RenderWidgetHostViewViews::OnMousePressed(const views::MouseEvent& event) {
  RequestFocus();

  // TODO(anicolao): validate event generation.
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  // TODO(anicolao): deal with double clicks
  e.type = WebKit::WebInputEvent::MouseDown;
  e.clickCount = 1;

  GetRenderWidgetHost()->ForwardMouseEvent(e);
  return true;
}

void RenderWidgetHostViewViews::OnMouseReleased(const views::MouseEvent& event,
                                                bool canceled) {
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  e.type = WebKit::WebInputEvent::MouseUp;
  e.clickCount = 1;

  GetRenderWidgetHost()->ForwardMouseEvent(e);
}

bool RenderWidgetHostViewViews::OnMouseDragged(const views::MouseEvent& event) {
  OnMouseMoved(event);
  return true;
}

void RenderWidgetHostViewViews::OnMouseMoved(const views::MouseEvent& event) {
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  e.type = WebKit::WebInputEvent::MouseMove;

  GetRenderWidgetHost()->ForwardMouseEvent(e);
}

void RenderWidgetHostViewViews::OnMouseEntered(const views::MouseEvent& event) {
  // Already generated synthetically by webkit.
}

void RenderWidgetHostViewViews::OnMouseExited(const views::MouseEvent& event) {
  // Already generated synthetically by webkit.
}

bool RenderWidgetHostViewViews::OnMouseWheel(const views::MouseWheelEvent& e) {
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewViews::OnKeyPressed(const views::KeyEvent &e) {
  // Send key event to input method.
  // TODO host_view->im_context_->ProcessKeyEvent(event);
  NativeWebKeyboardEvent wke;

  wke.type = WebKit::WebInputEvent::KeyDown;
  wke.windowsKeyCode = e.GetKeyCode();
  wke.setKeyIdentifierFromWindowsKeyCode();

  wke.text[0] = wke.unmodifiedText[0] =
    static_cast<unsigned short>(gdk_keyval_to_unicode(
          app::GdkKeyCodeForWindowsKeyCode(e.GetKeyCode(), false /*shift*/)));

  wke.modifiers = WebInputEventFlagsFromViewsEvent(e);
  ForwardKeyboardEvent(wke);

  // send the keypress event
  wke.type = WebKit::WebInputEvent::Char;

  // TODO(anicolao): fear this comment from GTK land
  // We return TRUE because we did handle the event. If it turns out webkit
  // can't handle the event, we'll deal with it in
  // RenderView::UnhandledKeyboardEvent().

  return TRUE;
}

bool RenderWidgetHostViewViews::OnKeyReleased(const views::KeyEvent &e) {
  // TODO(bryeung): deal with input methods
  NativeWebKeyboardEvent wke;

  wke.type = WebKit::WebInputEvent::KeyUp;
  wke.windowsKeyCode = e.GetKeyCode();
  wke.setKeyIdentifierFromWindowsKeyCode();

  ForwardKeyboardEvent(wke);

  return TRUE;
}

void RenderWidgetHostViewViews::DidGainFocus() {
#if 0
  // TODO(anicolao): - is this needed/replicable?
  // Comes from the GTK equivalent.

  int x, y;
  gtk_widget_get_pointer(native_view(), &x, &y);
  // http://crbug.com/13389
  // If the cursor is in the render view, fake a mouse move event so that
  // webkit updates its state. Otherwise webkit might think the cursor is
  // somewhere it's not.
  if (x >= 0 && y >= 0 && x < native_view()->allocation.width &&
      y < native_view()->allocation.height) {
    WebKit::WebMouseEvent fake_event;
    fake_event.timeStampSeconds = base::Time::Now().ToDoubleT();
    fake_event.modifiers = 0;
    fake_event.windowX = fake_event.x = x;
    fake_event.windowY = fake_event.y = y;
    gdk_window_get_origin(native_view()->window, &x, &y);
    fake_event.globalX = fake_event.x + x;
    fake_event.globalY = fake_event.y + y;
    fake_event.type = WebKit::WebInputEvent::MouseMove;
    fake_event.button = WebKit::WebMouseEvent::ButtonNone;
    GetRenderWidgetHost()->ForwardMouseEvent(fake_event);
  }
#endif

  ShowCurrentCursor();
  GetRenderWidgetHost()->GotFocus();
}

void RenderWidgetHostViewViews::WillLoseFocus() {
  // If we are showing a context menu, maintain the illusion that webkit has
  // focus.
  if (!is_showing_context_menu_)
    GetRenderWidgetHost()->Blur();
}


void RenderWidgetHostViewViews::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!native_view()->window)
    return;

  // TODO(anicolao): change to set cursors without GTK
}

void RenderWidgetHostViewViews::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.CreatePluginContainer(id);
}

void RenderWidgetHostViewViews::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.DestroyPluginContainer(id);
}

void RenderWidgetHostViewViews::SetVisuallyDeemphasized(bool deemphasized) {
  // TODO(anicolao)
}

bool RenderWidgetHostViewViews::ContainsNativeView(
    gfx::NativeView native_view) const {
  // TODO(port)
  NOTREACHED() <<
    "RenderWidgetHostViewViews::ContainsNativeView not implemented.";
  return false;
}

WebKit::WebMouseEvent RenderWidgetHostViewViews::WebMouseEventFromViewsEvent(
    const views::MouseEvent& event) {
  WebKit::WebMouseEvent wmevent;

  wmevent.timeStampSeconds = base::Time::Now().ToDoubleT();
  wmevent.windowX = wmevent.x = event.x();
  wmevent.windowY = wmevent.y = event.y();
  int x, y;
  gdk_window_get_origin(GetNativeView()->window, &x, &y);
  wmevent.globalX = wmevent.x + x;
  wmevent.globalY = wmevent.y + y;
  wmevent.modifiers = WebInputEventFlagsFromViewsEvent(event);

  // Setting |wmevent.button| is not necessary for -move events, but it is
  // necessary for -clicks and -drags.
  if (event.IsMiddleMouseButton()) {
    wmevent.modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
    wmevent.button = WebKit::WebMouseEvent::ButtonMiddle;
  }
  if (event.IsRightMouseButton()) {
    wmevent.modifiers |= WebKit::WebInputEvent::RightButtonDown;
    wmevent.button = WebKit::WebMouseEvent::ButtonRight;
  }
  if (event.IsLeftMouseButton()) {
    wmevent.modifiers |= WebKit::WebInputEvent::LeftButtonDown;
    wmevent.button = WebKit::WebMouseEvent::ButtonLeft;
  }

  return wmevent;
}

void RenderWidgetHostViewViews::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

  EditCommands edit_commands;
#if 0
TODO(bryeung): key bindings
  if (!event.skip_in_browser &&
      key_bindings_handler_->Match(event, &edit_commands)) {
    host_->ForwardEditCommandsForNextKeyEvent(edit_commands);
  }
#endif
  host_->ForwardKeyboardEvent(event);
}

// static
RenderWidgetHostView*
    RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView(
        gfx::NativeView widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget),
                                         kRenderWidgetHostViewKey);
  return reinterpret_cast<RenderWidgetHostView*>(user_data);
}
