// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_window_tree_factory_set.h"

#include "components/mus/ws/user_id_tracker_observer.h"
#include "components/mus/ws/window_manager_window_tree_factory.h"
#include "components/mus/ws/window_manager_window_tree_factory_set_observer.h"
#include "components/mus/ws/window_server.h"

namespace mus {
namespace ws {

WindowManagerWindowTreeFactorySet::WindowManagerWindowTreeFactorySet(
    WindowServer* window_server,
    UserIdTracker* id_tracker)
    : id_tracker_(id_tracker), window_server_(window_server) {
  id_tracker_->AddObserver(this);
}

WindowManagerWindowTreeFactorySet::~WindowManagerWindowTreeFactorySet() {
  id_tracker_->RemoveObserver(this);
}

WindowManagerWindowTreeFactory* WindowManagerWindowTreeFactorySet::Add(
    const UserId& user_id,
    mojo::InterfaceRequest<mojom::WindowManagerWindowTreeFactory> request) {
  if (ContainsFactoryForUser(user_id)) {
    DVLOG(1) << "can only have one factory per user";
    return nullptr;
  }

  std::unique_ptr<WindowManagerWindowTreeFactory> factory_ptr(
      new WindowManagerWindowTreeFactory(this, user_id, std::move(request)));
  WindowManagerWindowTreeFactory* factory = factory_ptr.get();
  factories_.push_back(std::move(factory_ptr));
  return factory;
}

void WindowManagerWindowTreeFactorySet::DeleteFactoryAssociatedWithTree(
    WindowTree* window_tree) {
  for (auto it = factories_.begin(); it != factories_.end(); ++it) {
    if ((*it)->window_tree() == window_tree) {
      factories_.erase(it);
      return;
    }
  }
}

std::vector<WindowManagerWindowTreeFactory*>
WindowManagerWindowTreeFactorySet::GetFactories() {
  std::vector<WindowManagerWindowTreeFactory*> result;
  for (auto& factory : factories_)
    result.push_back(factory.get());
  return result;
}

void WindowManagerWindowTreeFactorySet::AddObserver(
    WindowManagerWindowTreeFactorySetObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowManagerWindowTreeFactorySet::RemoveObserver(
    WindowManagerWindowTreeFactorySetObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WindowManagerWindowTreeFactorySet::ContainsFactoryForUser(
    const UserId& user_id) const {
  for (auto& factory : factories_) {
    if (factory->user_id() == user_id)
      return true;
  }
  return false;
}

void WindowManagerWindowTreeFactorySet::OnWindowManagerWindowTreeFactoryReady(
    WindowManagerWindowTreeFactory* factory) {
  const bool is_first_valid_factory = !got_valid_factory_;
  got_valid_factory_ = true;
  FOR_EACH_OBSERVER(WindowManagerWindowTreeFactorySetObserver, observers_,
                    OnWindowManagerWindowTreeFactoryReady(factory));

  // Notify after other observers as WindowServer triggers other
  // observers being added, which will have already processed the add.
  if (is_first_valid_factory)
    window_server_->OnFirstWindowManagerWindowTreeFactoryReady();
}

void WindowManagerWindowTreeFactorySet::OnActiveUserIdChanged(
    const UserId& previously_active_id,
    const UserId& active_id) {}

void WindowManagerWindowTreeFactorySet::OnUserIdAdded(const UserId& id) {}

void WindowManagerWindowTreeFactorySet::OnUserIdRemoved(const UserId& id) {
  for (auto iter = factories_.begin(); iter != factories_.end(); ++iter) {
    if ((*iter)->user_id() == id) {
      factories_.erase(iter);
      return;
    }
  }
}

}  // namespace ws
}  // namespace mus
