// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/user_window_controller_impl.h"

#include "components/mus/public/cpp/window.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/window_manager_application.h"

namespace mash {
namespace wm {

UserWindowControllerImpl::UserWindowControllerImpl() : state_(nullptr) {}

UserWindowControllerImpl::~UserWindowControllerImpl() {
  if (state_)
    GetUserWindowContainer()->RemoveObserver(this);
}

void UserWindowControllerImpl::Initialize(WindowManagerApplication* state) {
  DCHECK(state);
  DCHECK(!state_);
  state_ = state;
  GetUserWindowContainer()->AddObserver(this);
}

mus::Window* UserWindowControllerImpl::GetUserWindowContainer() const {
  return state_->GetWindowForContainer(mojom::CONTAINER_USER_WINDOWS);
}

void UserWindowControllerImpl::OnTreeChanging(const TreeChangeParams& params) {
  DCHECK(state_);
  if (user_window_observer_) {
    mus::Window* user_window_container = GetUserWindowContainer();
    if (params.new_parent == user_window_container)
      user_window_observer_->OnUserWindowAdded(params.target->id());
    else if (params.old_parent == user_window_container)
      user_window_observer_->OnUserWindowRemoved(params.target->id());
  }
}

void UserWindowControllerImpl::AddUserWindowObserver(
    mash::wm::mojom::UserWindowObserverPtr observer) {
  // TODO(msw): Support multiple observers.
  user_window_observer_ = std::move(observer);

  const mus::Window::Children& windows = GetUserWindowContainer()->children();
  mojo::Array<uint32_t> user_windows =
      mojo::Array<uint32_t>::New(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    user_windows[i] = windows[i]->id();
  user_window_observer_->OnUserWindowObserverAdded(std::move(user_windows));
}

void UserWindowControllerImpl::FocusUserWindow(uint32_t window_id) {
  mus::Window* window = GetUserWindowContainer()->GetChildById(window_id);
  if (window)
    window->SetFocus();
}

}  // namespace wm
}  // namespace mash
