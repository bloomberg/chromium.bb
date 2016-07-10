// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/user_window_controller_impl.h"

#include "ash/common/shell_window_ids.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/property_util.h"
#include "ash/mus/root_window_controller.h"
#include "ash/public/interfaces/container.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/resources/grit/ui_resources.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(uint32_t)

namespace {

// Key used for storing identifier sent to clients for windows.
MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(uint32_t, kUserWindowIdKey, 0u);

}  // namespace

namespace ash {
namespace mus {

namespace {

// Returns |window|, or an ancestor thereof, parented to |container|, or null.
::ui::Window* GetTopLevelWindow(::ui::Window* window, ::ui::Window* container) {
  while (window && window->parent() != container)
    window = window->parent();
  return window;
}

// Get a UserWindow struct from a ui::Window.
mojom::UserWindowPtr GetUserWindow(::ui::Window* window) {
  mojom::UserWindowPtr user_window(mojom::UserWindow::New());
  DCHECK_NE(0u, window->GetLocalProperty(kUserWindowIdKey));
  user_window->window_id = window->GetLocalProperty(kUserWindowIdKey);
  user_window->window_title = mojo::String::From(GetWindowTitle(window));
  user_window->window_app_icon = GetWindowAppIcon(window);
  user_window->window_app_id = mojo::String::From(GetAppID(window));
  user_window->ignored_by_shelf = GetWindowIgnoredByShelf(window);
  ::ui::Window* focused = window->window_tree()->GetFocusedWindow();
  focused = GetTopLevelWindow(focused, window->parent());
  user_window->window_has_focus = focused == window;
  return user_window;
}

}  // namespace

// Observes property changes on user windows. UserWindowControllerImpl uses
// this separate observer to avoid observing duplicate tree change
// notifications.
class WindowPropertyObserver : public ::ui::WindowObserver {
 public:
  explicit WindowPropertyObserver(UserWindowControllerImpl* controller)
      : controller_(controller) {}
  ~WindowPropertyObserver() override {}

 private:
  // ui::WindowObserver:
  void OnWindowSharedPropertyChanged(
      ::ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    if (!controller_->user_window_observer())
      return;
    if (name == ::ui::mojom::WindowManager::kWindowTitle_Property) {
      controller_->user_window_observer()->OnUserWindowTitleChanged(
          window->GetLocalProperty(kUserWindowIdKey),
          mojo::String::From(GetWindowTitle(window)));
    } else if (name == ::ui::mojom::WindowManager::kWindowAppIcon_Property) {
      controller_->user_window_observer()->OnUserWindowAppIconChanged(
          window->GetLocalProperty(kUserWindowIdKey),
          new_data ? mojo::Array<uint8_t>::From(*new_data)
                   : mojo::Array<uint8_t>());
    }
  }

  UserWindowControllerImpl* controller_;
  DISALLOW_COPY_AND_ASSIGN(WindowPropertyObserver);
};

UserWindowControllerImpl::UserWindowControllerImpl()
    : root_controller_(nullptr) {}

UserWindowControllerImpl::~UserWindowControllerImpl() {
  if (!root_controller_)
    return;

  ::ui::Window* user_container = GetUserWindowContainer();
  if (!user_container)
    return;

  RemoveObservers(user_container);
}

void UserWindowControllerImpl::Initialize(
    RootWindowController* root_controller) {
  DCHECK(root_controller);
  DCHECK(!root_controller_);
  root_controller_ = root_controller;
  GetUserWindowContainer()->AddObserver(this);
  GetUserWindowContainer()->window_tree()->AddObserver(this);
  window_property_observer_.reset(new WindowPropertyObserver(this));
  for (::ui::Window* window : GetUserWindowContainer()->children()) {
    AssignIdIfNecessary(window);
    window->AddObserver(window_property_observer_.get());
  }
}

void UserWindowControllerImpl::AssignIdIfNecessary(::ui::Window* window) {
  if (window->GetLocalProperty(kUserWindowIdKey) == 0u)
    window->SetLocalProperty(kUserWindowIdKey, next_id_++);
}

void UserWindowControllerImpl::RemoveObservers(::ui::Window* user_container) {
  user_container->RemoveObserver(this);
  user_container->window_tree()->RemoveObserver(this);
  for (auto* iter : user_container->children())
    iter->RemoveObserver(window_property_observer_.get());
}

::ui::Window* UserWindowControllerImpl::GetUserWindowById(uint32_t id) {
  for (::ui::Window* window : GetUserWindowContainer()->children()) {
    if (window->GetLocalProperty(kUserWindowIdKey) == id)
      return window;
  }
  return nullptr;
}

::ui::Window* UserWindowControllerImpl::GetUserWindowContainer() const {
  WmWindowMus* window = root_controller_->GetWindowByShellWindowId(
      kShellWindowId_DefaultContainer);
  return window ? window->mus_window() : nullptr;
}

void UserWindowControllerImpl::OnTreeChanging(const TreeChangeParams& params) {
  DCHECK(root_controller_);
  if (params.new_parent == GetUserWindowContainer()) {
    params.target->AddObserver(window_property_observer_.get());
    AssignIdIfNecessary(params.target);
    if (user_window_observer_)
      user_window_observer_->OnUserWindowAdded(GetUserWindow(params.target));
  } else if (params.old_parent == GetUserWindowContainer()) {
    params.target->RemoveObserver(window_property_observer_.get());
    if (user_window_observer_)
      user_window_observer_->OnUserWindowRemoved(
          params.target->GetLocalProperty(kUserWindowIdKey));
  }
}

void UserWindowControllerImpl::OnWindowDestroying(::ui::Window* window) {
  if (window == GetUserWindowContainer())
    RemoveObservers(window);
}

void UserWindowControllerImpl::OnWindowTreeFocusChanged(
    ::ui::Window* gained_focus,
    ::ui::Window* lost_focus) {
  if (!user_window_observer_)
    return;

  // Treat focus in the user window hierarchy as focus of the top-level window.
  gained_focus = GetTopLevelWindow(gained_focus, GetUserWindowContainer());
  lost_focus = GetTopLevelWindow(lost_focus, GetUserWindowContainer());
  if (gained_focus == lost_focus)
    return;

  if (lost_focus) {
    user_window_observer_->OnUserWindowFocusChanged(
        lost_focus->GetLocalProperty(kUserWindowIdKey), false);
  }
  if (gained_focus) {
    user_window_observer_->OnUserWindowFocusChanged(
        gained_focus->GetLocalProperty(kUserWindowIdKey), true);
  }
}

void UserWindowControllerImpl::AddUserWindowObserver(
    mojom::UserWindowObserverPtr observer) {
  // TODO(msw): Support multiple observers.
  user_window_observer_ = std::move(observer);

  const ::ui::Window::Children& windows = GetUserWindowContainer()->children();
  mojo::Array<mojom::UserWindowPtr> user_windows =
      mojo::Array<mojom::UserWindowPtr>::New(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    user_windows[i] = GetUserWindow(windows[i]);
  user_window_observer_->OnUserWindowObserverAdded(std::move(user_windows));
}

void UserWindowControllerImpl::ActivateUserWindow(uint32_t window_id) {
  ::ui::Window* window = GetUserWindowById(window_id);
  if (window) {
    window->SetVisible(true);
    window->SetFocus();
  }
}

}  // namespace mus
}  // namespace ash
