// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/display_manager.h"

#include "base/numerics/safe_conversions.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/quads.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display_manager_factory.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/input_events/mojo_extended_key_event_data.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"
#include "mojo/converters/transform/transform_type_converters.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/display.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#elif defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#elif defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#endif

using mojo::Rect;
using mojo::Size;

namespace mus {
namespace {

// DrawWindowTree recursively visits ServerWindows, creating a SurfaceDrawQuad
// for each that lacks one. A ServerWindow may hold a CompositorFrame that
// references other ServerWindows in SurfaceDrawQuads. We should not create new
// SurfaceDrawQuads for these |referenced_window_ids|. Instead,
// cc::SurfaceAggregator will do the heavy lifting here by expanding those
// references to generate one top-level display CompositorFrame.
void DrawWindowTree(cc::RenderPass* pass,
                    ServerWindow* window,
                    const gfx::Vector2d& parent_to_root_origin_offset,
                    float opacity,
                    std::set<WindowId>* referenced_window_ids) {
  if (!window->visible())
    return;

  ServerWindowSurface* surface = window->surface();

  if (surface) {
    // Accumulate referenced windows in each ServerWindow's CompositorFrame.
    referenced_window_ids->insert(surface->referenced_window_ids().begin(),
                                  surface->referenced_window_ids().end());
  }

  const gfx::Rect absolute_bounds =
      window->bounds() + parent_to_root_origin_offset;
  std::vector<ServerWindow*> children(window->GetChildren());
  // TODO(rjkroege, fsamuel): Make sure we're handling alpha correctly.
  const float combined_opacity = opacity * window->opacity();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    DrawWindowTree(pass, *it, absolute_bounds.OffsetFromOrigin(),
                   combined_opacity, referenced_window_ids);
  }

  // If an ancestor has already referenced this window, then we do not need
  // to create a SurfaceDrawQuad for it.
  if (referenced_window_ids->count(window->id()) || !surface)
    return;

  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(absolute_bounds.x(), absolute_bounds.y());
  gfx::Rect bounds_at_origin(window->bounds().size());

  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  // TODO(fsamuel): These clipping and visible rects are incorrect. They need
  // to be populated from CompositorFrame structs.
  sqs->SetAll(
      quad_to_target_transform, bounds_at_origin.size() /* layer_bounds */,
      bounds_at_origin /* visible_layer_bounds */,
      bounds_at_origin /* clip_rect */, false /* is_clipped */,
      window->opacity(), SkXfermode::kSrc_Mode, 0 /* sorting-context_id */);

  auto surface_quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  surface_quad->SetNew(sqs, bounds_at_origin /* rect */,
                       bounds_at_origin /* visible_rect */, surface->id());
}

}  // namespace

// static
DisplayManagerFactory* DisplayManager::factory_ = nullptr;

// static
DisplayManager* DisplayManager::Create(
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state) {
  if (factory_) {
    return factory_->CreateDisplayManager(app_impl, gpu_state,
                                          surfaces_state);
  }
  return new DefaultDisplayManager(app_impl, gpu_state,
                                   surfaces_state);
}

DefaultDisplayManager::DefaultDisplayManager(
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : app_impl_(app_impl),
      gpu_state_(gpu_state),
      surfaces_state_(surfaces_state),
      delegate_(nullptr),
      draw_timer_(false, false),
      frame_pending_(false),
      weak_factory_(this) {
  metrics_.size_in_pixels = mojo::Size::New();
  metrics_.size_in_pixels->width = 1024;
  metrics_.size_in_pixels->height = 768;
}

void DefaultDisplayManager::Init(DisplayManagerDelegate* delegate) {
  delegate_ = delegate;

  gfx::Rect bounds(metrics_.size_in_pixels.To<gfx::Size>());
#if defined(OS_WIN)
  platform_window_.reset(new ui::WinWindow(this, bounds));
#elif defined(USE_X11)
  platform_window_.reset(new ui::X11Window(this));
#elif defined(OS_ANDROID)
  platform_window_.reset(new ui::PlatformWindowAndroid(this));
#else
  NOTREACHED() << "Unsupported platform";
#endif
  platform_window_->SetBounds(bounds);
  platform_window_->Show();
}

DefaultDisplayManager::~DefaultDisplayManager() {
  delegate_->OnTopLevelSurfaceChanged(cc::SurfaceId());
  // Invalidate WeakPtrs now to avoid callbacks back into the
  // DefaultDisplayManager during destruction of |top_level_display_client_|.
  weak_factory_.InvalidateWeakPtrs();
  top_level_display_client_.reset();
  // Destroy the PlatformWindow early on as it may call us back during
  // destruction and we want to be in a known state. But destroy the surface
  // first because it can still be using the platform window.
  platform_window_.reset();
}

void DefaultDisplayManager::SchedulePaint(const ServerWindow* window,
                                          const gfx::Rect& bounds) {
  DCHECK(window);
  if (!window->IsDrawn())
    return;
  const gfx::Rect root_relative_rect =
      ConvertRectBetweenWindows(window, delegate_->GetRootWindow(), bounds);
  if (root_relative_rect.IsEmpty())
    return;
  dirty_rect_.Union(root_relative_rect);
  WantToDraw();
}

void DefaultDisplayManager::SetViewportSize(const gfx::Size& size) {
  platform_window_->SetBounds(gfx::Rect(size));
}

void DefaultDisplayManager::SetTitle(const base::string16& title) {
  platform_window_->SetTitle(title);
}

const mojom::ViewportMetrics& DefaultDisplayManager::GetViewportMetrics() {
  return metrics_;
}

void DefaultDisplayManager::UpdateTextInputState(
    const ui::TextInputState& state) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->UpdateTextInputState(state);
}

void DefaultDisplayManager::SetImeVisibility(bool visible) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->SetImeVisibility(visible);
}

void DefaultDisplayManager::Draw() {
  if (!delegate_->GetRootWindow()->visible())
    return;

  // TODO(fsamuel): We should add a trace for generating a top level frame.
  scoped_ptr<cc::CompositorFrame> frame(GenerateCompositorFrame());
  frame_pending_ = true;
  if (top_level_display_client_) {
    top_level_display_client_->SubmitCompositorFrame(
        frame.Pass(), base::Bind(&DefaultDisplayManager::DidDraw,
                                 weak_factory_.GetWeakPtr()));
  }
  dirty_rect_ = gfx::Rect();
}

void DefaultDisplayManager::DidDraw() {
  frame_pending_ = false;
  if (!dirty_rect_.IsEmpty())
    WantToDraw();
}

void DefaultDisplayManager::WantToDraw() {
  if (draw_timer_.IsRunning() || frame_pending_)
    return;

  draw_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&DefaultDisplayManager::Draw, weak_factory_.GetWeakPtr()));
}

void DefaultDisplayManager::UpdateMetrics(const gfx::Size& size,
                                          float device_pixel_ratio) {
  if (gfx::Display::HasForceDeviceScaleFactor())
    device_pixel_ratio = gfx::Display::GetForcedDeviceScaleFactor();
  if (metrics_.size_in_pixels.To<gfx::Size>() == size &&
      metrics_.device_pixel_ratio == device_pixel_ratio)
    return;
  mojom::ViewportMetrics old_metrics;
  old_metrics.size_in_pixels = metrics_.size_in_pixels.Clone();
  old_metrics.device_pixel_ratio = metrics_.device_pixel_ratio;

  metrics_.size_in_pixels = mojo::Size::From(size);
  metrics_.device_pixel_ratio = device_pixel_ratio;

  delegate_->OnViewportMetricsChanged(old_metrics, metrics_);
}

scoped_ptr<cc::CompositorFrame>
DefaultDisplayManager::GenerateCompositorFrame() {
  scoped_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->damage_rect = dirty_rect_;
  render_pass->output_rect = gfx::Rect(metrics_.size_in_pixels.To<gfx::Size>());

  std::set<WindowId> referenced_window_ids;
  DrawWindowTree(render_pass.get(), delegate_->GetRootWindow(), gfx::Vector2d(),
                 1.0f, &referenced_window_ids);

  scoped_ptr<cc::DelegatedFrameData> frame_data(new cc::DelegatedFrameData);
  frame_data->device_scale_factor = metrics_.device_pixel_ratio;
  frame_data->render_pass_list.push_back(render_pass.Pass());

  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  frame->delegated_frame_data = frame_data.Pass();
  return frame.Pass();
}

void DefaultDisplayManager::OnBoundsChanged(const gfx::Rect& new_bounds) {
  UpdateMetrics(new_bounds.size(), metrics_.device_pixel_ratio);
}

void DefaultDisplayManager::OnDamageRect(const gfx::Rect& damaged_region) {
  dirty_rect_.Union(damaged_region);
  WantToDraw();
}

void DefaultDisplayManager::DispatchEvent(ui::Event* event) {
  mojo::EventPtr mojo_event(mojo::Event::From(*event));
  delegate_->OnEvent(mojo_event.Pass());

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      platform_window_->SetCapture();
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      platform_window_->ReleaseCapture();
      break;
    default:
      break;
  }

#if defined(USE_X11)
  // We want to emulate the WM_CHAR generation behaviour of Windows.
  //
  // On Linux, we've previously inserted characters by having
  // InputMethodAuraLinux take all key down events and send a character event
  // to the TextInputClient. This causes a mismatch in code that has to be
  // shared between Windows and Linux, including blink code. Now that we're
  // trying to have one way of doing things, we need to standardize on and
  // emulate Windows character events.
  //
  // This is equivalent to what we're doing in the current Linux port, but
  // done once instead of done multiple times in different places.
  if (event->type() == ui::ET_KEY_PRESSED) {
    ui::KeyEvent* key_press_event = static_cast<ui::KeyEvent*>(event);
    ui::KeyEvent char_event(key_press_event->GetCharacter(),
                            key_press_event->key_code(),
                            key_press_event->flags());

    DCHECK_EQ(key_press_event->GetCharacter(), char_event.GetCharacter());
    DCHECK_EQ(key_press_event->key_code(), char_event.key_code());
    DCHECK_EQ(key_press_event->flags(), char_event.flags());

    char_event.SetExtendedKeyEventData(
        make_scoped_ptr(new mojo::MojoExtendedKeyEventData(
            key_press_event->GetLocatedWindowsKeyboardCode(),
            key_press_event->GetText(), key_press_event->GetUnmodifiedText())));

    delegate_->OnEvent(mojo::Event::From(char_event));
  }
#endif
}

void DefaultDisplayManager::OnCloseRequest() {
  platform_window_->Close();
}

void DefaultDisplayManager::OnClosed() {
  delegate_->OnDisplayClosed();
}

void DefaultDisplayManager::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {}

void DefaultDisplayManager::OnLostCapture() {}

void DefaultDisplayManager::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  if (widget != gfx::kNullAcceleratedWidget) {
    top_level_display_client_.reset(
        new TopLevelDisplayClient(widget, gpu_state_, surfaces_state_));
    delegate_->OnTopLevelSurfaceChanged(
        top_level_display_client_->surface_id());
  }
  UpdateMetrics(metrics_.size_in_pixels.To<gfx::Size>(), device_pixel_ratio);
}

void DefaultDisplayManager::OnAcceleratedWidgetDestroyed() {
  NOTREACHED();
}

void DefaultDisplayManager::OnActivationChanged(bool active) {}

}  // namespace mus
