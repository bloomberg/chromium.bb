// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/logging.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/hit_test.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
#include "base/bind.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/accelerated_surface_container_linux.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/gl/gl_bindings.h"
#endif

namespace {

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
void AcknowledgeSwapBuffers(int32 route_id, int gpu_host_id) {
  // It's possible that gpu_host_id is no longer valid at this point (like if
  // gpu process was restarted after a crash).  SendToGpuHost handles this.
  GpuProcessHostUIShim::SendToGpuHost(gpu_host_id,
      new AcceleratedSurfaceMsg_BuffersSwappedACK(route_id));
}
#endif

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

void RenderWidgetHostViewAura::TextInputStateChanged(
    ui::TextInputType type,
    bool can_compose_inline) {
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
  scoped_ptr<AcceleratedSurfaceContainerLinux> surface(
      AcceleratedSurfaceContainerLinux::CreateAcceleratedSurfaceContainer(
          gfx::Size(width, height)));
  if (!surface->Initialize(surface_id)) {
    LOG(ERROR) << "Failed to create AcceleratedSurfaceContainer";
    return;
  }
  *surface_handle = surface->Handle();

  accelerated_surface_containers_[*surface_id] = surface.release();
}

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
      uint64 surface_id,
      int32 route_id,
      int gpu_host_id) {
  window_->layer()->SetExternalTexture(
      accelerated_surface_containers_[surface_id].get());
  glFlush();

  if (!window_->layer()->GetCompositor()) {
    // We have no compositor, so we have no way to display the surface
    AcknowledgeSwapBuffers(route_id, gpu_host_id);  // Must still send the ACK
  } else {
    // Add sending an ACK to the list of things to do OnCompositingEnded
    on_compositing_ended_callbacks_.push_back(
        base::Bind(AcknowledgeSwapBuffers, route_id, gpu_host_id));
    ui::Compositor* compositor = window_->layer()->GetCompositor();
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease(uint64 surface_id) {
  accelerated_surface_containers_.erase(surface_id);
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
  NativeWebKeyboardEvent webkit_event(event);
  host_->ForwardKeyboardEvent(webkit_event);
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
  host_->ForwardMouseEvent(content::MakeWebMouseEvent(event));

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

#if !defined(TOUCH_UI)
////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  //NOTIMPLEMENTED();
  // TODO(beng): See RenderWidgetHostViewWin.
}
#endif
