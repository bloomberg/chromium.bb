// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_factory_registry.h"

#include "components/mus/ws/user_id_tracker_observer.h"
#include "components/mus/ws/window_manager_factory_registry_observer.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "components/mus/ws/window_server.h"

namespace mus {
namespace ws {

WindowManagerFactoryRegistry::WindowManagerFactoryRegistry(
    WindowServer* window_server,
    UserIdTracker* id_tracker)
    : id_tracker_(id_tracker), window_server_(window_server) {
  id_tracker_->AddObserver(this);
}

WindowManagerFactoryRegistry::~WindowManagerFactoryRegistry() {
  id_tracker_->RemoveObserver(this);
}

void WindowManagerFactoryRegistry::Register(
    const UserId& user_id,
    mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request) {
  if (ContainsServiceForUser(user_id))
    return;

  std::unique_ptr<WindowManagerFactoryService> service(
      new WindowManagerFactoryService(this, user_id, std::move(request)));
  AddServiceImpl(std::move(service));
}

std::vector<WindowManagerFactoryService*>
WindowManagerFactoryRegistry::GetServices() {
  std::vector<WindowManagerFactoryService*> result;
  for (auto& service_ptr : services_)
    result.push_back(service_ptr.get());
  return result;
}

void WindowManagerFactoryRegistry::AddObserver(
    WindowManagerFactoryRegistryObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowManagerFactoryRegistry::RemoveObserver(
    WindowManagerFactoryRegistryObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowManagerFactoryRegistry::AddServiceImpl(
    std::unique_ptr<WindowManagerFactoryService> service) {
  services_.push_back(std::move(service));
}

bool WindowManagerFactoryRegistry::ContainsServiceForUser(
    const UserId& user_id) const {
  for (auto& service_ptr : services_) {
    if (service_ptr->user_id() == user_id) {
      LOG(ERROR) << "WindowManagerFactoryService already registered for "
                 << user_id;
      return true;
    }
  }
  return false;
}

void WindowManagerFactoryRegistry::OnWindowManagerFactoryConnectionLost(
    WindowManagerFactoryService* service) {
  for (auto it = services_.begin(); it != services_.end(); ++it) {
    if (it->get() == service) {
      services_.erase(it);
      return;
    }
  }
}

void WindowManagerFactoryRegistry::OnWindowManagerFactorySet(
    WindowManagerFactoryService* service) {
  DCHECK(service->window_manager_factory());
  const bool is_first_valid_factory = !got_valid_factory_;
  got_valid_factory_ = true;
  FOR_EACH_OBSERVER(WindowManagerFactoryRegistryObserver, observers_,
                    OnWindowManagerFactorySet(service));

  // Notify after other observers as WindowServer triggers other
  // observers being added, which will have already processed the add.
  if (is_first_valid_factory)
    window_server_->OnFirstWindowManagerFactorySet();
}

void WindowManagerFactoryRegistry::OnActiveUserIdChanged(
    const UserId& previously_active_id,
    const UserId& active_id) {}

void WindowManagerFactoryRegistry::OnUserIdAdded(const UserId& id) {}

void WindowManagerFactoryRegistry::OnUserIdRemoved(const UserId& id) {
  for (auto iter = services_.begin(); iter != services_.end(); ++iter) {
    if ((*iter)->user_id() == id) {
      services_.erase(iter);
      return;
    }
  }
}

}  // namespace ws
}  // namespace mus
