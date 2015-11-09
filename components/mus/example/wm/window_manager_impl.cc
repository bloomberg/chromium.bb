// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_impl.h"

#include "components/mus/example/wm/move_loop.h"
#include "components/mus/example/wm/non_client_frame_controller.h"
#include "components/mus/example/wm/property_util.h"
#include "components/mus/example/wm/public/interfaces/container.mojom.h"
#include "components/mus/example/wm/window_manager_application.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace {

bool IsPointerEvent(const mus::mojom::Event& event) {
  return event.action == mus::mojom::EVENT_TYPE_POINTER_CANCEL ||
         event.action == mus::mojom::EVENT_TYPE_POINTER_DOWN ||
         event.action == mus::mojom::EVENT_TYPE_POINTER_MOVE ||
         event.action == mus::mojom::EVENT_TYPE_POINTER_UP;
}

}  // namespace

namespace mojo {

template <>
struct TypeConverter<const std::vector<uint8_t>, Array<uint8_t>> {
  static const std::vector<uint8_t> Convert(const Array<uint8_t>& input) {
    return input.storage();
  }
};

}  // namespace mojo

WindowManagerImpl::WindowManagerImpl(WindowManagerApplication* state)
    : state_(state) {}

WindowManagerImpl::~WindowManagerImpl() {
  mus::Window* parent =
      state_->GetWindowForContainer(ash::mojom::CONTAINER_USER_WINDOWS);
  if (!parent)
    return;

  for (mus::Window* child : parent->children())
    child->RemoveObserver(this);
}

void WindowManagerImpl::OpenWindow(
    mus::mojom::WindowTreeClientPtr client,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) {
  mus::Window* root = state_->root();
  DCHECK(root);

  const int width = (root->bounds().width() - 240);
  const int height = (root->bounds().height() - 240);

  mus::Window* child_window = root->connection()->NewWindow();
  // TODO(beng): mus::Window should have a "SetSharedProperties" method that
  //             joins the supplied map onto the internal one.
  for (auto prop : properties)
    child_window->SetSharedProperty(prop.GetKey(), prop.GetValue());

  gfx::Rect bounds(40 + (state_->window_count() % 4) * 40,
                   40 + (state_->window_count() % 4) * 40, width, height);
  child_window->SetBounds(bounds);
  GetContainerForChild(child_window)->AddChild(child_window);
  child_window->Embed(client.Pass());
  child_window->AddObserver(this);

  // NonClientFrameController deletes itself when the window is destroyed.
  new NonClientFrameController(state_->app()->shell(), child_window);

  state_->IncrementWindowCount();
}

void WindowManagerImpl::SetPreferredSize(
    mus::Id window_id,
    mojo::SizePtr size,
    const WindowManagerErrorCodeCallback& callback) {
  mus::Window* window = state_->GetWindowById(window_id);
  if (window)
    SetWindowPreferredSize(window, size.To<gfx::Size>());

  callback.Run(window
                   ? mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS
                   : mus::mojom::WINDOW_MANAGER_ERROR_CODE_ERROR_ACCESS_DENIED);
}

void WindowManagerImpl::SetShowState(
    mus::Id window_id,
    mus::mojom::ShowState show_state,
    const WindowManagerErrorCodeCallback& callback){
  mus::Window* window = state_->GetWindowById(window_id);
  if (window) {
    window->SetSharedProperty<int32_t>(
        mus::mojom::WindowManager::kShowState_Property, show_state);
  }
  callback.Run(window
                   ? mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS
                   : mus::mojom::WINDOW_MANAGER_ERROR_CODE_ERROR_ACCESS_DENIED);
}

void WindowManagerImpl::SetResizeBehavior(
    uint32_t window_id,
    mus::mojom::ResizeBehavior resize_behavior) {
  mus::Window* window = state_->GetWindowById(window_id);
  if (window) {
    window->SetSharedProperty<int32_t>(
        mus::mojom::WindowManager::kResizeBehavior_Property, resize_behavior);
  }
}

void WindowManagerImpl::GetConfig(const GetConfigCallback& callback) {
  mus::mojom::WindowManagerConfigPtr config(
      mus::mojom::WindowManagerConfig::New());
  config->displays = mojo::Array<mus::mojom::DisplayPtr>::New(1);
  config->displays[0] = mus::mojom::Display::New();
  config->displays[0]->id = 2001;
  config->displays[0]->bounds = mojo::Rect::New();
  config->displays[0]->bounds->y = 0;
  config->displays[0]->bounds->width = state_->root()->bounds().width();
  config->displays[0]->bounds->height = state_->root()->bounds().width();
  config->displays[0]->work_area = config->displays[0]->bounds.Clone();
  config->displays[0]->device_pixel_ratio =
      state_->root()->viewport_metrics().device_pixel_ratio;

  // The insets are roughly what is needed by CustomFrameView. The expectation
  // is at some point we'll write our own NonClientFrameView and get the insets
  // from it.
  config->normal_client_area_insets = mojo::Insets::New();
  config->normal_client_area_insets->top = 23;
  config->normal_client_area_insets->left = 5;
  config->normal_client_area_insets->right = 5;
  config->normal_client_area_insets->bottom = 5;

  config->maximized_client_area_insets = mojo::Insets::New();
  config->maximized_client_area_insets->top = 21;
  config->maximized_client_area_insets->left = 0;
  config->maximized_client_area_insets->right = 0;
  config->maximized_client_area_insets->bottom = 0;

  callback.Run(config.Pass());
}

void WindowManagerImpl::OnWindowDestroyed(mus::Window* window) {
  window->RemoveObserver(this);
}

void WindowManagerImpl::OnWindowInputEvent(mus::Window* window,
                                           const mus::mojom::EventPtr& event) {
  if (move_loop_ && IsPointerEvent(*event)) {
    if (move_loop_->Move(*event) == MoveLoop::DONE)
      move_loop_.reset();
    return;
  }
  if (!move_loop_ && event->action == mus::mojom::EVENT_TYPE_POINTER_DOWN)
    move_loop_ = MoveLoop::Create(window, *event);
}

mus::Window* WindowManagerImpl::GetContainerForChild(mus::Window* child) {
  ash::mojom::Container container = GetRequestedContainer(child);
  return state_->GetWindowForContainer(container);
}
