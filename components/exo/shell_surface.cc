// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

DECLARE_WINDOW_PROPERTY_TYPE(std::string*)

namespace exo {
namespace {

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
  CustomWindowTargeter() {}
  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = ShellSurface::GetMainSurface(window);
    if (!surface)
      return false;

    gfx::Point local_point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);

    aura::Window::ConvertPointToTarget(window, surface, &local_point);
    return surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

class ShellSurfaceWidget : public views::Widget {
 public:
  explicit ShellSurfaceWidget(ShellSurface* shell_surface)
      : shell_surface_(shell_surface) {}

  // Overridden from views::Widget
  void Close() override { shell_surface_->Close(); }

 private:
  ShellSurface* const shell_surface_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceWidget);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(std::string*, kApplicationIdKey, nullptr)
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

ShellSurface::ShellSurface(Surface* surface,
                           ShellSurface* parent,
                           const gfx::Rect& initial_bounds,
                           bool activatable)
    : widget_(nullptr),
      surface_(surface),
      parent_(parent ? parent->GetWidget()->GetNativeWindow() : nullptr),
      initial_bounds_(initial_bounds),
      activatable_(activatable) {
  ash::Shell::GetInstance()->activation_client()->AddObserver(this);
  surface_->SetSurfaceDelegate(this);
  surface_->AddSurfaceObserver(this);
  surface_->Show();
  set_owned_by_client();
  if (parent_)
    parent_->AddObserver(this);
}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurface(surface, nullptr, gfx::Rect(), true) {}

ShellSurface::~ShellSurface() {
  ash::Shell::GetInstance()->activation_client()->RemoveObserver(this);
  if (surface_) {
    surface_->SetSurfaceDelegate(nullptr);
    surface_->RemoveSurfaceObserver(this);
  }
  if (parent_)
    parent_->RemoveObserver(this);
  if (widget_) {
    ash::wm::GetWindowState(widget_->GetNativeWindow())->RemoveObserver(this);
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
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
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_)
    CreateShellSurfaceWidget();

  // Ask client to configure its surface if already maximized.
  if (widget_->IsMaximized()) {
    Configure();
    return;
  }

  widget_->Maximize();
}

void ShellSurface::Restore() {
  TRACE_EVENT0("exo", "ShellSurface::Restore");

  if (!widget_)
    return;

  // Ask client to configure its surface if already restored.
  if (!widget_->IsMaximized()) {
    Configure();
    return;
  }

  widget_->Restore();
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_)
    CreateShellSurfaceWidget();

  // Ask client to configure its surface if fullscreen state is not changing.
  if (widget_->IsFullscreen() == fullscreen) {
    Configure();
    return;
  }

  widget_->SetFullscreen(fullscreen);
}

void ShellSurface::SetTitle(const base::string16& title) {
  TRACE_EVENT1("exo", "ShellSurface::SetTitle", "title",
               base::UTF16ToUTF8(title));

  title_ = title;
  if (widget_)
    widget_->UpdateWindowTitle();
}

// static
void ShellSurface::SetApplicationId(aura::Window* window,
                                    std::string* application_id) {
  window->SetProperty(kApplicationIdKey, application_id);
}

// static
const std::string ShellSurface::GetApplicationId(aura::Window* window) {
  std::string* string_ptr = window->GetProperty(kApplicationIdKey);
  return string_ptr ? *string_ptr : std::string();
}

void ShellSurface::SetApplicationId(const std::string& application_id) {
  TRACE_EVENT1("exo", "ShellSurface::SetApplicationId", "application_id",
               application_id);

  application_id_ = application_id;
}

void ShellSurface::Move() {
  TRACE_EVENT0("exo", "ShellSurface::Move");

  if (widget_) {
    widget_->RunMoveLoop(gfx::Vector2d(), views::Widget::MOVE_LOOP_SOURCE_MOUSE,
                         views::Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE);
  }
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

  geometry_ = geometry;
}

// static
void ShellSurface::SetMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

// static
Surface* ShellSurface::GetMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

scoped_ptr<base::trace_event::TracedValue> ShellSurface::AsTracedValue() const {
  scoped_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("title", base::UTF16ToUTF8(title_));
  value->SetString("application_id", application_id_);
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSurfaceCommit() {
  surface_->CommitSurfaceHierarchy();

  if (enabled() && !widget_)
    CreateShellSurfaceWidget();

  if (widget_) {
    // Update surface bounds and widget size.
    gfx::Rect visible_bounds = GetVisibleBounds();
    surface_->SetBounds(
        gfx::Rect(gfx::Point() - visible_bounds.OffsetFromOrigin(),
                  surface_->layer()->size()));
    widget_->SetSize(visible_bounds.size());

    // Show widget if not already visible.
    if (!widget_->IsClosed() && !widget_->IsVisible())
      widget_->Show();
  }
}

bool ShellSurface::IsSurfaceSynchronized() const {
  // A shell surface is always desynchronized.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void ShellSurface::OnSurfaceDestroying(Surface* surface) {
  if (widget_)
    SetMainSurface(widget_->GetNativeWindow(), nullptr);
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;

  // Hide widget before surface is destroyed. This allows hide animations to
  // run using the current surface contents.
  if (widget_)
    widget_->Hide();

  // Note: In its use in the Wayland server implementation, the surface
  // destroyed callback may destroy the ShellSurface instance. This call needs
  // to be last so that the instance can be destroyed.
  if (!surface_destroyed_callback_.is_null())
    surface_destroyed_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

base::string16 ShellSurface::GetWindowTitle() const {
  return title_;
}

void ShellSurface::WindowClosing() {
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
  return new CustomFrameView(widget);
}

bool ShellSurface::WidgetHasHitTestMask() const {
  return surface_ ? surface_->HasHitTestMask() : false;
}

void ShellSurface::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(WidgetHasHitTestMask());
  surface_->GetHitTestMask(mask);
  gfx::Point origin = surface_->bounds().origin();
  mask->offset(SkIntToScalar(origin.x()), SkIntToScalar(origin.y()));
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size ShellSurface::GetPreferredSize() const {
  if (!geometry_.IsEmpty())
    return geometry_.size();

  return surface_ ? surface_->GetVisibleBounds().size() : gfx::Size();
}

////////////////////////////////////////////////////////////////////////////////
// ash::wm::WindowStateObserver overrides:

void ShellSurface::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::wm::WindowStateType old_type) {
  ash::wm::WindowStateType new_type = window_state->GetStateType();
  if (old_type == ash::wm::WINDOW_STATE_TYPE_MAXIMIZED ||
      new_type == ash::wm::WINDOW_STATE_TYPE_MAXIMIZED ||
      old_type == ash::wm::WINDOW_STATE_TYPE_FULLSCREEN ||
      new_type == ash::wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    Configure();
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurface::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  parent_ = nullptr;
  // Disable shell surface in case parent is destroyed before shell surface
  // widget has been created.
  SetEnabled(false);
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::ActivationChangeObserver overrides:

void ShellSurface::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!widget_)
    return;

  if (gained_active == widget_->GetNativeWindow() ||
      lost_active == widget_->GetNativeWindow()) {
    DCHECK(activatable_);
    Configure();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, private:

void ShellSurface::CreateShellSurfaceWidget() {
  DCHECK(enabled());
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(), ash::kShellWindowId_DefaultContainer);
  if (!initial_bounds_.IsEmpty()) {
    gfx::Point position(initial_bounds_.origin());
    if (parent_) {
      aura::Window::ConvertPointToTarget(GetMainSurface(parent_), params.parent,
                                         &position);
    }
    params.bounds = gfx::Rect(position + GetVisibleBounds().OffsetFromOrigin(),
                              initial_bounds_.size());
  }
  params.activatable = activatable_ ? views::Widget::InitParams::ACTIVATABLE_YES
                                    : views::Widget::InitParams::ACTIVATABLE_NO;

  // Note: NativeWidget owns this widget.
  widget_ = new ShellSurfaceWidget(this);
  widget_->Init(params);
  widget_->GetNativeWindow()->SetName("ExoShellSurface");
  widget_->GetNativeWindow()->AddChild(surface_);
  widget_->GetNativeWindow()->SetEventTargeter(
      make_scoped_ptr(new CustomWindowTargeter));
  SetApplicationId(widget_->GetNativeWindow(), &application_id_);
  SetMainSurface(widget_->GetNativeWindow(), surface_);

  // Start tracking window state changes.
  ash::wm::GetWindowState(widget_->GetNativeWindow())->AddObserver(this);

  // Make shell surface a transient child if |parent_| has been set.
  if (parent_)
    wm::AddTransientChild(parent_, widget_->GetNativeWindow());

  // Ash manages the position of a top-level shell surfaces unless
  // |initial_bounds_| has been set.
  if (initial_bounds_.IsEmpty()) {
    ash::wm::GetWindowState(widget_->GetNativeWindow())
        ->set_window_position_managed(true);
  }
}

void ShellSurface::Configure() {
  DCHECK(widget_);

  if (configure_callback_.is_null())
    return;

  configure_callback_.Run(
      widget_->GetWindowBoundsInScreen().size(),
      ash::wm::GetWindowState(widget_->GetNativeWindow())->GetStateType(),
      widget_->IsActive());
}

gfx::Rect ShellSurface::GetVisibleBounds() const {
  // Use |geometry_| if set, otherwise use the visual bounds of the surface.
  return geometry_.IsEmpty() ? surface_->GetVisibleBounds() : geometry_;
}

}  // namespace exo
