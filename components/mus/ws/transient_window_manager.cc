// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/transient_window_manager.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/transient_window_observer.h"
#include "components/mus/ws/window_utils.h"

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
  return const_cast<TransientWindowManager*>(
      Get(const_cast<const ServerWindow*>(window)));
}

const TransientWindowManager* TransientWindowManager::Get(
    const ServerWindow* window) {
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

  // Restack |child| properly above its transient parent, if they share the same
  // parent.
  if (child->parent() == window_->parent())
    RestackTransientDescendants();

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

  // If |child| and its former transient parent share the same parent, |child|
  // should be restacked properly so it is not among transient children of its
  // former parent, anymore.
  if (window_->parent() == child->parent())
    RestackTransientDescendants();

  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildRemoved(window_, child));
}

bool TransientWindowManager::IsStackingTransient(
    const ServerWindow* target) const {
  return stacking_target_ == target;
}

TransientWindowManager::TransientWindowManager(ServerWindow* window)
    : window_(window), transient_parent_(nullptr), stacking_target_(nullptr) {
  window_->AddObserver(this);
}

void TransientWindowManager::RestackTransientDescendants() {
  ServerWindow* parent = window_->parent();
  if (!parent)
    return;

  // stack any transient children that share the same parent to be in front of
  // |window_|. the existing stacking order is preserved by iterating backwards
  // and always stacking on top.
  Windows children(parent->children());
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if ((*it) != window_ && HasTransientAncestor(*it, window_)) {
      TransientWindowManager* descendant_manager = GetOrCreate(*it);
      base::AutoReset<ServerWindow*> resetter(
          &descendant_manager->stacking_target_, window_);
      parent->Reorder((*it), window_, mojom::ORDER_DIRECTION_ABOVE);
    }
  }
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

void TransientWindowManager::OnWindowStackingChanged(ServerWindow* window) {
  // Do nothing if we initiated the stacking change.
  const TransientWindowManager* transient_manager = Get(window);
  if (transient_manager && transient_manager->stacking_target_) {
    Windows::const_iterator window_i =
        std::find(window->parent()->children().begin(),
                  window->parent()->children().end(), window);
    DCHECK(window_i != window->parent()->children().end());
    if (window_i != window->parent()->children().begin() &&
        (*(window_i - 1) == transient_manager->stacking_target_))
      return;
  }

  RestackTransientDescendants();
}

void TransientWindowManager::OnWindowDestroyed(ServerWindow* window) {
  transient_window_manager_map.Get().erase(window_);
  delete this;
}

void TransientWindowManager::OnWindowHierarchyChanged(
    ServerWindow* window,
    ServerWindow* new_parent,
    ServerWindow* old_parent) {
  DCHECK_EQ(window_, window);
  // Stack |window| properly if it is transient child of a sibling.
  ServerWindow* transient_parent = GetTransientParent(window);
  if (transient_parent && transient_parent->parent() == new_parent) {
    TransientWindowManager* transient_parent_manager = Get(transient_parent);
    transient_parent_manager->RestackTransientDescendants();
  }
}

}  // namespace ws
}  // namespace mus
