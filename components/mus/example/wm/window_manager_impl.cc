// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_impl.h"

#include "components/mus/example/wm/container.h"
#include "components/mus/example/wm/window_manager_application.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"

namespace {

mus::Id GetWindowIdForContainer(mus::WindowTreeConnection* connection,
                                Container container) {
  return connection->GetConnectionId() << 16 | static_cast<uint16>(container);
}

}  // namespace

WindowManagerImpl::WindowManagerImpl(
    WindowManagerApplication* state,
    mojo::InterfaceRequest<mus::mojom::WindowManager> request)
    : state_(state),
      binding_(this, request.Pass()) {
}

WindowManagerImpl::~WindowManagerImpl() {}

void WindowManagerImpl::OpenWindow(mus::mojom::WindowTreeClientPtr client) {
  mus::Window* root = state_->root();
  DCHECK(root);
  mus::Id container_window_id =
      GetWindowIdForContainer(root->connection(), Container::USER_WINDOWS);
  mus::Window* container_window = root->GetChildById(container_window_id);

  const int width = (root->bounds().width - 240);
  const int height = (root->bounds().height - 240);

  mus::Window* child_window = root->connection()->NewWindow();
  windows_.insert(child_window->id());
  mojo::Rect bounds;
  bounds.x = 40 + (state_->window_count() % 4) * 40;
  bounds.y = 40 + (state_->window_count() % 4) * 40;
  bounds.width = width;
  bounds.height = height;
  child_window->SetBounds(bounds);
  container_window->AddChild(child_window);
  child_window->Embed(client.Pass());

  state_->IncrementWindowCount();
}

void WindowManagerImpl::CenterWindow(
    mus::Id window_id,
    mojo::SizePtr size,
    const WindowManagerErrorCodeCallback& callback) {
  auto it = std::find(windows_.begin(), windows_.end(), window_id);
  if (it == windows_.end()) {
    callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_ERROR_ACCESS_DENIED);
    return;
  }

  mus::Window* root = state_->root();
  mus::Id container_window_id =
      GetWindowIdForContainer(root->connection(), Container::USER_WINDOWS);
  mus::Window* container = root->GetChildById(container_window_id);
  mojo::Rect rect;
  rect.x = (container->bounds().width - size->width) / 2;
  rect.y = (container->bounds().height - size->height) / 2;
  rect.width = size->width;
  rect.height = size->height;
  mus::Window* window = root->GetChildById(window_id);
  DCHECK(window->parent() == container);
  window->SetBounds(rect);
  callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS);
}

void WindowManagerImpl::SetBounds(
    mus::Id window_id,
    mojo::RectPtr bounds,
    const WindowManagerErrorCodeCallback& callback) {
  auto it = std::find(windows_.begin(), windows_.end(), window_id);
  if (it == windows_.end()) {
    callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_ERROR_ACCESS_DENIED);
    return;
  }

  mus::Window* window = state_->root()->GetChildById(window_id);
  window->SetBounds(*bounds);
  callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS);
}

void WindowManagerImpl::GetDisplays(const GetDisplaysCallback& callback) {
  mojo::Array<mus::mojom::DisplayPtr> displays(1);
  displays[0] = mus::mojom::Display::New();
  displays[0]->id = 2001;
  displays[0]->bounds = mojo::Rect::New();
  displays[0]->bounds->y = 0;
  displays[0]->bounds->width = state_->root()->bounds().width;
  displays[0]->bounds->height = state_->root()->bounds().width;
  displays[0]->work_area = displays[0]->bounds.Clone();
  displays[0]->device_pixel_ratio =
      state_->root()->viewport_metrics().device_pixel_ratio;
  callback.Run(displays.Pass());
}

void WindowManagerImpl::OnWindowDestroyed(mus::Window* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window->id());
  DCHECK(it != windows_.end());
  windows_.erase(it);
}
