// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"

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

views::Widget::InitParams CreateWidgetInitParams(
    views::WidgetDelegate* delegate) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = delegate;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(), ash::kShellWindowId_DefaultContainer);
  return params;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

ShellSurface::ShellSurface(Surface* surface) : surface_(surface) {
  surface_->SetSurfaceDelegate(this);
  surface_->AddSurfaceObserver(this);
  surface_->Show();
  set_owned_by_client();
}

ShellSurface::~ShellSurface() {
  if (surface_) {
    surface_->SetSurfaceDelegate(nullptr);
    surface_->RemoveSurfaceObserver(this);
  }
  if (widget_)
    widget_->CloseNow();
}

void ShellSurface::SetToplevel() {
  TRACE_EVENT0("exo", "ShellSurface::SetToplevel");

  if (widget_) {
    DLOG(WARNING) << "Shell surface already mapped";
    return;
  }

  views::Widget::InitParams params = CreateWidgetInitParams(this);
  params.bounds = gfx::Rect(gfx::Size(1, 1));
  widget_.reset(new views::Widget);
  widget_->Init(params);
  widget_->GetNativeWindow()->set_owned_by_parent(false);
  widget_->GetNativeWindow()->SetName("ExoShellSurface");
  widget_->GetNativeWindow()->AddChild(surface_);

  // The position of a standard top level shell surface is managed by Ash.
  ash::wm::GetWindowState(widget_->GetNativeWindow())
      ->set_window_position_managed(true);
}

void ShellSurface::SetMaximized() {
  TRACE_EVENT0("exo", "ShellSurface::SetMaximized");

  if (widget_) {
    DLOG(WARNING) << "Shell surface already mapped";
    return;
  }

  views::Widget::InitParams params = CreateWidgetInitParams(this);
  params.show_state = ui::SHOW_STATE_MAXIMIZED;
  widget_.reset(new views::Widget);
  widget_->Init(params);
  widget_->GetNativeWindow()->set_owned_by_parent(false);
  widget_->GetNativeWindow()->SetName("ExoShellSurface");
  widget_->GetNativeWindow()->AddChild(surface_);
}

void ShellSurface::SetFullscreen() {
  TRACE_EVENT0("exo", "ShellSurface::SetFullscreen");

  if (widget_) {
    DLOG(WARNING) << "Shell surface already mapped";
    return;
  }

  views::Widget::InitParams params = CreateWidgetInitParams(this);
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  widget_.reset(new views::Widget);
  widget_->Init(params);
  widget_->GetNativeWindow()->set_owned_by_parent(false);
  widget_->GetNativeWindow()->SetName("ExoShellSurface");
  widget_->GetNativeWindow()->AddChild(surface_);
}

void ShellSurface::SetTitle(const base::string16& title) {
  TRACE_EVENT1("exo", "ShellSurface::SetTitle", "title",
               base::UTF16ToUTF8(title));

  title_ = title;
  if (widget_)
    widget_->UpdateWindowTitle();
}

void ShellSurface::Move() {
  TRACE_EVENT0("exo", "ShellSurface::Move");

  if (widget_) {
    widget_->RunMoveLoop(gfx::Vector2d(), views::Widget::MOVE_LOOP_SOURCE_MOUSE,
                         views::Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE);
  }
}

scoped_refptr<base::trace_event::TracedValue> ShellSurface::AsTracedValue()
    const {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue;
  value->SetString("title", base::UTF16ToUTF8(title_));
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSurfaceCommit() {
  surface_->CommitSurfaceHierarchy();
  if (widget_) {
    // Update surface bounds and widget size.
    gfx::Point origin;
    views::View::ConvertPointToWidget(this, &origin);
    surface_->SetBounds(gfx::Rect(origin, surface_->layer()->size()));
    widget_->SetSize(widget_->non_client_view()->GetPreferredSize());

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
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

base::string16 ShellSurface::GetWindowTitle() const {
  return title_;
}

views::Widget* ShellSurface::GetWidget() {
  return widget_.get();
}

const views::Widget* ShellSurface::GetWidget() const {
  return widget_.get();
}

views::View* ShellSurface::GetContentsView() {
  return this;
}

views::NonClientFrameView* ShellSurface::CreateNonClientFrameView(
    views::Widget* widget) {
  return new CustomFrameView(widget);
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size ShellSurface::GetPreferredSize() const {
  return surface_ ? surface_->GetPreferredSize() : gfx::Size();
}

}  // namespace exo
