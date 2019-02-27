// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/custom_tab/arc_custom_tab_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/ws/window_service_owner.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Tries to find the specified ARC window recursively.
aura::Window* FindArcWindow(const aura::Window::Windows& windows,
                            const std::string& arc_app_id) {
  for (aura::Window* window : windows) {
    const std::string* id = exo::GetShellApplicationId(window);
    if (id && *id == arc_app_id)
      return window;

    aura::Window* result = FindArcWindow(window->children(), arc_app_id);
    if (result)
      return result;
  }
  return nullptr;
}

// Tries to find the specified ARC surface window recursively.
aura::Window* FindSurfaceWindow(aura::Window* window, int surface_id) {
  auto* surface = exo::Surface::AsSurface(window);
  if (surface && surface->GetClientSurfaceId() == surface_id)
    return window;

  for (aura::Window* child : window->children()) {
    aura::Window* result = FindSurfaceWindow(child, surface_id);
    if (result)
      return result;
  }
  return nullptr;
}

}  // namespace

// static
mojom::ArcCustomTabViewPtr ArcCustomTabView::Create(int32_t task_id,
                                                    int32_t surface_id,
                                                    int32_t top_margin) {
  const std::string arc_app_id =
      base::StringPrintf("org.chromium.arc.%d", task_id);
  aura::Window* arc_app_window =
      FindArcWindow(ash::Shell::Get()->GetAllRootWindows(), arc_app_id);
  if (!arc_app_window) {
    LOG(ERROR) << "No ARC window with the specified task ID " << task_id;
    return nullptr;
  }
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(arc_app_window);
  if (!widget) {
    LOG(ERROR) << "No widget for the ARC app window.";
    return nullptr;
  }
  auto* parent = widget->widget_delegate()->GetContentsView();
  auto* view = new ArcCustomTabView(surface_id, top_margin);
  parent->AddChildView(view);
  parent->SetLayoutManager(std::make_unique<views::FillLayout>());
  parent->Layout();

  view->remote_view_host_->GetNativeViewContainer()->SetEventTargeter(
      std::make_unique<aura::WindowTargeter>());

  mojom::ArcCustomTabViewPtr ptr;
  view->Bind(&ptr);
  return ptr;
}

void ArcCustomTabView::EmbedUsingToken(const base::UnguessableToken& token) {
  remote_view_host_->EmbedUsingToken(token, 0, base::BindOnce([](bool success) {
                                       LOG_IF(ERROR, !success)
                                           << "Failed to embed.";
                                     }));
}

void ArcCustomTabView::Layout() {
  if (!GetWidget()) {
    LOG(ERROR) << "No widget";
    return;
  }
  DCHECK(GetWidget()->GetNativeWindow());
  aura::Window* surface_window =
      FindSurfaceWindow(GetWidget()->GetNativeWindow(), surface_id_);
  if (!surface_window) {
    LOG(ERROR) << "Surface not found " << surface_id_;
    return;
  }
  gfx::Point topleft(0, top_margin_),
      bottomright(surface_window->bounds().width(),
                  surface_window->bounds().height());
  ConvertPointFromWindow(surface_window, &topleft);
  ConvertPointFromWindow(surface_window, &bottomright);
  gfx::Rect bounds(topleft, gfx::Size(bottomright.x() - topleft.x(),
                                      bottomright.y() - topleft.y()));
  remote_view_host_->SetBoundsRect(bounds);

  // Stack the remote view window at top.
  aura::Window* window = remote_view_host_->GetNativeViewContainer();
  window->parent()->StackChildAtTop(window);
}

ArcCustomTabView::ArcCustomTabView(int32_t surface_id, int32_t top_margin)
    : binding_(this),
      remote_view_host_(new ws::ServerRemoteViewHost(
          ash::Shell::Get()->window_service_owner()->window_service())),
      surface_id_(surface_id),
      top_margin_(top_margin),
      weak_ptr_factory_(this) {
  AddChildView(remote_view_host_);
}

ArcCustomTabView::~ArcCustomTabView() = default;

void ArcCustomTabView::Bind(mojom::ArcCustomTabViewPtr* ptr) {
  binding_.Bind(mojo::MakeRequest(ptr));
  binding_.set_connection_error_handler(
      base::BindOnce(&ArcCustomTabView::Close, weak_ptr_factory_.GetWeakPtr()));
}

void ArcCustomTabView::Close() {
  delete this;
}

void ArcCustomTabView::ConvertPointFromWindow(aura::Window* window,
                                              gfx::Point* point) {
  aura::Window::ConvertPointToTarget(window, GetWidget()->GetNativeWindow(),
                                     point);
  views::View::ConvertPointFromWidget(parent(), point);
}

}  // namespace ash
