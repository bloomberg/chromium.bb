// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/window_manager_impl.h"

#include <stdint.h>
#include <utility>

#include "components/mus/common/types.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mash/wm/non_client_frame_controller.h"
#include "mash/wm/property_util.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/window_manager_application.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace mojo {

template <>
struct TypeConverter<const std::vector<uint8_t>, Array<uint8_t>> {
  static const std::vector<uint8_t> Convert(const Array<uint8_t>& input) {
    return input.storage();
  }
};

}  // namespace mojo

namespace mash {
namespace wm {

WindowManagerImpl::WindowManagerImpl()
    : state_(nullptr) {}

WindowManagerImpl::~WindowManagerImpl() {
  if (!state_)
    return;
  for (auto container : state_->root()->children()) {
    container->RemoveObserver(this);
    for (auto child : container->children())
      child->RemoveObserver(this);
  }
}

void WindowManagerImpl::Initialize(WindowManagerApplication* state) {
  DCHECK(state);
  DCHECK(!state_);
  state_ = state;
  // The children of the root are considered containers.
  for (auto container : state_->root()->children()) {
    container->AddObserver(this);
    for (auto child : container->children())
      child->AddObserver(this);
  }
}

gfx::Rect WindowManagerImpl::CalculateDefaultBounds(mus::Window* window) const {
  DCHECK(state_);
  int width, height;
  const gfx::Size pref = GetWindowPreferredSize(window);
  const mus::Window* root = state_->root();
  if (pref.IsEmpty()) {
    width = root->bounds().width() - 240;
    height = root->bounds().height() - 240;
  } else {
    // TODO(sky): likely want to constrain more than root size.
    const gfx::Size max_size = GetMaximizedWindowBounds().size();
    width = std::max(0, std::min(max_size.width(), pref.width()));
    height = std::max(0, std::min(max_size.height(), pref.height()));
  }
  return gfx::Rect(40 + (state_->window_count() % 4) * 40,
                   40 + (state_->window_count() % 4) * 40, width, height);
}

gfx::Rect WindowManagerImpl::GetMaximizedWindowBounds() const {
  DCHECK(state_);
  return gfx::Rect(state_->root()->bounds().size());
}

mus::Window* WindowManagerImpl::NewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties,
    mus::mojom::WindowTreeClientPtr client) {
  DCHECK(state_);
  mus::Window* root = state_->root();
  DCHECK(root);

  const bool provide_non_client_frame =
      GetWindowType(*properties) == mus::mojom::WINDOW_TYPE_WINDOW;
  if (provide_non_client_frame)
    (*properties)[mus::mojom::kWaitForUnderlay_Property].clear();

  // TODO(sky): constrain and validate properties before passing to server.
  mus::Window* window = root->connection()->NewWindow(properties);
  window->SetBounds(CalculateDefaultBounds(window));

  mojom::Container container = GetRequestedContainer(window);
  state_->GetWindowForContainer(container)->AddChild(window);

  if (client)
    window->Embed(std::move(client));

  if (provide_non_client_frame) {
    // NonClientFrameController deletes itself when |window| is destroyed.
    new NonClientFrameController(state_->app()->shell(), window,
                                 state_->window_tree_host());
  }

  state_->IncrementWindowCount();

  return window;
}

void WindowManagerImpl::OnTreeChanging(const TreeChangeParams& params) {
  DCHECK(state_);
  if (state_->WindowIsContainer(params.old_parent))
    params.target->RemoveObserver(this);
  else if (state_->WindowIsContainer(params.new_parent))
    params.target->AddObserver(this);
}

void WindowManagerImpl::OnWindowEmbeddedAppDisconnected(mus::Window* window) {
  window->Destroy();
}

void WindowManagerImpl::OpenWindow(
    mus::mojom::WindowTreeClientPtr client,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  mus::Window::SharedProperties properties =
      transport_properties.To<mus::Window::SharedProperties>();
  NewTopLevelWindow(&properties, std::move(client));
}

void WindowManagerImpl::GetConfig(const GetConfigCallback& callback) {
  DCHECK(state_);
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
  const gfx::Insets client_area_insets =
      NonClientFrameController::GetPreferredClientAreaInsets();
  config->normal_client_area_insets = mojo::Insets::From(client_area_insets);

  config->maximized_client_area_insets = mojo::Insets::From(client_area_insets);

  config->max_title_bar_button_width =
      NonClientFrameController::GetMaxTitleBarButtonWidth();

  callback.Run(std::move(config));
}

bool WindowManagerImpl::OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) {
  // By returning true the bounds of |window| is updated.
  return true;
}

bool WindowManagerImpl::OnWmSetProperty(
    mus::Window* window,
    const std::string& name,
    scoped_ptr<std::vector<uint8_t>>* new_data) {
  // TODO(sky): constrain this to set of keys we know about, and allowed
  // values.
  return name == mus::mojom::WindowManager::kShowState_Property ||
         name == mus::mojom::WindowManager::kPreferredSize_Property ||
         name == mus::mojom::WindowManager::kResizeBehavior_Property ||
         name == mus::mojom::WindowManager::kWindowTitle_Property;
}

mus::Window* WindowManagerImpl::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return NewTopLevelWindow(properties, nullptr);
}

}  // namespace wm
}  // namespace mash
