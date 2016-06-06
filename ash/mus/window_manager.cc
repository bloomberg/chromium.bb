// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager.h"

#include <stdint.h>

#include <utility>

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/container_finder.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/root_window_controller.h"
#include "ash/public/interfaces/container.mojom.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"

namespace ash {
namespace mus {

WindowManager::WindowManager()
    : root_controller_(nullptr),
      window_manager_client_(nullptr),
      binding_(this) {}

WindowManager::~WindowManager() {}

void WindowManager::Initialize(RootWindowController* root_controller,
                               mash::session::mojom::Session* session) {
  DCHECK(root_controller);
  DCHECK(!root_controller_);
  root_controller_ = root_controller;

  // Observe all the containers so that windows can be added to/removed from the
  // |disconnected_app_handler_|.
  WmWindowMus* root = WmWindowMus::Get(root_controller_->root());
  for (int shell_window_id = kShellWindowId_Min;
       shell_window_id < kShellWindowId_Max; ++shell_window_id) {
    // kShellWindowId_VirtualKeyboardContainer is lazily created.
    // TODO(sky): http://crbug.com/616909 .
    // kShellWindowId_PhantomWindow is not a container, but a window.
    if (shell_window_id == kShellWindowId_VirtualKeyboardContainer ||
        shell_window_id == kShellWindowId_PhantomWindow)
      continue;

// kShellWindowId_MouseCursorContainer is chromeos specific.
#if !defined(OS_CHROMEOS)
    if (shell_window_id == kShellWindowId_MouseCursorContainer)
      continue;
#endif

    WmWindowMus* container = WmWindowMus::AsWmWindowMus(
        root->GetChildByShellWindowId(shell_window_id));
    Add(container->mus_window());

    // Add any pre-existing windows in the container to
    // |disconnected_app_handler_|.
    for (::mus::Window* child : container->mus_window()->children()) {
      if (!root_controller_->WindowIsContainer(child))
        disconnected_app_handler_.Add(child);
    }
  }

  // The insets are roughly what is needed by CustomFrameView. The expectation
  // is at some point we'll write our own NonClientFrameView and get the insets
  // from it.
  ::mus::mojom::FrameDecorationValuesPtr frame_decoration_values =
      ::mus::mojom::FrameDecorationValues::New();
  const gfx::Insets client_area_insets =
      NonClientFrameController::GetPreferredClientAreaInsets();
  frame_decoration_values->normal_client_area_insets = client_area_insets;
  frame_decoration_values->maximized_client_area_insets = client_area_insets;
  frame_decoration_values->max_title_bar_button_width =
      NonClientFrameController::GetMaxTitleBarButtonWidth();
  window_manager_client_->SetFrameDecorationValues(
      std::move(frame_decoration_values));

  if (session)
    session->AddScreenlockStateListener(binding_.CreateInterfacePtrAndBind());
}

::mus::Window* WindowManager::NewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  DCHECK(root_controller_);
  ::mus::Window* root = root_controller_->root();
  DCHECK(root);

  // TODO(sky): panels need a different frame, http:://crbug.com/614362.
  const bool provide_non_client_frame =
      GetWindowType(*properties) == ::mus::mojom::WindowType::WINDOW ||
      GetWindowType(*properties) == ::mus::mojom::WindowType::PANEL;
  if (provide_non_client_frame)
    (*properties)[::mus::mojom::kWaitForUnderlay_Property].clear();

  // TODO(sky): constrain and validate properties before passing to server.
  ::mus::Window* window = root->window_tree()->NewWindow(properties);
  window->SetBounds(CalculateDefaultBounds(window));

  ::mus::Window* container_window = nullptr;
  mojom::Container container;
  if (GetRequestedContainer(window, &container)) {
    container_window = root_controller_->GetWindowForContainer(container);
  } else {
    // TODO(sky): window->bounds() isn't quite right.
    container_window = WmWindowMus::GetMusWindow(
        wm::GetDefaultParent(WmWindowMus::Get(root_controller_->root()),
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

gfx::Rect WindowManager::CalculateDefaultBounds(::mus::Window* window) const {
  if (window->HasSharedProperty(
          ::mus::mojom::WindowManager::kInitialBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        ::mus::mojom::WindowManager::kInitialBounds_Property);
  }

  DCHECK(root_controller_);
  int width, height;
  const gfx::Size pref = GetWindowPreferredSize(window);
  const ::mus::Window* root = root_controller_->root();
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

  ::mus::WindowTracker::OnTreeChanging(params);
}

void WindowManager::SetWindowManagerClient(::mus::WindowManagerClient* client) {
  window_manager_client_ = client;
}

bool WindowManager::OnWmSetBounds(::mus::Window* window, gfx::Rect* bounds) {
  // TODO(sky): this indirectly sets bounds, which is against what
  // OnWmSetBounds() recommends doing. Remove that restriction, or fix this.
  WmWindowMus::Get(window)->SetBounds(*bounds);
  *bounds = window->bounds();
  return true;
}

bool WindowManager::OnWmSetProperty(
    ::mus::Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  // TODO(sky): constrain this to set of keys we know about, and allowed
  // values.
  return name == ::mus::mojom::WindowManager::kShowState_Property ||
         name == ::mus::mojom::WindowManager::kPreferredSize_Property ||
         name == ::mus::mojom::WindowManager::kResizeBehavior_Property ||
         name == ::mus::mojom::WindowManager::kWindowAppIcon_Property ||
         name == ::mus::mojom::WindowManager::kWindowTitle_Property;
}

::mus::Window* WindowManager::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return NewTopLevelWindow(properties);
}

void WindowManager::OnWmClientJankinessChanged(
    const std::set<::mus::Window*>& client_windows,
    bool janky) {
  for (auto window : client_windows)
    SetWindowIsJanky(window, janky);
}

void WindowManager::OnAccelerator(uint32_t id, const ui::Event& event) {
  root_controller_->OnAccelerator(id, std::move(event));
}

void WindowManager::ScreenlockStateChanged(bool locked) {
  WmWindowMus* non_lock_screen_containers_container =
      root_controller_->GetWindowByShellWindowId(
          kShellWindowId_NonLockScreenContainersContainer);
  non_lock_screen_containers_container->mus_window()->SetVisible(!locked);
}

}  // namespace mus
}  // namespace ash
