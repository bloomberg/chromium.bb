// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/user_window_controller_impl.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/wm/property_util.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/root_window_controller.h"
#include "mojo/common/common_type_converters.h"
#include "ui/resources/grit/ui_resources.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(uint32_t);

namespace mash {
namespace wm {

// Key used for storing identifier sent to clients for windows.
MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(uint32_t, kUserWindowIdKey, 0u);

namespace {

// Returns |window|, or an ancestor thereof, parented to |container|, or null.
mus::Window* GetTopLevelWindow(mus::Window* window, mus::Window* container) {
  while (window && window->parent() != container)
    window = window->parent();
  return window;
}

// Get a UserWindow struct from a mus::Window.
mojom::UserWindowPtr GetUserWindow(mus::Window* window) {
  mojom::UserWindowPtr user_window(mojom::UserWindow::New());
  DCHECK_NE(0u, window->GetLocalProperty(kUserWindowIdKey));
  user_window->window_id = window->GetLocalProperty(kUserWindowIdKey);
  user_window->window_title = mojo::String::From(GetWindowTitle(window));
  user_window->window_app_icon = GetWindowAppIcon(window);
  user_window->window_app_id = mojo::String::From(GetAppID(window));
  user_window->ignored_by_shelf = GetWindowIgnoredByShelf(window);
  mus::Window* focused = window->connection()->GetFocusedWindow();
  focused = GetTopLevelWindow(focused, window->parent());
  user_window->window_has_focus = focused == window;
  return user_window;
}

}  // namespace

// Observes property changes on user windows. UserWindowControllerImpl uses
// this separate observer to avoid observing duplicate tree change
// notifications.
class WindowPropertyObserver : public mus::WindowObserver {
 public:
  explicit WindowPropertyObserver(UserWindowControllerImpl* controller)
      : controller_(controller) {}
  ~WindowPropertyObserver() override {}

 private:
  // mus::WindowObserver:
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    if (!controller_->user_window_observer())
      return;
    if (name == mus::mojom::WindowManager::kWindowTitle_Property) {
      controller_->user_window_observer()->OnUserWindowTitleChanged(
          window->GetLocalProperty(kUserWindowIdKey),
          mojo::String::From(GetWindowTitle(window)));
    } else if (name == mus::mojom::WindowManager::kWindowAppIcon_Property) {
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

  // TODO(msw): should really listen for user window container being destroyed
  // and cleanup there.
  mus::Window* user_container = GetUserWindowContainer();
  if (!user_container)
    return;

  user_container->RemoveObserver(this);
  for (auto iter : user_container->children())
    iter->RemoveObserver(window_property_observer_.get());
}

void UserWindowControllerImpl::Initialize(
    RootWindowController* root_controller) {
  DCHECK(root_controller);
  DCHECK(!root_controller_);
  root_controller_ = root_controller;
  GetUserWindowContainer()->AddObserver(this);
  GetUserWindowContainer()->connection()->AddObserver(this);
  window_property_observer_.reset(new WindowPropertyObserver(this));
  for (mus::Window* window : GetUserWindowContainer()->children()) {
    AssignIdIfNecessary(window);
    window->AddObserver(window_property_observer_.get());
  }
}

void UserWindowControllerImpl::AssignIdIfNecessary(mus::Window* window) {
  if (window->GetLocalProperty(kUserWindowIdKey) == 0u)
    window->SetLocalProperty(kUserWindowIdKey, next_id_++);
}

mus::Window* UserWindowControllerImpl::GetUserWindowById(uint32_t id) {
  for (mus::Window* window : GetUserWindowContainer()->children()) {
    if (window->GetLocalProperty(kUserWindowIdKey) == id)
      return window;
  }
  return nullptr;
}

mus::Window* UserWindowControllerImpl::GetUserWindowContainer() const {
  return root_controller_->GetWindowForContainer(
      mojom::Container::USER_WINDOWS);
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

void UserWindowControllerImpl::OnWindowTreeFocusChanged(
    mus::Window* gained_focus,
    mus::Window* lost_focus) {
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

  const mus::Window::Children& windows = GetUserWindowContainer()->children();
  mojo::Array<mojom::UserWindowPtr> user_windows =
      mojo::Array<mojom::UserWindowPtr>::New(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    user_windows[i] = GetUserWindow(windows[i]);
  user_window_observer_->OnUserWindowObserverAdded(std::move(user_windows));
}

void UserWindowControllerImpl::FocusUserWindow(uint32_t window_id) {
  mus::Window* window = GetUserWindowById(window_id);
  if (window)
    window->SetFocus();
}

}  // namespace wm
}  // namespace mash
