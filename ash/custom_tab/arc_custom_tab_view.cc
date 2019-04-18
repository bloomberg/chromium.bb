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

// Enumerates surfaces under the window.
void EnumerateSurfaces(aura::Window* window, std::vector<exo::Surface*>* out) {
  auto* surface = exo::Surface::AsSurface(window);
  if (surface)
    out->push_back(surface);
  for (aura::Window* child : window->children())
    EnumerateSurfaces(child, out);
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
  auto* view = new ArcCustomTabView(arc_app_window, surface_id, top_margin);
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
  exo::Surface* surface = FindSurface();
  if (!surface)
    return;
  DCHECK(observed_surfaces_.empty());
  aura::Window* surface_window = surface->window();
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

void ArcCustomTabView::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.receiver == arc_app_window_) {
    auto* surface = exo::Surface::AsSurface(params.target);
    if (surface && params.new_parent != nullptr) {
      // Call Layout() aggressively without checking the surface ID to start
      // observing surface ID updates in case it's not set yet.
      Layout();
    }
  }
}

void ArcCustomTabView::OnWindowPropertyChanged(aura::Window* window,
                                               const void* key,
                                               intptr_t old) {
  if (observed_surfaces_.contains(window)) {
    if (key == exo::kClientSurfaceIdKey) {
      // Client surface ID was updated. Try to find the surface again.
      Layout();
    }
  }
}

void ArcCustomTabView::OnWindowDestroying(aura::Window* window) {
  if (observed_surfaces_.contains(window)) {
    window->RemoveObserver(this);
    observed_surfaces_.erase(window);
  }
}

ArcCustomTabView::ArcCustomTabView(aura::Window* arc_app_window,
                                   int32_t surface_id,
                                   int32_t top_margin)
    : binding_(this),
      remote_view_host_(new ws::ServerRemoteViewHost(
          ash::Shell::Get()->window_service_owner()->window_service())),
      arc_app_window_(arc_app_window),
      surface_id_(surface_id),
      top_margin_(top_margin),
      weak_ptr_factory_(this) {
  AddChildView(remote_view_host_);
  arc_app_window_->AddObserver(this);
}

ArcCustomTabView::~ArcCustomTabView() {
  for (auto* window : observed_surfaces_)
    window->RemoveObserver(this);
  arc_app_window_->RemoveObserver(this);
}

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

exo::Surface* ArcCustomTabView::FindSurface() {
  std::vector<exo::Surface*> surfaces;
  EnumerateSurfaces(arc_app_window_, &surfaces);

  // Try to find the surface.
  for (auto* surface : surfaces) {
    if (surface->GetClientSurfaceId() == surface_id_) {
      // Stop observing surfaces for ID updates.
      for (auto* window : observed_surfaces_)
        window->RemoveObserver(this);
      observed_surfaces_.clear();
      return surface;
    }
  }
  // Surface not found. Start observing surfaces for ID updates.
  for (auto* surface : surfaces) {
    if (surface->GetClientSurfaceId() == 0) {
      observed_surfaces_.insert(surface->window());
      surface->window()->AddObserver(this);
    }
  }
  return nullptr;
}

}  // namespace ash
