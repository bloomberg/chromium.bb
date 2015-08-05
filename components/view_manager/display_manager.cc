// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/display_manager.h"

#include "base/numerics/safe_conversions.h"
#include "components/view_manager/display_manager_factory.h"
#include "components/view_manager/gles2/gpu_state.h"
#include "components/view_manager/native_viewport/onscreen_context_provider.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "components/view_manager/public/interfaces/quads.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/view_coordinate_conversions.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/converters/input_events/mojo_extended_key_event_data.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"
#include "mojo/converters/transform/transform_type_converters.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/stub/stub_window.h"

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#elif defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#elif defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#endif

using mojo::Rect;
using mojo::Size;

namespace view_manager {
namespace {

void DrawViewTree(mojo::Pass* pass,
                  const ServerView* view,
                  const gfx::Vector2d& parent_to_root_origin_offset,
                  float opacity) {
  if (!view->visible())
    return;

  const gfx::Rect absolute_bounds =
      view->bounds() + parent_to_root_origin_offset;
  std::vector<const ServerView*> children(view->GetChildren());
  const float combined_opacity = opacity * view->opacity();
  for (std::vector<const ServerView*>::reverse_iterator it = children.rbegin();
       it != children.rend();
       ++it) {
    DrawViewTree(pass, *it, absolute_bounds.OffsetFromOrigin(),
                 combined_opacity);
  }

  cc::SurfaceId node_id = view->surface_id();

  auto surface_quad_state = mojo::SurfaceQuadState::New();
  surface_quad_state->surface = mojo::SurfaceId::From(node_id);

  gfx::Transform node_transform;
  node_transform.Translate(absolute_bounds.x(), absolute_bounds.y());

  const gfx::Rect bounds_at_origin(view->bounds().size());
  auto surface_quad = mojo::Quad::New();
  surface_quad->material = mojo::Material::MATERIAL_SURFACE_CONTENT;
  surface_quad->rect = Rect::From(bounds_at_origin);
  surface_quad->opaque_rect = Rect::From(bounds_at_origin);
  surface_quad->visible_rect = Rect::From(bounds_at_origin);
  surface_quad->needs_blending = true;
  surface_quad->shared_quad_state_index =
      base::saturated_cast<int32_t>(pass->shared_quad_states.size());
  surface_quad->surface_quad_state = surface_quad_state.Pass();

  auto sqs = mojo::CreateDefaultSQS(view->bounds().size());
  sqs->blend_mode = mojo::SK_XFERMODE_kSrcOver_Mode;
  sqs->opacity = combined_opacity;
  sqs->quad_to_target_transform = mojo::Transform::From(node_transform);

  pass->quads.push_back(surface_quad.Pass());
  pass->shared_quad_states.push_back(sqs.Pass());
}

float ConvertUIWheelValueToMojoValue(int offset) {
  // Mojo's event type takes a value between -1 and 1. Normalize by allowing
  // up to 20 of ui's offset. This is a bit arbitrary.
  return std::max(
      -1.0f, std::min(1.0f, static_cast<float>(offset) /
                                (20 * static_cast<float>(
                                          ui::MouseWheelEvent::kWheelDelta))));
}

}  // namespace

// static
DisplayManagerFactory* DisplayManager::factory_ = nullptr;

// static
DisplayManager* DisplayManager::Create(
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state) {
  if (factory_)
    return factory_->CreateDisplayManager(is_headless, app_impl, gpu_state);
  return new DefaultDisplayManager(is_headless, app_impl, gpu_state);
}

DefaultDisplayManager::DefaultDisplayManager(
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state)
    : is_headless_(is_headless),
      app_impl_(app_impl),
      gpu_state_(gpu_state),
      delegate_(nullptr),
      draw_timer_(false, false),
      frame_pending_(false),
      context_provider_(
          new native_viewport::OnscreenContextProvider(gpu_state)),
      weak_factory_(this) {
  metrics_.size_in_pixels = mojo::Size::New();
  metrics_.size_in_pixels->width = 800;
  metrics_.size_in_pixels->height = 600;
}

void DefaultDisplayManager::Init(DisplayManagerDelegate* delegate) {
  delegate_ = delegate;

  gfx::Rect bounds(metrics_.size_in_pixels.To<gfx::Size>());
  if (is_headless_) {
    platform_window_.reset(new ui::StubWindow(this));
  } else {
#if defined(OS_WIN)
    platform_window_.reset(new ui::WinWindow(this, bounds));
#elif defined(USE_X11)
    platform_window_.reset(new ui::X11Window(this));
#elif defined(OS_ANDROID)
    platform_window_.reset(new ui::PlatformWindowAndroid(this));
#else
    NOTREACHED() << "Unsupported platform";
#endif
  }
  platform_window_->SetBounds(bounds);
  platform_window_->Show();

  mojo::ContextProviderPtr context_provider;
  context_provider_->Bind(GetProxy(&context_provider).Pass());
  mojo::DisplayFactoryPtr display_factory;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:surfaces_service");
  app_impl_->ConnectToService(request.Pass(), &display_factory);
  // TODO(fsamuel): We should indicate to the delegate that this object failed
  // to initialize.
  if (!display_factory)
    return;
  display_factory->Create(context_provider.Pass(),
                          nullptr,  // returner - we never submit resources.
                          GetProxy(&display_));
}

DefaultDisplayManager::~DefaultDisplayManager() {
  // Destroy before |platform_window_| because this will destroy
  // CommandBufferDriver objects that contain child windows. Otherwise if this
  // class destroys its window first, X errors will occur.
  context_provider_.reset();

  // Destroy the PlatformWindow early on as it may call us back during
  // destruction and we want to be in a known state.
  platform_window_.reset();
}

void DefaultDisplayManager::SchedulePaint(const ServerView* view,
                                          const gfx::Rect& bounds) {
  DCHECK(view);
  if (!view->IsDrawn())
    return;
  const gfx::Rect root_relative_rect =
      ConvertRectBetweenViews(view, delegate_->GetRootView(), bounds);
  if (root_relative_rect.IsEmpty())
    return;
  dirty_rect_.Union(root_relative_rect);
  WantToDraw();
}

void DefaultDisplayManager::SetViewportSize(const gfx::Size& size) {
  platform_window_->SetBounds(gfx::Rect(size));
}

const mojo::ViewportMetrics& DefaultDisplayManager::GetViewportMetrics() {
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
  if (!delegate_->GetRootView()->visible())
    return;
  gfx::Rect rect(metrics_.size_in_pixels.To<gfx::Size>());
  auto pass = mojo::CreateDefaultPass(1, rect);
  pass->damage_rect = Rect::From(dirty_rect_);

  DrawViewTree(pass.get(), delegate_->GetRootView(), gfx::Vector2d(), 1.0f);

  auto frame = mojo::Frame::New();
  frame->passes.push_back(pass.Pass());
  frame->resources.resize(0u);
  frame_pending_ = true;
  if (display_) {
    display_->SubmitFrame(frame.Pass(),
                          base::Bind(&DefaultDisplayManager::DidDraw,
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
      base::Bind(&DefaultDisplayManager::Draw, base::Unretained(this)));
}

void DefaultDisplayManager::UpdateMetrics(const gfx::Size& size,
                                          float device_pixel_ratio) {
  if (metrics_.size_in_pixels.To<gfx::Size>() == size &&
      metrics_.device_pixel_ratio == device_pixel_ratio)
    return;
  mojo::ViewportMetrics old_metrics;
  old_metrics.size_in_pixels = metrics_.size_in_pixels.Clone();
  old_metrics.device_pixel_ratio = metrics_.device_pixel_ratio;

  metrics_.size_in_pixels = mojo::Size::From(size);
  metrics_.device_pixel_ratio = device_pixel_ratio;

  delegate_->OnViewportMetricsChanged(old_metrics, metrics_);
}

void DefaultDisplayManager::OnBoundsChanged(const gfx::Rect& new_bounds) {
  UpdateMetrics(new_bounds.size(), metrics_.device_pixel_ratio);
}

void DefaultDisplayManager::OnDamageRect(const gfx::Rect& damaged_region) {
}

void DefaultDisplayManager::DispatchEvent(ui::Event* event) {
  mojo::EventPtr mojo_event(mojo::Event::From(*event));
  if (event->IsMouseWheelEvent()) {
    // Mojo's event type has a different meaning for wheel events. Convert
    // between the two.
    ui::MouseWheelEvent* wheel_event =
        static_cast<ui::MouseWheelEvent*>(event);
    DCHECK(mojo_event->pointer_data);
    mojo_event->pointer_data->horizontal_wheel =
        ConvertUIWheelValueToMojoValue(wheel_event->x_offset());
    mojo_event->pointer_data->horizontal_wheel =
        ConvertUIWheelValueToMojoValue(wheel_event->y_offset());
  }
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
            key_press_event->GetText(),
            key_press_event->GetUnmodifiedText())));
    char_event.set_platform_keycode(key_press_event->platform_keycode());

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
    ui::PlatformWindowState new_state) {
}

void DefaultDisplayManager::OnLostCapture() {
}

void DefaultDisplayManager::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  context_provider_->SetAcceleratedWidget(widget);
  UpdateMetrics(metrics_.size_in_pixels.To<gfx::Size>(), device_pixel_ratio);
}

void DefaultDisplayManager::OnActivationChanged(bool active) {
}

}  // namespace view_manager
