// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/transient_window_manager.h"

#include "base/auto_reset.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/transient_window_observer.h"

namespace mus {
namespace ws {

namespace {
using TransientWindowManagerMap =
    std::map<const ServerWindow*, TransientWindowManager*>;

static base::LazyInstance<TransientWindowManagerMap>
    transient_window_manager_map = LAZY_INSTANCE_INITIALIZER;
}

TransientWindowManager::~TransientWindowManager() {}

// static
TransientWindowManager* TransientWindowManager::GetOrCreate(
    ServerWindow* window) {
  TransientWindowManagerMap* map = transient_window_manager_map.Pointer();
  auto it = map->find(window);
  if (it != map->end())
    return it->second;
  TransientWindowManager* manager = new TransientWindowManager(window);
  transient_window_manager_map.Get().insert(std::make_pair(window, manager));
  return manager;
}

// static
TransientWindowManager* TransientWindowManager::Get(ServerWindow* window) {
  TransientWindowManagerMap* map = transient_window_manager_map.Pointer();
  auto it = map->find(window);
  return it == map->end() ? nullptr : it->second;
}

void TransientWindowManager::AddObserver(TransientWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void TransientWindowManager::RemoveObserver(TransientWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TransientWindowManager::AddTransientChild(ServerWindow* child) {
  TransientWindowManager* child_manager = GetOrCreate(child);
  if (child_manager->transient_parent_)
    Get(child_manager->transient_parent_)->RemoveTransientChild(child);
  DCHECK(std::find(transient_children_.begin(), transient_children_.end(),
                   child) == transient_children_.end());
  transient_children_.push_back(child);
  child_manager->transient_parent_ = window_;

  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildAdded(window_, child));
}

void TransientWindowManager::RemoveTransientChild(ServerWindow* child) {
  Windows::iterator i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  TransientWindowManager* child_manager = Get(child);
  DCHECK(child_manager);
  DCHECK_EQ(window_, child_manager->transient_parent_);
  child_manager->transient_parent_ = nullptr;

  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildRemoved(window_, child));
}

TransientWindowManager::TransientWindowManager(ServerWindow* window)
    : window_(window),
      transient_parent_(nullptr) {
  window_->AddObserver(this);
}

void TransientWindowManager::OnWillDestroyWindow(ServerWindow* window) {
  // Removes ourselves from our transient parent (if it hasn't been done by the
  // RootWindow).
  if (transient_parent_) {
    TransientWindowManager::Get(transient_parent_)
        ->RemoveTransientChild(window_);
  }

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  Windows transient_children(transient_children_);
  STLDeleteElements(&transient_children);
  DCHECK(transient_children_.empty());
}

void TransientWindowManager::OnWindowDestroyed(ServerWindow* window) {
  transient_window_manager_map.Get().erase(window_);
  delete this;
}

}  // namespace ws
}  // namespace mus
