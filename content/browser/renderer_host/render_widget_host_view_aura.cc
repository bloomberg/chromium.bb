// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/logging.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/hit_test.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"

namespace {

int WebInputEventFlagsFromAuraEvent(const aura::Event& event) {
  int modifiers = 0;
  if (event.flags() & ui::EF_SHIFT_DOWN)
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (event.flags() & ui::EF_CONTROL_DOWN)
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (event.flags() & ui::EF_ALT_DOWN)
    modifiers |= WebKit::WebInputEvent::AltKey;
  if (event.flags() & ui::EF_CAPS_LOCK_DOWN)
    modifiers |= WebKit::WebInputEvent::CapsLockOn;
  return modifiers;
}

WebKit::WebInputEvent::Type WebInputEventTypeFromAuraEvent(
    const aura::Event& event) {
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      return WebKit::WebInputEvent::MouseDown;
    case ui::ET_MOUSE_RELEASED:
      return WebKit::WebInputEvent::MouseUp;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      // Drags are treated as moves by WebKit, which does its own drag handling.
      return WebKit::WebInputEvent::MouseMove;
    case ui::ET_MOUSEWHEEL:
      return WebKit::WebInputEvent::MouseWheel;
    default:
      NOTREACHED();
      break;
  }
  return WebKit::WebInputEvent::Undefined;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      is_loading_(false) {
  host_->SetView(this);
}

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
}

void RenderWidgetHostViewAura::Init() {
  window_->Init();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
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
  window_->SetBounds(rect);
  host_->WasResized();
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
  return NULL;
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

bool RenderWidgetHostViewAura::HasFocus() {
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
  return window_->bounds();
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::ImeUpdateTextInputState(
    ui::TextInputType type,
    bool can_compose_inline,
    const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
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
  //NOTIMPLEMENTED();
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreSkia(host_, size);
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
void RenderWidgetHostViewAura::AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) {
}

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
      uint64 surface_id,
      int32 route_id,
      int gpu_host_id) {
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease(uint64 surface_id) {
}
#endif

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->SetBackground(background);
}

#if defined(OS_POSIX)
void RenderWidgetHostViewAura::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::GetScreenInfo(WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

gfx::Rect RenderWidgetHostViewAura::GetRootWindowBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}
#endif

void RenderWidgetHostViewAura::SetVisuallyDeemphasized(const SkColor* color,
                                                       bool animate) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::WillWmDestroy() {
  NOTREACHED();
}

void RenderWidgetHostViewAura::ShowCompositorHostWindow(bool show) {
  NOTREACHED();
}
#endif

gfx::PluginWindowHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  return NULL;
}

bool RenderWidgetHostViewAura::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  NOTIMPLEMENTED();
  host_->LostMouseLock();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

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
  host_->ForwardKeyboardEvent(
      NativeWebKeyboardEvent(
#if defined(OS_WIN)
          event->native_event().hwnd,
          event->native_event().message,
          event->native_event().wParam,
          event->native_event().lParam
#endif
      ));
  return true;
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  // TODO(beng): talk to beng before implementing this.
  //NOTIMPLEMENTED();
  return NULL;
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::OnMouseEvent(aura::MouseEvent* event) {
  // TODO(beng): replace with construction using WebInputEventFactories for
  //             Windows/X using |event|'s native_event() field.

  WebKit::WebMouseEvent webkit_event;
  webkit_event.timeStampSeconds = base::Time::Now().ToDoubleT();
  webkit_event.modifiers = WebInputEventFlagsFromAuraEvent(*event);
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  webkit_event.globalX = webkit_event.x;
  webkit_event.globalY = webkit_event.y;

  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_RELEASED) {
    webkit_event.clickCount = 1;
  }

  if (event->flags() & ui::EF_MIDDLE_BUTTON_DOWN) {
    webkit_event.modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
    webkit_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  }
  if (event->flags() & ui::EF_LEFT_BUTTON_DOWN) {
    webkit_event.modifiers |= WebKit::WebInputEvent::LeftButtonDown;
    webkit_event.button = WebKit::WebMouseEvent::ButtonLeft;
  }
  if (event->flags() & ui::EF_RIGHT_BUTTON_DOWN) {
    webkit_event.modifiers |= WebKit::WebInputEvent::RightButtonDown;
    webkit_event.button = WebKit::WebMouseEvent::ButtonRight;
  }

  webkit_event.type = WebInputEventTypeFromAuraEvent(*event);
  host_->ForwardMouseEvent(webkit_event);

  // Return true so that we receive released/drag events.
  return true;
}

ui::TouchStatus RenderWidgetHostViewAura::OnTouchEvent(
    aura::TouchEvent* event) {
  NOTIMPLEMENTED();
  return ui::TOUCH_STATUS_UNKNOWN;
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
  BackingStore* backing_store = host_->GetBackingStore(true);
  if (backing_store) {
    static_cast<BackingStoreSkia*>(backing_store)->SkiaShowRect(gfx::Point(),
                                                                canvas);
  } else {
    canvas->FillRectInt(SK_ColorWHITE, 0, 0, window_->bounds().width(),
                        window_->bounds().height());
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

#if !defined(TOUCH_UI)
////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  //NOTIMPLEMENTED();
  // TODO(beng): See RenderWidgetHostViewWin.
}
#endif
