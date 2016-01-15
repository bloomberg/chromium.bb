// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/user_window_controller_impl.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/window_manager_application.h"
#include "mojo/common/common_type_converters.h"

namespace mash {
namespace wm {
namespace {

mojo::String GetWindowTitle(mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kWindowTitle_Property)) {
    return mojo::String::From(window->GetSharedProperty<base::string16>(
               mus::mojom::WindowManager::kWindowTitle_Property));
  }
  return mojo::String(std::string());
}

mojom::UserWindowPtr GetUserWindow(mus::Window* window) {
  mojom::UserWindowPtr user_window(mojom::UserWindow::New());
  user_window->window_id = window->id();
  user_window->window_title = GetWindowTitle(window);
  return user_window;
}

}  // namespace

// Observes title changes on user windows. UserWindowControllerImpl uses this
// separate observer to avoid observing duplicate tree change notifications.
class WindowTitleObserver : public mus::WindowObserver {
 public:
  explicit WindowTitleObserver(UserWindowControllerImpl* controller)
      : controller_(controller) {}
  ~WindowTitleObserver() override {}

 private:
  // mus::WindowObserver:
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    if (controller_->user_window_observer() &&
        name == mus::mojom::WindowManager::kWindowTitle_Property) {
      controller_->user_window_observer()->OnUserWindowTitleChanged(
          window->id(), GetWindowTitle(window));
    }
  }

  UserWindowControllerImpl* controller_;
  DISALLOW_COPY_AND_ASSIGN(WindowTitleObserver);
};

UserWindowControllerImpl::UserWindowControllerImpl() : state_(nullptr) {}

UserWindowControllerImpl::~UserWindowControllerImpl() {
  if (state_) {
    GetUserWindowContainer()->RemoveObserver(this);
    for (auto iter : GetUserWindowContainer()->children())
      iter->RemoveObserver(window_title_observer_.get());
  }
}

void UserWindowControllerImpl::Initialize(WindowManagerApplication* state) {
  DCHECK(state);
  DCHECK(!state_);
  state_ = state;
  GetUserWindowContainer()->AddObserver(this);
  window_title_observer_.reset(new WindowTitleObserver(this));
  for (auto iter : GetUserWindowContainer()->children())
    iter->AddObserver(window_title_observer_.get());
}

mus::Window* UserWindowControllerImpl::GetUserWindowContainer() const {
  return state_->GetWindowForContainer(mojom::CONTAINER_USER_WINDOWS);
}

void UserWindowControllerImpl::OnTreeChanging(const TreeChangeParams& params) {
  DCHECK(state_);
  if (user_window_observer_) {
    mus::Window* user_window_container = GetUserWindowContainer();
    if (params.new_parent == user_window_container) {
      params.target->AddObserver(window_title_observer_.get());
      user_window_observer_->OnUserWindowAdded(GetUserWindow(params.target));
    } else if (params.old_parent == user_window_container) {
      params.target->RemoveObserver(window_title_observer_.get());
      user_window_observer_->OnUserWindowRemoved(params.target->id());
    }
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
  mus::Window* window = GetUserWindowContainer()->GetChildById(window_id);
  if (window)
    window->SetFocus();
}

}  // namespace wm
}  // namespace mash
