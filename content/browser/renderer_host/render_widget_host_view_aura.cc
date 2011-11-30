// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/logging.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/common/gpu/gpu_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
#include "base/bind.h"
#include "content/browser/renderer_host/accelerated_surface_container_linux.h"
#include "ui/gfx/gl/gl_bindings.h"
#endif

using WebKit::WebTouchEvent;

namespace {

ui::TouchStatus DecideTouchStatus(const WebKit::WebTouchEvent& event,
                                  WebKit::WebTouchPoint* point) {
  if (event.type == WebKit::WebInputEvent::TouchStart &&
      event.touchesLength == 1)
    return ui::TOUCH_STATUS_START;

  if (event.type == WebKit::WebInputEvent::TouchMove && point == NULL)
    return ui::TOUCH_STATUS_CONTINUE;

  if (event.type == WebKit::WebInputEvent::TouchEnd &&
      event.touchesLength == 0)
    return ui::TOUCH_STATUS_END;

  if (event.type == WebKit::WebInputEvent::TouchCancel)
    return ui::TOUCH_STATUS_CANCEL;

  return point ? ui::TOUCH_STATUS_CONTINUE : ui::TOUCH_STATUS_UNKNOWN;
}

void UpdateWebTouchEventAfterDispatch(WebKit::WebTouchEvent* event,
                                      WebKit::WebTouchPoint* point) {
  if (point->state != WebKit::WebTouchPoint::StateReleased)
    return;
  --event->touchesLength;
  for (unsigned i = point - event->touches;
       i < event->touchesLength;
       ++i) {
    event->touches[i] = event->touches[i + 1];
  }
}

bool CanRendererHandleEvent(const base::NativeEvent& native_event) {
#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON events.
  switch (native_event.message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_NCXBUTTONDBLCLK:
      return false;
    default:
      break;
  }
#endif
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      is_fullscreen_(false),
      popup_parent_host_view_(NULL),
      is_loading_(false),
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
      current_surface_(gfx::kNullPluginWindow),
#endif
      skip_schedule_paint_(false) {
  host_->SetView(this);
  window_->SetProperty(aura::kTooltipTextKey, &tooltip_);
}

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
}

void RenderWidgetHostViewAura::InitAsChild() {
  window_->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window_->SetName("RenderWidgetHostViewAura");
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);
  window_->SetType(aura::WINDOW_TYPE_MENU);
  window_->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window_->SetName("RenderWidgetHostViewAura");

  window_->SetParent(NULL);
  Show();

  // |pos| is in desktop coordinates. So convert it to
  // |popup_parent_host_view_|'s coordinates first.
  gfx::Point origin = pos.origin();
  aura::Window::ConvertPointToWindow(
      aura::Desktop::GetInstance(),
      popup_parent_host_view_->window_, &origin);
  SetBounds(gfx::Rect(origin, pos.size()));
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  is_fullscreen_ = true;
  window_->SetType(aura::WINDOW_TYPE_NORMAL);
  window_->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window_->SetName("RenderWidgetHostViewAura");
  window_->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window_->SetParent(NULL);
  Show();
  Focus();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::DidBecomeSelected() {
  host_->WasRestored();
}

void RenderWidgetHostViewAura::WasHidden() {
  host_->WasHidden();
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  gfx::Rect adjusted_rect = rect;

  if (popup_parent_host_view_) {
    gfx::Point translated_origin = adjusted_rect.origin();
    // |rect| is relative to |popup_parent_host_view_|; translate it for the
    // window's container.
    aura::Window::ConvertPointToWindow(
        popup_parent_host_view_->window_,
        window_->parent(),
        &translated_origin);
    adjusted_rect.set_origin(translated_origin);
  }

  window_->SetBounds(adjusted_rect);
  host_->WasResized();
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
  return static_cast<gfx::NativeViewId>(NULL);
}

void RenderWidgetHostViewAura::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // We don't support windowed plugins.
}

void RenderWidgetHostViewAura::Focus() {
  window_->Focus();
}

void RenderWidgetHostViewAura::Blur() {
  window_->Blur();
}

bool RenderWidgetHostViewAura::HasFocus() const {
  return window_->HasFocus();
}

void RenderWidgetHostViewAura::Show() {
  window_->Show();
}

void RenderWidgetHostViewAura::Hide() {
  window_->Hide();
}

bool RenderWidgetHostViewAura::IsShowing() {
  return window_->IsVisible();
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() const {
  return window_->GetScreenBounds();
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::TextInputStateChanged(
    ui::TextInputType type,
    bool can_compose_inline) {
  // http://crbug.com/102569
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  // http://crbug.com/102569
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (!window_->IsVisible() || skip_schedule_paint_)
    return;

  if (!scroll_rect.IsEmpty())
    window_->SchedulePaintInRect(scroll_rect);

  for (size_t i = 0; i < copy_rects.size(); ++i) {
    gfx::Rect rect = copy_rects[i].Subtract(scroll_rect);
    if (rect.IsEmpty())
      continue;

    window_->SchedulePaintInRect(rect);
  }
}

void RenderWidgetHostViewAura::RenderViewGone(base::TerminationStatus status,
                                              int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::Destroy() {
  delete window_;
}

void RenderWidgetHostViewAura::SetTooltipText(const string16& tooltip_text) {
  tooltip_ = tooltip_text;
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreSkia(host_, size);
}

void RenderWidgetHostViewAura::OnAcceleratedCompositingStateChange() {
  UpdateExternalTexture();
}

void RenderWidgetHostViewAura::UpdateExternalTexture() {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  if (current_surface_ != gfx::kNullPluginWindow &&
      host_->is_accelerated_compositing_active()) {
    window_->layer()->SetExternalTexture(
        accelerated_surface_containers_[current_surface_]->GetTexture());
    glFlush();
  } else {
    window_->layer()->SetExternalTexture(NULL);
  }
#endif
}

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  current_surface_ = params.surface_id;
  UpdateExternalTexture();

  if (!window_->layer()->GetCompositor()) {
    // We have no compositor, so we have no way to display the surface.
    // Must still send the ACK.
    RenderWidgetHost::AcknowledgeSwapBuffers(params.route_id, gpu_host_id);
  } else {
    window_->layer()->ScheduleDraw();

    // Add sending an ACK to the list of things to do OnCompositingEnded
    on_compositing_ended_callbacks_.push_back(
        base::Bind(&RenderWidgetHost::AcknowledgeSwapBuffers,
                   params.route_id, gpu_host_id));
    ui::Compositor* compositor = window_->layer()->GetCompositor();
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
#else
  NOTREACHED();
#endif
}

void RenderWidgetHostViewAura::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  window_->layer()->SetExternalTexture(
      accelerated_surface_containers_[params.surface_id]->GetTexture());
  glFlush();

  if (!window_->layer()->GetCompositor()) {
    // We have no compositor, so we have no way to display the surface
    // Must still send the ACK
    RenderWidgetHost::AcknowledgePostSubBuffer(params.route_id, gpu_host_id);
  } else {
    // TODO(backer): Plumb the damage rect to the ui compositor so that we
    // can do a partial swap to display.
    window_->layer()->ScheduleDraw();

    // Add sending an ACK to the list of things to do OnCompositingEnded
    on_compositing_ended_callbacks_.push_back(
        base::Bind(&RenderWidgetHost::AcknowledgePostSubBuffer,
                   params.route_id, gpu_host_id));
    ui::Compositor* compositor = window_->layer()->GetCompositor();
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
#else
  NOTREACHED();
#endif
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
void RenderWidgetHostViewAura::AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) {
  scoped_refptr<AcceleratedSurfaceContainerLinux> surface(
      AcceleratedSurfaceContainerLinux::Create(gfx::Size(width, height)));
  if (!surface->Initialize(surface_id)) {
    LOG(ERROR) << "Failed to create AcceleratedSurfaceContainer";
    return;
  }
  *surface_handle = surface->Handle();

  accelerated_surface_containers_[*surface_id] = surface;
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease(uint64 surface_id) {
  if (current_surface_ == surface_id) {
    current_surface_ = gfx::kNullPluginWindow;
    // Don't call UpdateExternalTexture: it's possible that a new surface with
    // the same ID will be re-created right away, in which case we don't want to
    // flip back and forth. Instead wait until we got the accelerated
    // compositing deactivation.
  }
  accelerated_surface_containers_.erase(surface_id);
}
#endif

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->SetBackground(background);
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  window_->layer()->SetFillsBoundsOpaquely(background.isOpaque());
#endif
}

void RenderWidgetHostViewAura::GetScreenInfo(WebKit::WebScreenInfo* results) {
  GetDefaultScreenInfo(results);
}

gfx::Rect RenderWidgetHostViewAura::GetRootWindowBounds() {
  // TODO(beng): this is actually wrong, we are supposed to return the bounds
  //             of the container "top level" window, but we don't have a firm
  //             concept of what constitutes a toplevel right now, so just do
  //             this.
  return window_->GetScreenBounds();
}

void RenderWidgetHostViewAura::SetVisuallyDeemphasized(const SkColor* color,
                                                       bool animate) {
  // http://crbug.com/102568
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // Not needed. Mac-only.
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
gfx::PluginWindowHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  // The GPU process renders to an offscreen surface (created by the GPU
  // process), which is later displayed by the browser. As the GPU process
  // creates this surface, we can return any non-zero value.
  return 1;
}
#else
gfx::PluginWindowHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  // TODO(oshima): The original implementation was broken as
  // GtkNativeViewManager doesn't know about NativeWidgetGtk. Figure
  // out if this makes sense without compositor. If it does, then find
  // out the right way to handle.
  NOTIMPLEMENTED();
  return gfx::kNullPluginWindow;
}
#endif

bool RenderWidgetHostViewAura::LockMouse() {
  // http://crbug.com/102563
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  // http://crbug.com/102563
  NOTIMPLEMENTED();
  host_->LostMouseLock();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

gfx::Size RenderWidgetHostViewAura::GetMinimumSize() const {
  return gfx::Size();
}

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  // We don't care about this one, we are always sized via SetSize() or
  // SetBounds().
}

void RenderWidgetHostViewAura::OnFocus() {
  host_->GotFocus();
}

void RenderWidgetHostViewAura::OnBlur() {
  host_->Blur();
}

bool RenderWidgetHostViewAura::OnKeyEvent(aura::KeyEvent* event) {
  // We need to handle the Escape key for Pepper Flash.
  if (is_fullscreen_ && event->key_code() == ui::VKEY_ESCAPE) {
    host_->Shutdown();
  } else {
    NativeWebKeyboardEvent webkit_event(event);
    host_->ForwardKeyboardEvent(webkit_event);
  }
  return true;
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::OnMouseEvent(aura::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSEWHEEL)
    host_->ForwardWheelEvent(content::MakeWebMouseWheelEvent(event));
  else if (CanRendererHandleEvent(event->native_event()))
    host_->ForwardMouseEvent(content::MakeWebMouseEvent(event));

  // Return true so that we receive released/drag events.
  return true;
}

ui::TouchStatus RenderWidgetHostViewAura::OnTouchEvent(
    aura::TouchEvent* event) {
  // Update the touch event first.
  WebKit::WebTouchPoint* point = content::UpdateWebTouchEvent(event,
      &touch_event_);

  // Forward the touch event only if a touch point was updated.
  if (point) {
    host_->ForwardTouchEvent(touch_event_);
    UpdateWebTouchEventAfterDispatch(&touch_event_, point);
  }

  return DecideTouchStatus(touch_event_, point);
}

bool RenderWidgetHostViewAura::ShouldActivate(aura::Event* event) {
  return false;
}

void RenderWidgetHostViewAura::OnActivated() {
}

void RenderWidgetHostViewAura::OnLostActive() {
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host_->LostCapture();
}

void RenderWidgetHostViewAura::OnPaint(gfx::Canvas* canvas) {
  if (!window_->IsVisible())
    return;
  skip_schedule_paint_ = true;
  BackingStore* backing_store = host_->GetBackingStore(true);
  skip_schedule_paint_ = false;
  if (backing_store) {
    static_cast<BackingStoreSkia*>(backing_store)->SkiaShowRect(gfx::Point(),
                                                                canvas);
  } else {
    canvas->FillRect(SK_ColorWHITE,
                     gfx::Rect(gfx::Point(), window_->bounds().size()));
  }
}

void RenderWidgetHostViewAura::OnWindowDestroying() {
}

void RenderWidgetHostViewAura::OnWindowDestroyed() {
  host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostViewAura::OnWindowVisibilityChanged(bool visible) {
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::CompositorDelegate implementation:

void RenderWidgetHostViewAura::OnCompositingEnded(ui::Compositor* compositor) {
  for (std::vector< base::Callback<void(void)> >::const_iterator
      it = on_compositing_ended_callbacks_.begin();
      it != on_compositing_ended_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_ended_callbacks_.clear();
  compositor->RemoveObserver(this);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  const gfx::Point screen_point = gfx::Screen::GetCursorScreenPoint();
  aura::Desktop* desktop = aura::Desktop::GetInstance();
  if (desktop->GetEventHandlerForPoint(screen_point) != window_)
    return;

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  if (is_loading_ && cursor == aura::kCursorPointer)
    cursor = aura::kCursorProgress;

  aura::Desktop::GetInstance()->SetCursor(cursor);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  const gfx::Size size = gfx::Screen::GetPrimaryMonitorSize();
  results->rect = WebKit::WebRect(0, 0, size.width(), size.height());
  results->availableRect = results->rect;
  // TODO(derat): Don't hardcode this?
  results->depth = 24;
  results->depthPerComponent = 8;
}
