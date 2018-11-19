// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/fullscreen_shell_surface.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {
// Main surface key
DEFINE_LOCAL_UI_CLASS_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr);

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kStartupIdKey, nullptr);

}  // namespace

FullscreenShellSurface::FullscreenShellSurface(Surface* surface)
    : SurfaceTreeHost("FullscreenShellSurfaceHost") {
  surface->AddSurfaceObserver(this);
  SetRootSurface(surface);
  host_window()->Show();
  set_owned_by_client();
  CreateFullscreenShellSurfaceWidget(ui::SHOW_STATE_FULLSCREEN);
  widget_->SetFullscreen(true);
}

FullscreenShellSurface::~FullscreenShellSurface() {
  if (widget_) {
    widget_->GetNativeWindow()->RemoveObserver(this);
    // Remove transient children so they are not automatically destroyed.
    for (auto* child : wm::GetTransientChildren(widget_->GetNativeWindow()))
      wm::RemoveTransientChild(widget_->GetNativeWindow(), child);
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
  }
  if (parent_)
    parent_->RemoveObserver(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
}

// static
void FullscreenShellSurface::SetStartupId(
    aura::Window* window,
    const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "FullscreenShellSurface::SetStartupId", "startup_id",
               id ? *id : "null");

  if (id)
    window->SetProperty(kStartupIdKey, new std::string(*id));
  else
    window->ClearProperty(kStartupIdKey);
}

// static
void FullscreenShellSurface::SetStartupId(const char* startup_id) {
  // Store the value in |startup_id_| in case the window does not exist yet.
  if (startup_id)
    startup_id_ = std::string(startup_id);
  else
    startup_id_.reset();

  if (widget_ && widget_->GetNativeWindow())
    SetStartupId(widget_->GetNativeWindow(), startup_id_);
}

// static
std::string* FullscreenShellSurface::GetStartupId(aura::Window* window) {
  return window->GetProperty(kStartupIdKey);
}

void FullscreenShellSurface::SetApplicationId(
    aura::Window* window,
    const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetApplicationId", "application_id",
               id ? *id : "null");

  if (id)
    window->SetProperty(kApplicationIdKey, new std::string(*id));
  else
    window->ClearProperty(kApplicationIdKey);
}

// static
std::string* FullscreenShellSurface::GetApplicationId(aura::Window* window) {
  return window->GetProperty(kApplicationIdKey);
}

void FullscreenShellSurface::SetApplicationId(const char* application_id) {
  // Store the value in |application_id_| in case the window does not exist yet.
  if (application_id)
    application_id_ = std::string(application_id);
  else
    application_id_.reset();

  if (widget_ && widget_->GetNativeWindow())
    SetApplicationId(widget_->GetNativeWindow(), application_id_);
}

// static
void FullscreenShellSurface::SetMainSurface(aura::Window* window,
                                            Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

// static
Surface* FullscreenShellSurface::GetMainSurface(aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

void FullscreenShellSurface::Maximize() {
  if (!widget_)
    return;

  widget_->Maximize();
}

void FullscreenShellSurface::Minimize() {
  if (!widget_)
    return;

  widget_->Minimize();
}

void FullscreenShellSurface::Close() {
  if (!close_callback_.is_null())
    close_callback_.Run();
}

void FullscreenShellSurface::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();
  if (!OnPreWidgetCommit())
    return;

  CommitWidget();
  SubmitCompositorFrame();
}

bool FullscreenShellSurface::IsInputEnabled(Surface*) const {
  return true;
}

void FullscreenShellSurface::OnSetFrame(SurfaceFrameType frame_type) {}

void FullscreenShellSurface::OnSetFrameColors(SkColor active_color,
                                              SkColor inactive_color) {}

void FullscreenShellSurface::OnSetStartupId(const char* startup_id) {
  SetStartupId(startup_id);
}

void FullscreenShellSurface::OnSetApplicationId(const char* application_id) {
  SetApplicationId(application_id);
}

void FullscreenShellSurface::OnSurfaceDestroying(Surface* surface) {
  DCHECK_EQ(root_surface(), surface);
  surface->RemoveSurfaceObserver(this);
  SetRootSurface(nullptr);

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
  std::move(surface_destroyed_callback_).Run();
}

bool FullscreenShellSurface::CanResize() const {
  return false;
}

bool FullscreenShellSurface::CanMaximize() const {
  return true;
}

bool FullscreenShellSurface::CanMinimize() const {
  return true;
}

bool FullscreenShellSurface::ShouldShowWindowTitle() const {
  return false;
}

void FullscreenShellSurface::WindowClosing() {
  SetEnabled(false);
  widget_ = nullptr;
}

views::Widget* FullscreenShellSurface::GetWidget() {
  return widget_;
}

const views::Widget* FullscreenShellSurface::GetWidget() const {
  return widget_;
}

views::View* FullscreenShellSurface::GetContentsView() {
  return this;
}

bool FullscreenShellSurface::WidgetHasHitTestMask() const {
  return true;
}

void FullscreenShellSurface::GetWidgetHitTestMask(gfx::Path* mask) const {
  GetHitTestMask(mask);
  gfx::Point origin = host_window()->bounds().origin();
  SkMatrix matrix;
  // TODO (sagallea) acquire scale from display
  float scale = 1.f;
  matrix.setScaleTranslate(
      SkFloatToScalar(1.0f / scale), SkFloatToScalar(1.0f / scale),
      SkIntToScalar(origin.x()), SkIntToScalar(origin.y()));
  mask->transform(matrix);
}

void FullscreenShellSurface::OnWindowDestroying(aura::Window* window) {
  if (window == parent_) {
    parent_ = nullptr;
    // |parent_| being set to null effects the ability to maximize the window.
    if (widget_)
      widget_->OnSizeConstraintsChanged();
  }

  window->RemoveObserver(this);
}

void FullscreenShellSurface::CreateFullscreenShellSurfaceWidget(
    ui::WindowShowState show_state) {
  DCHECK(enabled());
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.show_state = show_state;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.parent = WMHelper::GetInstance()->GetRootWindowForNewWindows();
  params.bounds = gfx::Rect(params.parent->bounds().size());

  widget_ = new views::Widget();
  widget_->Init(params);

  aura::Window* window = widget_->GetNativeWindow();
  window->SetName("FullscreenShellSurface");
  window->AddChild(host_window());

  SetApplicationId(window, application_id_);
  SetStartupId(window, startup_id_);
  SetMainSurface(window, root_surface());

  window->AddObserver(this);
}

void FullscreenShellSurface::CommitWidget() {
  if (!widget_)
    return;

  // Show widget if needed.
  if (!widget_->IsVisible()) {
    DCHECK(!widget_->IsClosed());
    widget_->Show();
  }
}

bool FullscreenShellSurface::OnPreWidgetCommit() {
  if (!widget_ && enabled() && host_window()->bounds().IsEmpty())
    return false;

  return true;
}

}  // namespace exo
