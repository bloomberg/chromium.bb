// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_utils.h"

#include "components/mus/ws/transient_window_manager.h"

namespace mus {
namespace ws {

ServerWindow* GetTransientParent(ServerWindow* window) {
  return const_cast<ServerWindow*>(
      GetTransientParent(const_cast<const ServerWindow*>(window)));
}

const ServerWindow* GetTransientParent(const ServerWindow* window) {
  const TransientWindowManager* manager = TransientWindowManager::Get(window);
  return manager ? manager->transient_parent() : NULL;
}

const std::vector<ServerWindow*>& GetTransientChildren(
    const ServerWindow* window) {
  const TransientWindowManager* manager = TransientWindowManager::Get(window);
  if (manager)
    return manager->transient_children();

  static std::vector<ServerWindow*>* shared = new std::vector<ServerWindow*>;
  return *shared;
}

void AddTransientChild(ServerWindow* parent, ServerWindow* child) {
  TransientWindowManager::GetOrCreate(parent)->AddTransientChild(child);
}

void RemoveTransientChild(ServerWindow* parent, ServerWindow* child) {
  TransientWindowManager::Get(parent)->RemoveTransientChild(child);
}

bool HasTransientAncestor(const ServerWindow* window,
                          const ServerWindow* ancestor) {
  const ServerWindow* transient_parent = GetTransientParent(window);
  if (transient_parent == ancestor)
    return true;
  return transient_parent ? HasTransientAncestor(transient_parent, ancestor)
                          : false;
}

}  // namespace ws
}  // namespace mus
