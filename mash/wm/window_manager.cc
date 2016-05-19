// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/window_manager.h"

#include <stdint.h>

#include <utility>

#include "ash/wm/common/container_finder.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mash/wm/bridge/wm_window_mus.h"
#include "mash/wm/non_client_frame_controller.h"
#include "mash/wm/property_util.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/root_window_controller.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace mash {
namespace wm {

WindowManager::WindowManager()
    : root_controller_(nullptr),
      window_manager_client_(nullptr),
      binding_(this) {}

WindowManager::~WindowManager() {
}

void WindowManager::Initialize(RootWindowController* root_controller,
                               session::mojom::Session* session) {
  DCHECK(root_controller);
  DCHECK(!root_controller_);
  root_controller_ = root_controller;

  // Observe all the containers so that windows can be added to/removed from the
  // |disconnected_app_handler_|.
  int count = static_cast<int>(mojom::Container::COUNT);
  for (int id = static_cast<int>(mojom::Container::ROOT) + 1; id < count;
       ++id) {
    mus::Window* container = root_controller_->GetWindowForContainer(
        static_cast<mojom::Container>(id));
    Add(container);

    // Add any pre-existing windows in the container to
    // |disconnected_app_handler_|.
    for (auto child : container->children()) {
      if (!root_controller_->WindowIsContainer(child))
        disconnected_app_handler_.Add(child);
    }
  }

  // The insets are roughly what is needed by CustomFrameView. The expectation
  // is at some point we'll write our own NonClientFrameView and get the insets
  // from it.
  mus::mojom::FrameDecorationValuesPtr frame_decoration_values =
      mus::mojom::FrameDecorationValues::New();
  const gfx::Insets client_area_insets =
      NonClientFrameController::GetPreferredClientAreaInsets();
  frame_decoration_values->normal_client_area_insets =
      mojo::Insets::From(client_area_insets);
  frame_decoration_values->maximized_client_area_insets =
      mojo::Insets::From(client_area_insets);
  frame_decoration_values->max_title_bar_button_width =
      NonClientFrameController::GetMaxTitleBarButtonWidth();
  window_manager_client_->SetFrameDecorationValues(
      std::move(frame_decoration_values));

  if (session)
    session->AddScreenlockStateListener(binding_.CreateInterfacePtrAndBind());
}

mus::Window* WindowManager::NewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  DCHECK(root_controller_);
  mus::Window* root = root_controller_->root();
  DCHECK(root);

  const bool provide_non_client_frame =
      GetWindowType(*properties) == mus::mojom::WindowType::WINDOW;
  if (provide_non_client_frame)
    (*properties)[mus::mojom::kWaitForUnderlay_Property].clear();

  // TODO(sky): constrain and validate properties before passing to server.
  mus::Window* window = root->connection()->NewWindow(properties);
  window->SetBounds(CalculateDefaultBounds(window));

  mus::Window* container_window = nullptr;
  if (window->HasSharedProperty(mojom::kWindowContainer_Property)) {
    container_window =
        root_controller_->GetWindowForContainer(GetRequestedContainer(window));
  } else {
    // TODO(sky): window->bounds() isn't quite right.
    container_window = WmWindowMus::GetMusWindow(
        ash::wm::GetDefaultParent(WmWindowMus::Get(root_controller_->root()),
                                  WmWindowMus::Get(window), window->bounds()));
  }
  DCHECK(root_controller_->WindowIsContainer(container_window));

  if (provide_non_client_frame) {
    NonClientFrameController::Create(root_controller_->GetConnector(),
                                     container_window, window,
                                     root_controller_->window_manager_client());
  } else {
    container_window->AddChild(window);
  }

  root_controller_->IncrementWindowCount();

  return window;
}

gfx::Rect WindowManager::CalculateDefaultBounds(mus::Window* window) const {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kInitialBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        mus::mojom::WindowManager::kInitialBounds_Property);
  }

  DCHECK(root_controller_);
  int width, height;
  const gfx::Size pref = GetWindowPreferredSize(window);
  const mus::Window* root = root_controller_->root();
  if (pref.IsEmpty()) {
    width = root->bounds().width() - 240;
    height = root->bounds().height() - 240;
  } else {
    // TODO(sky): likely want to constrain more than root size.
    const gfx::Size max_size = GetMaximizedWindowBounds().size();
    width = std::max(0, std::min(max_size.width(), pref.width()));
    height = std::max(0, std::min(max_size.height(), pref.height()));
  }
  return gfx::Rect(40 + (root_controller_->window_count() % 4) * 40,
                   40 + (root_controller_->window_count() % 4) * 40, width,
                   height);
}

gfx::Rect WindowManager::GetMaximizedWindowBounds() const {
  DCHECK(root_controller_);
  return gfx::Rect(root_controller_->root()->bounds().size());
}

void WindowManager::OnTreeChanging(const TreeChangeParams& params) {
  DCHECK(root_controller_);
  if (params.old_parent == params.receiver &&
      root_controller_->WindowIsContainer(params.old_parent))
    disconnected_app_handler_.Remove(params.target);

  if (params.new_parent == params.receiver &&
      root_controller_->WindowIsContainer(params.new_parent))
    disconnected_app_handler_.Add(params.target);

  mus::WindowTracker::OnTreeChanging(params);
}

void WindowManager::SetWindowManagerClient(mus::WindowManagerClient* client) {
  window_manager_client_ = client;
}

bool WindowManager::OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) {
  // TODO(sky): this indirectly sets bounds, which is against what
  // OnWmSetBounds() recommends doing. Remove that restriction, or fix this.
  WmWindowMus::Get(window)->SetBounds(*bounds);
  *bounds = window->bounds();
  return true;
}

bool WindowManager::OnWmSetProperty(
    mus::Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  // TODO(sky): constrain this to set of keys we know about, and allowed
  // values.
  return name == mus::mojom::WindowManager::kShowState_Property ||
         name == mus::mojom::WindowManager::kPreferredSize_Property ||
         name == mus::mojom::WindowManager::kResizeBehavior_Property ||
         name == mus::mojom::WindowManager::kWindowAppIcon_Property ||
         name == mus::mojom::WindowManager::kWindowTitle_Property;
}

mus::Window* WindowManager::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return NewTopLevelWindow(properties);
}

void WindowManager::OnAccelerator(uint32_t id, const ui::Event& event) {
  root_controller_->OnAccelerator(id, std::move(event));
}

void WindowManager::ScreenlockStateChanged(bool locked) {
  // Hide USER_PRIVATE_CONTAINER windows when the screen is locked.
  mus::Window* window = root_controller_->GetWindowForContainer(
      mash::wm::mojom::Container::USER_PRIVATE);
  window->SetVisible(!locked);
}

}  // namespace wm
}  // namespace mash
