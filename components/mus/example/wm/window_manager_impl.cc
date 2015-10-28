// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_impl.h"

#include "components/mus/example/wm/move_loop.h"
#include "components/mus/example/wm/property_util.h"
#include "components/mus/example/wm/public/interfaces/container.mojom.h"
#include "components/mus/example/wm/window_manager_application.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace {

bool IsPointerEvent(const mojo::Event& event) {
  return event.action == mojo::EVENT_TYPE_POINTER_CANCEL ||
         event.action == mojo::EVENT_TYPE_POINTER_DOWN ||
         event.action == mojo::EVENT_TYPE_POINTER_MOVE ||
         event.action == mojo::EVENT_TYPE_POINTER_UP;
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

  state_->IncrementWindowCount();
}

void WindowManagerImpl::SetPreferredSize(
    mus::Id window_id,
    mojo::SizePtr size,
    const WindowManagerErrorCodeCallback& callback) {
  mus::Window* window = state_->GetWindowById(window_id);
  window->SetSharedProperty<gfx::Size>(
      mus::mojom::WindowManager::kPreferredSize_Property, size.To<gfx::Size>());

  callback.Run(mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS);
}

void WindowManagerImpl::SetBounds(
    mus::Id window_id,
    mojo::RectPtr bounds,
    const WindowManagerErrorCodeCallback& callback) {
  mus::Window* window = state_->root()->GetChildById(window_id);
  window->SetBounds(bounds->To<gfx::Rect>());
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
  displays[0]->bounds->width = state_->root()->bounds().width();
  displays[0]->bounds->height = state_->root()->bounds().width();
  displays[0]->work_area = displays[0]->bounds.Clone();
  displays[0]->device_pixel_ratio =
      state_->root()->viewport_metrics().device_pixel_ratio;
  callback.Run(displays.Pass());
}

void WindowManagerImpl::OnWindowDestroyed(mus::Window* window) {
  window->RemoveObserver(this);
}

void WindowManagerImpl::OnWindowInputEvent(mus::Window* window,
                                           const mojo::EventPtr& event) {
  if (move_loop_ && IsPointerEvent(*event)) {
    if (move_loop_->Move(*event) == MoveLoop::DONE)
      move_loop_.reset();
    return;
  }
  if (!move_loop_ && event->action == mojo::EVENT_TYPE_POINTER_DOWN)
    move_loop_ = MoveLoop::Create(window, *event);
}

mus::Window* WindowManagerImpl::GetContainerForChild(mus::Window* child) {
  ash::mojom::Container container = GetRequestedContainer(child);
  return state_->GetWindowForContainer(container);
}
