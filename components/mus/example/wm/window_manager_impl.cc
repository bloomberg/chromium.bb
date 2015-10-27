// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_impl.h"

#include "components/mus/example/wm/container.h"
#include "components/mus/example/wm/window_manager_application.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"

WindowManagerImpl::WindowManagerImpl(WindowManagerApplication* state)
    : state_(state) {}

WindowManagerImpl::~WindowManagerImpl() {}

void WindowManagerImpl::OpenWindow(mus::mojom::WindowTreeClientPtr client) {
  mus::Window* root = state_->root();
  DCHECK(root);

  const int width = (root->bounds().width - 240);
  const int height = (root->bounds().height - 240);

  mus::Window* child_window = root->connection()->NewWindow();
  mojo::Rect bounds;
  bounds.x = 40 + (state_->window_count() % 4) * 40;
  bounds.y = 40 + (state_->window_count() % 4) * 40;
  bounds.width = width;
  bounds.height = height;
  child_window->SetBounds(bounds);
  state_->GetWindowForContainer(Container::USER_WINDOWS)
      ->AddChild(child_window);
  child_window->Embed(client.Pass());

  state_->IncrementWindowCount();
}

void WindowManagerImpl::SetPreferredSize(
    mus::Id window_id,
    mojo::SizePtr size,
    const WindowManagerErrorCodeCallback& callback) {
  mus::Window* window = state_->GetWindowById(window_id);
  window->SetSharedProperty<mojo::Size>(
      mus::mojom::WindowManager::kPreferredSize_Property, *size);

  callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS);
}

void WindowManagerImpl::SetBounds(
    mus::Id window_id,
    mojo::RectPtr bounds,
    const WindowManagerErrorCodeCallback& callback) {
  mus::Window* window = state_->root()->GetChildById(window_id);
  window->SetBounds(*bounds);
  callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS);
}

void WindowManagerImpl::SetShowState(
    mus::Id window_id,
    mus::mojom::ShowState show_state,
    const WindowManagerErrorCodeCallback& callback){
  mus::Window* window = state_->GetWindowById(window_id);
  window->SetSharedProperty<int32_t>(
      mus::mojom::WindowManager::kShowState_Property, show_state);
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
