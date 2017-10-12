// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include <algorithm>

#include "ash/frame/custom_frame_view_ash.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/exo/surface.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

DEFINE_LOCAL_UI_CLASS_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr);

// Maximum amount of time to wait for contents that match the display's
// orientation in tablet mode.
// TODO(oshima): Looks like android is generating unnecessary frames.
// Fix it on Android side and reduce the timeout.
constexpr int kOrientationLockTimeoutMs = 2500;

// Maximum amount of time to wait for contents after a change to maximize,
// fullscreen or pinned state.
constexpr int kMaximizedOrFullscreenOrPinnedLockTimeoutMs = 100;

// The accelerator keys used to close ShellSurfaces.
const struct {
  ui::KeyboardCode keycode;
  int modifiers;
} kCloseWindowAccelerators[] = {
    {ui::VKEY_W, ui::EF_CONTROL_DOWN},
    {ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
    {ui::VKEY_F4, ui::EF_ALT_DOWN}};

class CustomFrameView : public views::NonClientFrameView {
 public:
  explicit CustomFrameView(views::Widget* widget) : widget_(widget) {}
  ~CustomFrameView() override {}

  // Overridden from views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    return widget_->client_view()->NonClientHitTest(point);
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  views::Widget* const widget_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameView);
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  CustomWindowTargeter(views::Widget* widget) : widget_(widget) {}
  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = ShellSurface::GetMainSurface(window);
    if (!surface)
      return false;

    gfx::Point local_point = event.location();

    if (window->parent()) {
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);
    }

    int component = widget_->non_client_view()->NonClientHitTest(local_point);
    if (component != HTNOWHERE && component != HTCLIENT)
      return true;

    // If there is an underlay, test against it first as it's bounds may be
    // larger than the surface's bounds.
    aura::Window* shadow_underlay =
        static_cast<ShellSurface*>(
            widget_->widget_delegate()->GetContentsView())
            ->shadow_underlay();
    if (shadow_underlay) {
      gfx::Point local_point_in_shadow_underlay = local_point;
      aura::Window::ConvertPointToTarget(window, shadow_underlay,
                                         &local_point_in_shadow_underlay);
      if (gfx::Rect(shadow_underlay->layer()->size())
              .Contains(local_point_in_shadow_underlay)) {
        return true;
      }
    }

    // Otherwise, fallback to hit test on the surface.
    aura::Window::ConvertPointToTarget(window, surface->window(), &local_point);
    return surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1)));
  }

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    aura::Window* window = static_cast<aura::Window*>(root);
    Surface* surface = ShellSurface::GetMainSurface(window);

    // Send events which wouldn't be handled by the surface, to the shadow
    // underlay.
    aura::Window* shadow_underlay =
        static_cast<ShellSurface*>(
            widget_->widget_delegate()->GetContentsView())
            ->shadow_underlay();
    if (surface && event->IsLocatedEvent() && shadow_underlay) {
      gfx::Point local_point = event->AsLocatedEvent()->location();
      int component = widget_->non_client_view()->NonClientHitTest(local_point);
      if (component == HTNOWHERE) {
        aura::Window::ConvertPointToTarget(window, surface->window(),
                                           &local_point);
        if (!surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1))))
          return shadow_underlay;
      }
    }
    return aura::WindowTargeter::FindTargetForEvent(root, event);
  }

 private:
  views::Widget* const widget_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

// Minimal WindowResizer that unlike DefaultWindowResizer does not handle
// dragging and resizing windows.
class CustomWindowResizer : public ash::WindowResizer {
 public:
  explicit CustomWindowResizer(ash::wm::WindowState* window_state)
      : WindowResizer(window_state) {}

  // Overridden from ash::WindowResizer:
  void Drag(const gfx::Point& location, int event_flags) override {}
  void CompleteDrag() override {}
  void RevertDrag() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowResizer);
};

class ShellSurfaceWidget : public views::Widget {
 public:
  explicit ShellSurfaceWidget(ShellSurface* shell_surface)
      : shell_surface_(shell_surface) {}

  // Overridden from views::Widget
  void Close() override { shell_surface_->Close(); }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Handle only accelerators. Do not call Widget::OnKeyEvent that eats focus
    // management keys (like the tab key) as well.
    if (GetFocusManager()->ProcessAccelerator(ui::Accelerator(*event)))
      event->SetHandled();
  }

 private:
  ShellSurface* const shell_surface_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceWidget);
};

Orientation SizeToOrientation(const gfx::Size& size) {
  DCHECK_NE(size.width(), size.height());
  return size.width() > size.height() ? Orientation::LANDSCAPE
                                      : Orientation::PORTRAIT;
}

}  // namespace

// Surface state associated with each configure request.
struct ShellSurface::Config {
  Config(uint32_t serial,
         const gfx::Vector2d& origin_offset,
         int resize_component,
         std::unique_ptr<ui::CompositorLock> compositor_lock);
  ~Config();

  uint32_t serial;
  gfx::Vector2d origin_offset;
  int resize_component;
  std::unique_ptr<ui::CompositorLock> compositor_lock;
};

// Helper class used to coalesce a number of changes into one "configure"
// callback. Callbacks are suppressed while an instance of this class is
// instantiated and instead called when the instance is destroyed.
// If |force_configure_| is true ShellSurface::Configure() will be called
// even if no changes to shell surface took place during the lifetime of the
// ScopedConfigure instance.
class ShellSurface::ScopedConfigure {
 public:
  ScopedConfigure(ShellSurface* shell_surface, bool force_configure);
  ~ScopedConfigure();

  void set_needs_configure() { needs_configure_ = true; }

 private:
  ShellSurface* const shell_surface_;
  const bool force_configure_;
  bool needs_configure_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedConfigure);
};

// Helper class used to temporarily disable animations. Restores the
// animations disabled property when instance is destroyed.
class ShellSurface::ScopedAnimationsDisabled {
 public:
  explicit ScopedAnimationsDisabled(ShellSurface* shell_surface);
  ~ScopedAnimationsDisabled();

 private:
  ShellSurface* const shell_surface_;
  bool saved_animations_disabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedAnimationsDisabled);
};

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, Config:

ShellSurface::Config::Config(
    uint32_t serial,
    const gfx::Vector2d& origin_offset,
    int resize_component,
    std::unique_ptr<ui::CompositorLock> compositor_lock)
    : serial(serial),
      origin_offset(origin_offset),
      resize_component(resize_component),
      compositor_lock(std::move(compositor_lock)) {}

ShellSurface::Config::~Config() {}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedConfigure:

ShellSurface::ScopedConfigure::ScopedConfigure(ShellSurface* shell_surface,
                                               bool force_configure)
    : shell_surface_(shell_surface), force_configure_(force_configure) {
  // ScopedConfigure instances cannot be nested.
  DCHECK(!shell_surface_->scoped_configure_);
  shell_surface_->scoped_configure_ = this;
}

ShellSurface::ScopedConfigure::~ScopedConfigure() {
  DCHECK_EQ(shell_surface_->scoped_configure_, this);
  shell_surface_->scoped_configure_ = nullptr;
  if (needs_configure_ || force_configure_)
    shell_surface_->Configure();
  // ScopedConfigure instance might have suppressed a widget bounds update.
  if (shell_surface_->widget_) {
    shell_surface_->UpdateWidgetBounds();
    shell_surface_->UpdateShadow();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedAnimationsDisabled:

ShellSurface::ScopedAnimationsDisabled::ScopedAnimationsDisabled(
    ShellSurface* shell_surface)
    : shell_surface_(shell_surface) {
  if (shell_surface_->widget_) {
    aura::Window* window = shell_surface_->widget_->GetNativeWindow();
    saved_animations_disabled_ =
        window->GetProperty(aura::client::kAnimationsDisabledKey);
    window->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }
}

ShellSurface::ScopedAnimationsDisabled::~ScopedAnimationsDisabled() {
  if (shell_surface_->widget_) {
    aura::Window* window = shell_surface_->widget_->GetNativeWindow();
    DCHECK_EQ(window->GetProperty(aura::client::kAnimationsDisabledKey), true);
    window->SetProperty(aura::client::kAnimationsDisabledKey,
                        saved_animations_disabled_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

ShellSurface::ShellSurface(Surface* surface,
                           ShellSurface* parent,
                           BoundsMode bounds_mode,
                           const gfx::Point& origin,
                           bool activatable,
                           bool can_minimize,
                           int container)
    : SurfaceTreeHost("ExoShellSurfaceHost", nullptr),
      widget_(nullptr),
      parent_(parent ? parent->GetWidget()->GetNativeWindow() : nullptr),
      bounds_mode_(bounds_mode),
      primary_display_id_(
          display::Screen::GetScreen()->GetPrimaryDisplay().id()),
      origin_(origin),
      activatable_(activatable),
      can_minimize_(can_minimize),
      container_(container) {
  WMHelper::GetInstance()->AddActivationObserver(this);
  WMHelper::GetInstance()->AddDisplayConfigurationObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  surface->AddSurfaceObserver(this);
  SetRootSurface(surface);
  host_window()->Show();
  set_owned_by_client();
  if (parent_)
    parent_->AddObserver(this);
}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurface(surface,
                   nullptr,
                   BoundsMode::SHELL,
                   gfx::Point(),
                   true,
                   true,
                   ash::kShellWindowId_DefaultContainer) {}

ShellSurface::~ShellSurface() {
  DCHECK(!scoped_configure_);
  if (resizer_)
    EndDrag(false /* revert */);
  // Remove activation observer before hiding widget to prevent it from
  // casuing the configure callback to be called.
  WMHelper::GetInstance()->RemoveActivationObserver(this);
  if (widget_) {
    ash::wm::GetWindowState(widget_->GetNativeWindow())->RemoveObserver(this);
    widget_->GetNativeWindow()->RemoveObserver(this);
    // Remove transient children so they are not automatically destroyed.
    for (auto* child : wm::GetTransientChildren(widget_->GetNativeWindow()))
      wm::RemoveTransientChild(widget_->GetNativeWindow(), child);
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
  }
  WMHelper::GetInstance()->RemoveDisplayConfigurationObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  if (parent_)
    parent_->RemoveObserver(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
}

void ShellSurface::AcknowledgeConfigure(uint32_t serial) {
  TRACE_EVENT1("exo", "ShellSurface::AcknowledgeConfigure", "serial", serial);

  // Apply all configs that are older or equal to |serial|. The result is that
  // the origin of the main surface will move and the resize direction will
  // change to reflect the acknowledgement of configure request with |serial|
  // at the next call to Commit().
  while (!pending_configs_.empty()) {
    std::unique_ptr<Config> config = std::move(pending_configs_.front());
    pending_configs_.pop_front();

    // Add the config offset to the accumulated offset that will be applied when
    // Commit() is called.
    pending_origin_offset_ += config->origin_offset;

    // Set the resize direction that will be applied when Commit() is called.
    pending_resize_component_ = config->resize_component;

    if (config->serial == serial)
      break;
  }

  if (widget_) {
    UpdateWidgetBounds();
    UpdateShadow();
  }
}

void ShellSurface::SetParent(ShellSurface* parent) {
  TRACE_EVENT1("exo", "ShellSurface::SetParent", "parent",
               parent ? base::UTF16ToASCII(parent->title_) : "null");

  if (parent_) {
    parent_->RemoveObserver(this);
    if (widget_)
      wm::RemoveTransientChild(parent_, widget_->GetNativeWindow());
  }
  parent_ = parent ? parent->GetWidget()->GetNativeWindow() : nullptr;
  if (parent_) {
    parent_->AddObserver(this);
    if (widget_)
      wm::AddTransientChild(parent_, widget_->GetNativeWindow());
  }

  // If |parent_| is set effects the ability to maximize the window.
  if (widget_)
    widget_->OnSizeConstraintsChanged();
}

void ShellSurface::Activate() {
  TRACE_EVENT0("exo", "ShellSurface::Activate");

  if (!widget_ || widget_->IsActive())
    return;

  widget_->Activate();
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_MAXIMIZED);

  // Note: This will ask client to configure its surface even if already
  // maximized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Maximize();
}

void ShellSurface::Minimize() {
  TRACE_EVENT0("exo", "ShellSurface::Minimize");

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_MINIMIZED);

  // Note: This will ask client to configure its surface even if already
  // minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Minimize();
}

void ShellSurface::Restore() {
  TRACE_EVENT0("exo", "ShellSurface::Restore");

  if (!widget_)
    return;

  // Note: This will ask client to configure its surface even if not already
  // maximized or minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Restore();
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_FULLSCREEN);

  // Note: This will ask client to configure its surface even if fullscreen
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->SetFullscreen(fullscreen);
}

void ShellSurface::SetPinned(ash::mojom::WindowPinType type) {
  TRACE_EVENT1("exo", "ShellSurface::SetPinned", "type",
               static_cast<int>(type));

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  // Note: This will ask client to configure its surface even if pinned
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->GetNativeWindow()->SetProperty(ash::kWindowPinTypeKey, type);
}

void ShellSurface::SetSystemUiVisibility(bool autohide) {
  TRACE_EVENT1("exo", "ShellSurface::SetSystemUiVisibility", "autohide",
               autohide);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  ash::wm::SetAutoHideShelf(widget_->GetNativeWindow(), autohide);
}

void ShellSurface::SetAlwaysOnTop(bool always_on_top) {
  TRACE_EVENT1("exo", "ShellSurface::SetAlwaysOnTop", "always_on_top",
               always_on_top);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  widget_->GetNativeWindow()->SetProperty(aura::client::kAlwaysOnTopKey,
                                          always_on_top);
}

void ShellSurface::SetTitle(const base::string16& title) {
  TRACE_EVENT1("exo", "ShellSurface::SetTitle", "title",
               base::UTF16ToUTF8(title));

  title_ = title;
  if (widget_)
    widget_->UpdateWindowTitle();
}

void ShellSurface::SetIcon(const gfx::ImageSkia& icon) {
  TRACE_EVENT0("exo", "ShellSurface::SetIcon");

  icon_ = icon;
  if (widget_)
    widget_->UpdateWindowIcon();
}

void ShellSurface::SetSystemModal(bool system_modal) {
  // System modal container is used by clients to implement client side
  // managed system modal dialogs using a single ShellSurface instance.
  // Hit-test region will be non-empty when at least one dialog exists on
  // the client side. Here we detect the transition between no client side
  // dialog and at least one dialog so activatable state is properly
  // updated.
  if (container_ != ash::kShellWindowId_SystemModalContainer) {
    LOG(ERROR)
        << "Only a window in SystemModalContainer can change the modality";
    return;
  }

  if (system_modal == system_modal_)
    return;

  bool non_system_modal_window_was_active = !system_modal_ && widget_ && widget_->IsActive();

  system_modal_ = system_modal;

  if (widget_) {
    UpdateSystemModal();
    // Deactivate to give the focus back to normal windows.
    if (!system_modal_ && !non_system_modal_window_was_active_) {
      widget_->Deactivate();
    }
  }

  non_system_modal_window_was_active_ = non_system_modal_window_was_active;
}

void ShellSurface::UpdateSystemModal() {
  DCHECK(widget_);
  DCHECK_EQ(container_, ash::kShellWindowId_SystemModalContainer);
  widget_->GetNativeWindow()->SetProperty(
      aura::client::kModalKey,
      system_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_NONE);
}

// static
void ShellSurface::SetApplicationId(aura::Window* window,
                                    const std::string& id) {
  TRACE_EVENT1("exo", "ShellSurface::SetApplicationId", "application_id", id);
  window->SetProperty(kApplicationIdKey, new std::string(id));
}

// static
const std::string* ShellSurface::GetApplicationId(aura::Window* window) {
  return window->GetProperty(kApplicationIdKey);
}

void ShellSurface::SetApplicationId(const std::string& application_id) {
  // Store the value in |application_id_| in case the window does not exist yet.
  application_id_ = application_id;
  if (widget_ && widget_->GetNativeWindow())
    SetApplicationId(widget_->GetNativeWindow(), application_id);
}

void ShellSurface::Move() {
  TRACE_EVENT0("exo", "ShellSurface::Move");

  if (!widget_)
    return;

  switch (bounds_mode_) {
    case BoundsMode::SHELL:
    case BoundsMode::CLIENT:
      AttemptToStartDrag(HTCAPTION);
      return;
    case BoundsMode::FIXED:
      return;
  }

  NOTREACHED();
}

void ShellSurface::Resize(int component) {
  TRACE_EVENT1("exo", "ShellSurface::Resize", "component", component);

  if (!widget_)
    return;

  switch (bounds_mode_) {
    case BoundsMode::SHELL:
      AttemptToStartDrag(component);
      return;
    case BoundsMode::CLIENT:
    case BoundsMode::FIXED:
      return;
  }

  NOTREACHED();
}

void ShellSurface::Close() {
  if (!close_callback_.is_null())
    close_callback_.Run();
}

void ShellSurface::SetGeometry(const gfx::Rect& geometry) {
  TRACE_EVENT1("exo", "ShellSurface::SetGeometry", "geometry",
               geometry.ToString());

  if (geometry.IsEmpty()) {
    DLOG(WARNING) << "Surface geometry must be non-empty";
    return;
  }

  pending_geometry_ = geometry;
}

void ShellSurface::SetOrientation(Orientation orientation) {
  TRACE_EVENT1("exo", "ShellSurface::SetOrientation", "orientation",
               orientation == Orientation::PORTRAIT ? "portrait" : "landscape");
  pending_orientation_ = orientation;
}

void ShellSurface::SetShadowBounds(const gfx::Rect& bounds) {
  TRACE_EVENT1("exo", "ShellSurface::SetShadowBounds", "bounds",
               bounds.ToString());
  auto shadow_bounds =
      bounds.IsEmpty() ? base::nullopt : base::make_optional(bounds);
  if (shadow_bounds_ != shadow_bounds) {
    shadow_bounds_ = shadow_bounds;
    shadow_bounds_changed_ = true;
  }
}

void ShellSurface::SetRectangularShadowBackgroundOpacity(float opacity) {
  TRACE_EVENT1("exo", "ShellSurface::SetRectangularShadowBackgroundOpacity",
               "opacity", opacity);
  shadow_background_opacity_ = opacity;
}

void ShellSurface::SetScale(double scale) {
  TRACE_EVENT1("exo", "ShellSurface::SetScale", "scale", scale);

  if (scale <= 0.0) {
    DLOG(WARNING) << "Surface scale must be greater than 0";
    return;
  }

  pending_scale_ = scale;
}

void ShellSurface::SetTopInset(int height) {
  TRACE_EVENT1("exo", "ShellSurface::SetTopInset", "height", height);

  pending_top_inset_height_ = height;
}

void ShellSurface::SetOrigin(const gfx::Point& origin) {
  TRACE_EVENT1("exo", "ShellSurface::SetOrigin", "origin", origin.ToString());

  origin_ = origin;
}

void ShellSurface::SetActivatable(bool activatable) {
  TRACE_EVENT1("exo", "ShellSurface::SetActivatable", "activatable",
               activatable);

  activatable_ = activatable;
}

void ShellSurface::SetContainer(int container) {
  TRACE_EVENT1("exo", "ShellSurface::SetContainer", "container", container);

  container_ = container;
}

// static
void ShellSurface::SetMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

// static
Surface* ShellSurface::GetMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

std::unique_ptr<base::trace_event::TracedValue> ShellSurface::AsTracedValue()
    const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("title", base::UTF16ToUTF8(title_));
  if (GetWidget() && GetWidget()->GetNativeWindow()) {
    const std::string* application_id =
        GetApplicationId(GetWidget()->GetNativeWindow());

    if (application_id)
      value->SetString("application_id", *application_id);
  }
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSurfaceCommit() {
  // When the shadow underlay is in surface coordinate space and the surface's
  // bounds have changed, shadow API requires that we synchronize the shadow
  // bounds change with the next frame, so we have to submit the next frame to a
  // new surface, and let the host_window() use the new surface.
  if (shadow_bounds_changed_)
    host_window()->AllocateLocalSurfaceId();

  SurfaceTreeHost::OnSurfaceCommit();

  if (enabled() && !widget_) {
    // Defer widget creation until surface contains some contents.
    if (host_window()->bounds().IsEmpty()) {
      Configure();
      return;
    }

    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);
  }

  SubmitCompositorFrame();

  // Apply the accumulated pending origin offset to reflect acknowledged
  // configure requests.
  origin_offset_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();

  // Update resize direction to reflect acknowledged configure requests.
  resize_component_ = pending_resize_component_;

  if (widget_) {
    // Apply new window geometry.
    geometry_ = pending_geometry_;

    UpdateWidgetBounds();
    UpdateShadow();

    // Apply new top inset height.
    if (pending_top_inset_height_ != top_inset_height_) {
      widget_->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                              pending_top_inset_height_);
      top_inset_height_ = pending_top_inset_height_;
    }

    // System modal container is used by clients to implement overlay
    // windows using a single ShellSurface instance.  If hit-test
    // region is empty, then it is non interactive window and won't be
    // activated.
    if (container_ == ash::kShellWindowId_SystemModalContainer) {
      gfx::Rect hit_test_bounds = GetHitTestBounds();

      // Prevent window from being activated when hit test bounds are empty.
      bool activatable = activatable_ && !hit_test_bounds.IsEmpty();
      if (activatable != CanActivate()) {
        set_can_activate(activatable);
        // Activate or deactivate window if activation state changed.
        if (activatable) {
          // Automatically activate only if the window is modal.
          // Non modal window should be activated by a user action.
          // TODO(oshima): Non modal system window does not have an associated
          // task ID, and as a result, it cannot be activated from client side.
          // Fix this (b/65460424) and remove this if condition.
          if (system_modal_)
            wm::ActivateWindow(widget_->GetNativeWindow());
        } else if (widget_->IsActive()) {
          wm::DeactivateWindow(widget_->GetNativeWindow());
        }
      }
    }

    UpdateSurfaceBounds();

    // Update surface scale.
    if (pending_scale_ != scale_) {
      gfx::Transform transform;
      DCHECK_NE(pending_scale_, 0.0);
      transform.Scale(1.0 / pending_scale_, 1.0 / pending_scale_);
      host_window()->SetTransform(transform);
      scale_ = pending_scale_;
    }

    // Show widget if needed.
    if (pending_show_widget_) {
      DCHECK(!widget_->IsClosed());
      DCHECK(!widget_->IsVisible());
      pending_show_widget_ = false;
      widget_->Show();
      if (container_ == ash::kShellWindowId_SystemModalContainer)
        UpdateSystemModal();
    }
    orientation_ = pending_orientation_;
    if (expected_orientation_ == orientation_)
      orientation_compositor_lock_.reset();
  } else {
    orientation_compositor_lock_.reset();
  }
}

void ShellSurface::OnSetFrame(SurfaceFrameType type) {
  // TODO(reveman): Allow frame to change after surface has been enabled.
  switch (type) {
    case SurfaceFrameType::NONE:
      frame_enabled_ = false;
      shadow_bounds_.reset();
      break;
    case SurfaceFrameType::NORMAL:
      frame_enabled_ = true;
      shadow_bounds_ = gfx::Rect();
      break;
    case SurfaceFrameType::SHADOW:
      frame_enabled_ = false;
      shadow_bounds_ = gfx::Rect();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void ShellSurface::OnSurfaceDestroying(Surface* surface) {
  DCHECK_EQ(root_surface(), surface);
  surface->RemoveSurfaceObserver(this);
  SetRootSurface(nullptr);

  if (resizer_)
    EndDrag(false /* revert */);
  if (widget_)
    SetMainSurface(widget_->GetNativeWindow(), nullptr);

  // Hide widget before surface is destroyed. This allows hide animations to
  // run using the current surface contents.
  if (widget_) {
    // Remove transient children so they are not automatically hidden.
    for (auto* child : wm::GetTransientChildren(widget_->GetNativeWindow()))
      wm::RemoveTransientChild(widget_->GetNativeWindow(), child);
    widget_->Hide();
  }

  // Note: In its use in the Wayland server implementation, the surface
  // destroyed callback may destroy the ShellSurface instance. This call needs
  // to be last so that the instance can be destroyed.
  if (!surface_destroyed_callback_.is_null())
    surface_destroyed_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

bool ShellSurface::CanResize() const {
  return bounds_mode_ == BoundsMode::SHELL;
}

bool ShellSurface::CanMaximize() const {
  // Shell surfaces in system modal container cannot be maximized.
  if (container_ == ash::kShellWindowId_SystemModalContainer)
    return false;

  // Non-transient shell surfaces can be maximized.
  return !parent_;
}

bool ShellSurface::CanMinimize() const {
  return can_minimize_;
}

base::string16 ShellSurface::GetWindowTitle() const {
  return title_;
}

gfx::ImageSkia ShellSurface::GetWindowIcon() {
  return icon_;
}

void ShellSurface::SaveWindowPlacement(const gfx::Rect& bounds,
                                       ui::WindowShowState show_state) {
  if (bounds_mode_ != BoundsMode::CLIENT)
    WidgetDelegate::SaveWindowPlacement(bounds, show_state);
}

bool ShellSurface::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  if (bounds_mode_ != BoundsMode::CLIENT)
    return WidgetDelegate::GetSavedWindowPlacement(widget, bounds, show_state);
  return false;
}

void ShellSurface::WindowClosing() {
  if (resizer_)
    EndDrag(true /* revert */);
  SetEnabled(false);
  widget_ = nullptr;
}

views::Widget* ShellSurface::GetWidget() {
  return widget_;
}

const views::Widget* ShellSurface::GetWidget() const {
  return widget_;
}

views::View* ShellSurface::GetContentsView() {
  return this;
}

views::NonClientFrameView* ShellSurface::CreateNonClientFrameView(
    views::Widget* widget) {
  aura::Window* window = widget_->GetNativeWindow();
  // ShellSurfaces always use immersive mode.
  window->SetProperty(aura::client::kImmersiveFullscreenKey, true);
  if (frame_enabled_)
    return new ash::CustomFrameViewAsh(widget);

  return new CustomFrameView(widget);
}

bool ShellSurface::WidgetHasHitTestMask() const {
  return HasHitTestMask();
}

void ShellSurface::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(WidgetHasHitTestMask());
  GetHitTestMask(mask);

  gfx::Point origin = host_window()->bounds().origin();
  SkMatrix matrix;
  matrix.setScaleTranslate(
      SkFloatToScalar(1.0f / scale_), SkFloatToScalar(1.0f / scale_),
      SkIntToScalar(origin.x()), SkIntToScalar(origin.y()));
  mask->transform(matrix);
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size ShellSurface::CalculatePreferredSize() const {
  if (!geometry_.IsEmpty())
    return geometry_.size();

  return host_window()->bounds().size();
}

gfx::Size ShellSurface::GetMinimumSize() const {
  return gfx::Size(1, 1);
}

////////////////////////////////////////////////////////////////////////////////
// ash::wm::WindowStateObserver overrides:

void ShellSurface::OnPreWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType old_type) {
  ash::mojom::WindowStateType new_type = window_state->GetStateType();
  if (old_type == ash::mojom::WindowStateType::MINIMIZED ||
      new_type == ash::mojom::WindowStateType::MINIMIZED) {
    return;
  }

  if (ash::IsMaximizedOrFullscreenOrPinnedWindowStateType(old_type) ||
      ash::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
    // When transitioning in/out of maximized or fullscreen mode we need to
    // make sure we have a configure callback before we allow the default
    // cross-fade animations. The configure callback provides a mechanism for
    // the client to inform us that a frame has taken the state change into
    // account and without this cross-fade animations are unreliable.
    // TODO(domlaskowski): For BoundsMode::CLIENT, the configure callback does
    // not yet support window state changes. See crbug.com/699746.
    if (configure_callback_.is_null() || bounds_mode_ == BoundsMode::CLIENT) {
      scoped_animations_disabled_.reset(new ScopedAnimationsDisabled(this));
    } else if (widget_) {
      // Give client a chance to produce a frame that takes state change into
      // account by acquiring a compositor lock.
      ui::Compositor* compositor =
          widget_->GetNativeWindow()->layer()->GetCompositor();
      configure_compositor_lock_ = compositor->GetCompositorLock(
          nullptr, base::TimeDelta::FromMilliseconds(
                       kMaximizedOrFullscreenOrPinnedLockTimeoutMs));
    }
  }
}

void ShellSurface::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType old_type) {
  ash::mojom::WindowStateType new_type = window_state->GetStateType();
  if (ash::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
    Configure();
  }

  if (widget_) {
    UpdateWidgetBounds();
    UpdateShadow();
    UpdateBackdrop();
  }

  if (old_type != new_type && !state_changed_callback_.is_null())
    state_changed_callback_.Run(old_type, new_type);

  // Re-enable animations if they were disabled in pre state change handler.
  scoped_animations_disabled_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurface::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (window == host_window())
    return;

  // TODO(domlaskowski): For BoundsMode::CLIENT, the configure callback does not
  // yet support resizing. See crbug.com/699746.
  if (bounds_mode_ == BoundsMode::CLIENT)
    return;

  if (!widget_ || !root_surface() || ignore_window_bounds_changes_)
    return;

  if (window == widget_->GetNativeWindow()) {
    if (new_bounds.size() == old_bounds.size())
      return;

    // If size changed then give the client a chance to produce new contents
    // before origin on screen is changed. Retain the old origin by reverting
    // the origin delta until the next configure is acknowledged.
    gfx::Vector2d delta = new_bounds.origin() - old_bounds.origin();
    origin_offset_ -= delta;
    pending_origin_offset_accumulator_ += delta;

    UpdateSurfaceBounds();

    // The shadow size may be updated to match the widget. Change it back
    // to the shadow content size.
    // TODO(oshima): When the window reiszing is enabled, we may want to
    // implement shadow management here instead of using shadow controller.
    UpdateShadow();

    Configure();
  }
}

void ShellSurface::OnWindowAddedToRootWindow(aura::Window* window) {
  if (window == host_window())
    SurfaceTreeHost::OnWindowAddedToRootWindow(window);
}

void ShellSurface::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                  aura::Window* new_root) {
  if (window == host_window())
    SurfaceTreeHost::OnWindowRemovingFromRootWindow(window, new_root);
}

void ShellSurface::OnWindowDestroying(aura::Window* window) {
  if (window == host_window()) {
    SurfaceTreeHost::OnWindowDestroying(window);
    return;
  }

  if (window == parent_) {
    parent_ = nullptr;
    // |parent_| being set to null effects the ability to maximize the window.
    if (widget_)
      widget_->OnSizeConstraintsChanged();
  }
  window->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WMHelper::ActivationObserver overrides:

void ShellSurface::OnWindowActivated(
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!widget_)
    return;

  if (gained_active == widget_->GetNativeWindow() ||
      lost_active == widget_->GetNativeWindow()) {
    DCHECK(activatable_);
    Configure();
    UpdateShadow();
  }
}

////////////////////////////////////////////////////////////////////////////////
// WMHelper::DisplayConfigurationObserver overrides:

void ShellSurface::OnDisplayConfigurationChanged() {
  if (bounds_mode_ != BoundsMode::CLIENT)
    return;

  const display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_display_id = screen->GetPrimaryDisplay().id();
  if (primary_display_id == primary_display_id_)
    return;

  display::Display old_primary_display;
  if (screen->GetDisplayWithDisplayId(primary_display_id_,
                                      &old_primary_display)) {
    // Give the client a chance to adjust window positions before switching to
    // the new coordinate system. Retain the old origin by reverting the origin
    // delta until the next configure is acknowledged.
    gfx::Vector2d delta = gfx::Point() - old_primary_display.bounds().origin();
    origin_offset_ -= delta;
    pending_origin_offset_accumulator_ += delta;

    if (widget_) {
      UpdateWidgetBounds();
      UpdateShadow();
    }

    Configure();
  }

  primary_display_id_ = primary_display_id;
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void ShellSurface::OnKeyEvent(ui::KeyEvent* event) {
  if (!resizer_) {
    views::View::OnKeyEvent(event);
    return;
  }

  // TODO(domlaskowski): For BoundsMode::CLIENT, synchronize the revert with the
  // client, instead of having the client destroy the window on VKEY_ESCAPE. See
  // crbug.com/699746.
  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    EndDrag(true /* revert */);
  }
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver overrides:

void ShellSurface::OnDisplayMetricsChanged(const display::Display& new_display,
                                           uint32_t changed_metrics) {
  if (!widget_ || !widget_->IsActive() || bounds_mode_ != BoundsMode::CLIENT ||
      !WMHelper::GetInstance()->IsTabletModeWindowManagerEnabled()) {
    return;
  }

  const display::Screen* screen = display::Screen::GetScreen();
  display::Display current_display =
      screen->GetDisplayNearestWindow(widget_->GetNativeWindow());
  if (current_display.id() != new_display.id() ||
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  Orientation target_orientation = SizeToOrientation(new_display.size());
  if (orientation_ == target_orientation)
    return;
  expected_orientation_ = target_orientation;
  EnsureCompositorIsLockedForOrientationChange();
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void ShellSurface::OnMouseEvent(ui::MouseEvent* event) {
  if (!resizer_) {
    views::View::OnMouseEvent(event);
    return;
  }

  if (event->handled())
    return;

  if ((event->flags() &
       (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED) {
    // We complete the drag instead of reverting it, as reverting it will
    // result in a weird behavior when a client produces a modal dialog
    // while the drag is in progress.
    EndDrag(false /* revert */);
    return;
  }

  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED: {
      if (bounds_mode_ == BoundsMode::CLIENT)
        break;

      gfx::Point location(event->location());
      aura::Window::ConvertPointToTarget(widget_->GetNativeWindow(),
                                         widget_->GetNativeWindow()->parent(),
                                         &location);
      ScopedConfigure scoped_configure(this, false);
      resizer_->Drag(location, event->flags());
      event->StopPropagation();
      break;
    }
    case ui::ET_MOUSE_RELEASED: {
      ScopedConfigure scoped_configure(this, false);
      EndDrag(false /* revert */);
      break;
    }
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ShellSurface::OnGestureEvent(ui::GestureEvent* event) {
  if (!resizer_) {
    views::View::OnGestureEvent(event);
    return;
  }

  if (event->handled())
    return;

  // TODO(domlaskowski): Handle touch dragging/resizing for BoundsMode::SHELL.
  // See crbug.com/738606.
  switch (event->type()) {
    case ui::ET_GESTURE_END: {
      ScopedConfigure scoped_configure(this, false);
      EndDrag(false /* revert */);
      break;
    }
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_SCROLL_UPDATE:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_TAP_UNCONFIRMED:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_PINCH_END:
    case ui::ET_GESTURE_PINCH_UPDATE:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_SWIPE:
    case ui::ET_GESTURE_SHOW_PRESS:
    case ui::ET_SCROLL:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_SCROLL_FLING_CANCEL:
      break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::AcceleratorTarget overrides:

bool ShellSurface::AcceleratorPressed(const ui::Accelerator& accelerator) {
  for (const auto& entry : kCloseWindowAccelerators) {
    if (ui::Accelerator(entry.keycode, entry.modifiers) == accelerator) {
      if (!close_callback_.is_null())
        close_callback_.Run();
      return true;
    }
  }
  return views::View::AcceleratorPressed(accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ui::CompositorLockClient overrides:

void ShellSurface::CompositorLockTimedOut() {
  orientation_compositor_lock_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, private:

void ShellSurface::CreateShellSurfaceWidget(ui::WindowShowState show_state) {
  DCHECK(enabled());
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.show_state = show_state;
  // Make shell surface a transient child if |parent_| has been set.
  params.parent =
      parent_ ? parent_
              : WMHelper::GetInstance()->GetPrimaryDisplayContainer(container_);
  params.bounds = gfx::Rect(origin_, gfx::Size());
  bool activatable = activatable_;
  // ShellSurfaces in system modal container are only activatable if input
  // region is non-empty. See OnCommitSurface() for more details.
  if (container_ == ash::kShellWindowId_SystemModalContainer)
    activatable &= !GetHitTestBounds().IsEmpty();
  params.activatable = activatable ? views::Widget::InitParams::ACTIVATABLE_YES
                                   : views::Widget::InitParams::ACTIVATABLE_NO;

  // Note: NativeWidget owns this widget.
  widget_ = new ShellSurfaceWidget(this);
  widget_->Init(params);

  aura::Window* window = widget_->GetNativeWindow();
  window->SetName("ExoShellSurface");
  window->SetProperty(aura::client::kAccessibilityFocusFallsbackToWidgetKey,
                      false);
  window->AddChild(host_window());
  // The window of widget_ is a container window. It doesn't handle pointer
  // events.
  window->SetEventTargetingPolicy(
      ui::mojom::EventTargetingPolicy::DESCENDANTS_ONLY);
  window->SetEventTargeter(base::WrapUnique(new CustomWindowTargeter(widget_)));
  SetApplicationId(window, application_id_);
  SetMainSurface(window, root_surface());

  // Start tracking changes to window bounds and window state.
  window->AddObserver(this);
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
  window_state->AddObserver(this);

  // Allow the client to request bounds that do not fill the entire work area
  // when maximized, or the entire display when fullscreen.
  window_state->set_allow_set_bounds_direct(bounds_mode_ == BoundsMode::CLIENT);

  // Notify client of initial state if different than normal.
  if (window_state->GetStateType() != ash::mojom::WindowStateType::NORMAL &&
      !state_changed_callback_.is_null()) {
    state_changed_callback_.Run(ash::mojom::WindowStateType::NORMAL,
                                window_state->GetStateType());
  }

  // Disable movement if bounds are controlled by the client or fixed.
  bool movement_disabled = bounds_mode_ != BoundsMode::SHELL;
  widget_->set_movement_disabled(movement_disabled);
  window_state->set_ignore_keyboard_bounds_change(movement_disabled);

  // AutoHide shelf in fullscreen state.
  window_state->set_hide_shelf_when_fullscreen(false);

  // Fade visibility animations for non-activatable windows.
  if (!activatable_) {
    wm::SetWindowVisibilityAnimationType(
        window, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  }

  // Register close window accelerators.
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  for (const auto& entry : kCloseWindowAccelerators) {
    focus_manager->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers),
        ui::AcceleratorManager::kNormalPriority, this);
  }

  // Show widget next time Commit() is called.
  pending_show_widget_ = true;
}

void ShellSurface::Configure() {
  // Delay configure callback if |scoped_configure_| is set.
  if (scoped_configure_) {
    scoped_configure_->set_needs_configure();
    return;
  }

  gfx::Vector2d origin_offset = pending_origin_offset_accumulator_;
  pending_origin_offset_accumulator_ = gfx::Vector2d();

  int resize_component = HTCAPTION;
  if (widget_) {
    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(widget_->GetNativeWindow());

    // If surface is being resized, save the resize direction.
    if (window_state->is_dragged())
      resize_component = window_state->drag_details()->window_component;
  }

  uint32_t serial = 0;
  if (!configure_callback_.is_null()) {
    if (widget_) {
      const views::NonClientView* non_client_view = widget_->non_client_view();
      serial = configure_callback_.Run(
          non_client_view->frame_view()->GetBoundsForClientView().size(),
          ash::wm::GetWindowState(widget_->GetNativeWindow())->GetStateType(),
          IsResizing(), widget_->IsActive(), origin_offset);
    } else {
      serial = configure_callback_.Run(gfx::Size(),
                                       ash::mojom::WindowStateType::NORMAL,
                                       false, false, origin_offset);
    }
  }

  if (!serial) {
    pending_origin_offset_ += origin_offset;
    pending_resize_component_ = resize_component;
    return;
  }

  // Apply origin offset and resize component at the first Commit() after this
  // configure request has been acknowledged.
  pending_configs_.push_back(
      std::make_unique<Config>(serial, origin_offset, resize_component,
                               std::move(configure_compositor_lock_)));
  LOG_IF(WARNING, pending_configs_.size() > 100)
      << "Number of pending configure acks for shell surface has reached: "
      << pending_configs_.size();
}

aura::Window* ShellSurface::GetDragWindow() {
  switch (bounds_mode_) {
    case BoundsMode::SHELL:
      return widget_->GetNativeWindow();

    case BoundsMode::CLIENT:
      return root_surface() ? root_surface()->window() : nullptr;

    case BoundsMode::FIXED:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

void ShellSurface::AttemptToStartDrag(int component) {
  DCHECK(widget_);

  // Cannot start another drag if one is already taking place.
  if (resizer_)
    return;

  aura::Window* window = GetDragWindow();
  if (!window || window->HasCapture())
    return;

  if (bounds_mode_ == BoundsMode::SHELL) {
    // Set the cursor before calling CreateWindowResizer(), as that will
    // eventually call LockCursor() and prevent the cursor from changing.
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window->GetRootWindow());
    if (!cursor_client)
      return;

    switch (component) {
      case HTCAPTION:
        cursor_client->SetCursor(ui::CursorType::kPointer);
        break;
      case HTTOP:
        cursor_client->SetCursor(ui::CursorType::kNorthResize);
        break;
      case HTTOPRIGHT:
        cursor_client->SetCursor(ui::CursorType::kNorthEastResize);
        break;
      case HTRIGHT:
        cursor_client->SetCursor(ui::CursorType::kEastResize);
        break;
      case HTBOTTOMRIGHT:
        cursor_client->SetCursor(ui::CursorType::kSouthEastResize);
        break;
      case HTBOTTOM:
        cursor_client->SetCursor(ui::CursorType::kSouthResize);
        break;
      case HTBOTTOMLEFT:
        cursor_client->SetCursor(ui::CursorType::kSouthWestResize);
        break;
      case HTLEFT:
        cursor_client->SetCursor(ui::CursorType::kWestResize);
        break;
      case HTTOPLEFT:
        cursor_client->SetCursor(ui::CursorType::kNorthWestResize);
        break;
      default:
        NOTREACHED();
        break;
    }

    resizer_ = ash::CreateWindowResizer(window, GetMouseLocation(), component,
                                        wm::WINDOW_MOVE_SOURCE_MOUSE);
    if (!resizer_)
      return;

    // Apply pending origin offsets and resize direction before starting a
    // new resize operation. These can still be pending if the client has
    // acknowledged the configure request but not yet called Commit().
    origin_offset_ += pending_origin_offset_;
    pending_origin_offset_ = gfx::Vector2d();
    resize_component_ = pending_resize_component_;
  } else {
    DCHECK(bounds_mode_ == BoundsMode::CLIENT);

    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(widget_->GetNativeWindow());
    DCHECK(!window_state->drag_details());
    DCHECK(component == HTCAPTION);
    window_state->CreateDragDetails(GetMouseLocation(), component,
                                    wm::WINDOW_MOVE_SOURCE_MOUSE);

    // Chained with a CustomWindowResizer, DragWindowResizer does not handle
    // dragging. It only renders phantom windows and moves the window to the
    // target root window when dragging ends.
    resizer_.reset(ash::DragWindowResizer::Create(
        new CustomWindowResizer(window_state), window_state));
  }

  WMHelper::GetInstance()->AddPreTargetHandler(this);
  window->SetCapture();

  // Notify client that resizing state has changed.
  if (IsResizing())
    Configure();
}

void ShellSurface::EndDrag(bool revert) {
  DCHECK(widget_);
  DCHECK(resizer_);

  aura::Window* window = GetDragWindow();
  DCHECK(window);
  DCHECK(window->HasCapture());

  bool was_resizing = IsResizing();

  if (revert)
    resizer_->RevertDrag();
  else
    resizer_->CompleteDrag();

  WMHelper::GetInstance()->RemovePreTargetHandler(this);
  window->ReleaseCapture();
  resizer_.reset();

  // Notify client that resizing state has changed.
  if (was_resizing)
    Configure();

  UpdateWidgetBounds();
}

bool ShellSurface::IsResizing() const {
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(widget_->GetNativeWindow());
  if (!window_state->is_dragged())
    return false;

  return window_state->drag_details()->bounds_change &
         ash::WindowResizer::kBoundsChange_Resizes;
}

gfx::Rect ShellSurface::GetVisibleBounds() const {
  // Use |geometry_| if set, otherwise use the visual bounds of the surface.
  if (!geometry_.IsEmpty())
    return geometry_;

  return root_surface() ? gfx::Rect(root_surface()->content_size())
                        : gfx::Rect();
}

gfx::Point ShellSurface::GetSurfaceOrigin() const {
  DCHECK(bounds_mode_ == BoundsMode::SHELL || resize_component_ == HTCAPTION);

  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect client_bounds =
      widget_->non_client_view()->frame_view()->GetBoundsForClientView();
  switch (resize_component_) {
    case HTCAPTION:
      if (bounds_mode_ == BoundsMode::CLIENT)
        return origin_ + origin_offset_ - visible_bounds.OffsetFromOrigin();

      return gfx::Point() + origin_offset_ - visible_bounds.OffsetFromOrigin();
    case HTBOTTOM:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
      return gfx::Point() - visible_bounds.OffsetFromOrigin();
    case HTTOP:
    case HTTOPRIGHT:
      return gfx::Point(0, client_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
    case HTLEFT:
    case HTBOTTOMLEFT:
      return gfx::Point(client_bounds.width() - visible_bounds.width(), 0) -
             visible_bounds.OffsetFromOrigin();
    case HTTOPLEFT:
      return gfx::Point(client_bounds.width() - visible_bounds.width(),
                        client_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
    default:
      NOTREACHED();
      return gfx::Point();
  }
}

void ShellSurface::UpdateWidgetBounds() {
  DCHECK(widget_);

  // Return early if the shell is currently managing the bounds of the widget.
  // 1) When a window is either maximized/fullscreen/pinned, and the bounds
  // are not controlled by a client.
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(widget_->GetNativeWindow());
  if (window_state->IsMaximizedOrFullscreenOrPinned() &&
      !window_state->allow_set_bounds_direct()) {
    return;
  }

  // 2) When a window is being dragged.
  if (IsResizing())
    return;

  // Return early if there is pending configure requests.
  if (!pending_configs_.empty() || scoped_configure_)
    return;

  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect new_widget_bounds =
      widget_->non_client_view()->GetWindowBoundsForClientBounds(
          visible_bounds);

  switch (bounds_mode_) {
    case BoundsMode::CLIENT:
    case BoundsMode::FIXED:
      new_widget_bounds.set_origin(origin_ -
                                   GetSurfaceOrigin().OffsetFromOrigin());
      break;
    case BoundsMode::SHELL:
      // Update widget origin using the surface origin if the current location
      // of surface is being anchored to one side of the widget as a result of a
      // resize operation.
      if (resize_component_ != HTCAPTION) {
        gfx::Point widget_origin =
            GetSurfaceOrigin() + visible_bounds.OffsetFromOrigin();
        wm::ConvertPointToScreen(widget_->GetNativeWindow(), &widget_origin);
        new_widget_bounds.set_origin(widget_origin);
      } else {
        // Preserve widget position.
        new_widget_bounds.set_origin(
            widget_->GetWindowBoundsInScreen().origin());
      }
      break;
  }

  // Set |ignore_window_bounds_changes_| as this change to window bounds
  // should not result in a configure request.
  DCHECK(!ignore_window_bounds_changes_);
  ignore_window_bounds_changes_ = true;
  const gfx::Rect widget_bounds = widget_->GetWindowBoundsInScreen();
  if (widget_bounds != new_widget_bounds) {
    if (bounds_mode_ != BoundsMode::CLIENT || !resizer_) {
      widget_->SetBounds(new_widget_bounds);
      UpdateSurfaceBounds();
    } else {
      // TODO(domlaskowski): Synchronize window state transitions with the
      // client, and abort client-side dragging on transition to fullscreen. See
      // crbug.com/699746.
      DLOG_IF(ERROR, widget_bounds.size() != new_widget_bounds.size())
          << "Window size changed during client-driven drag";

      // Convert from screen to display coordinates.
      gfx::Point origin = new_widget_bounds.origin();
      wm::ConvertPointFromScreen(widget_->GetNativeWindow()->parent(), &origin);
      new_widget_bounds.set_origin(origin);

      // Move the window relative to the current display.
      widget_->GetNativeWindow()->SetBounds(new_widget_bounds);
      UpdateSurfaceBounds();

      // Render phantom windows when beyond the current display.
      resizer_->Drag(GetMouseLocation(), 0);
    }
  }

  ignore_window_bounds_changes_ = false;
}

void ShellSurface::UpdateSurfaceBounds() {
  gfx::Rect client_view_bounds =
      widget_->non_client_view()->frame_view()->GetBoundsForClientView();

  host_window()->SetBounds(
      gfx::Rect(GetSurfaceOrigin() + client_view_bounds.OffsetFromOrigin(),
                host_window()->bounds().size()));
}

void ShellSurface::UpdateShadow() {
  if (!widget_ || !root_surface())
    return;

  shadow_bounds_changed_ = false;

  UpdateBackdrop();

  aura::Window* window = widget_->GetNativeWindow();

  if (!shadow_bounds_) {
    wm::SetShadowElevation(window, wm::ShadowElevation::NONE);
    shadow_underlay_.reset();
  } else {
    wm::SetShadowElevation(window, wm::ShadowElevation::DEFAULT);

    gfx::Rect shadow_bounds;
    if (shadow_bounds_->IsEmpty()) {
      shadow_bounds = gfx::Rect(window->bounds().size());
    } else {
      shadow_bounds = gfx::ScaleToEnclosedRect(*shadow_bounds_, 1.f / scale_);

      // Convert from screen to display coordinates.
      shadow_bounds -= origin_offset_;
      wm::ConvertRectFromScreen(window->parent(), &shadow_bounds);

      // Convert from display to window coordinates.
      shadow_bounds -= window->bounds().OffsetFromOrigin();
    }

    bool needs_shadow_underlay = shadow_background_opacity_ > 0.f;
    if (needs_shadow_underlay) {
      if (!shadow_underlay_) {
        shadow_underlay_ = std::make_unique<aura::Window>(nullptr);
        shadow_underlay_->set_owned_by_parent(false);
        DCHECK(!shadow_underlay_->owned_by_parent());
        // Ensure the background area inside the shadow is solid black.
        // Clients that provide translucent contents should not be using
        // rectangular shadows as this method requires opaque contents to
        // cast a shadow that represent it correctly.
        shadow_underlay_->Init(ui::LAYER_SOLID_COLOR);
        shadow_underlay_->layer()->SetColor(SK_ColorBLACK);
        DCHECK(shadow_underlay_->layer()->fills_bounds_opaquely());
        window->AddChild(shadow_underlay());
        window->StackChildAtBottom(shadow_underlay());
      }
      gfx::Rect shadow_underlay_bounds(shadow_bounds);
      // Constrain the underlay bounds to the client area in case shell surface
      // frame is enabled.
      if (frame_enabled_) {
        shadow_underlay_bounds.Intersect(
            widget_->non_client_view()->frame_view()->GetBoundsForClientView());
      }
      shadow_underlay_->SetBounds(shadow_underlay_bounds);
      if (!shadow_underlay_->IsVisible())
        shadow_underlay_->Show();
      // TODO(oshima): Setting to the same value should be no-op.
      // crbug.com/642223.
      if (shadow_background_opacity_ !=
          shadow_underlay_->layer()->GetTargetOpacity()) {
        shadow_underlay_->layer()->SetOpacity(shadow_background_opacity_);
      }
    } else {
      shadow_underlay_.reset();
    }

    wm::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);
    // Maximized/Fullscreen window does not create a shadow.
    if (!shadow)
      return;

    shadow->SetContentBounds(shadow_bounds);
    // Surfaces that can't be activated are usually menus and tooltips. Use a
    // small style shadow for them.
    if (!activatable_)
      shadow->SetElevation(wm::ShadowElevation::SMALL);
    // We don't have rounded corners unless frame is enabled.
    if (!frame_enabled_)
      shadow->SetRoundedCornerRadius(0);
  }
}

void ShellSurface::UpdateBackdrop() {
  aura::Window* window = widget_->GetNativeWindow();
  // Enable the black backdrop layer behind the window if the window
  // is in immersive fullscreen, maximized, yet the window can control
  // the bounds of the window in fullscreen/tablet mode (thus the
  // background can be visible).
  bool enable_backdrop =
      (widget_->IsFullscreen() || widget_->IsMaximized()) &&
      ash::wm::GetWindowState(window)->allow_set_bounds_direct();
  if (window->GetProperty(aura::client::kHasBackdrop) != enable_backdrop)
    window->SetProperty(aura::client::kHasBackdrop, enable_backdrop);
}

gfx::Point ShellSurface::GetMouseLocation() const {
  aura::Window* const root_window = widget_->GetNativeWindow()->GetRootWindow();
  gfx::Point location =
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
  aura::Window::ConvertPointToTarget(
      root_window, widget_->GetNativeWindow()->parent(), &location);
  return location;
}

void ShellSurface::EnsureCompositorIsLockedForOrientationChange() {
  if (!orientation_compositor_lock_) {
    ui::Compositor* compositor =
      widget_->GetNativeWindow()->layer()->GetCompositor();
    orientation_compositor_lock_ = compositor->GetCompositorLock(
        this, base::TimeDelta::FromMilliseconds(kOrientationLockTimeoutMs));
  }
}

}  // namespace exo
