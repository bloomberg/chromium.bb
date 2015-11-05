// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/transient_window_stacking_client.h"

#include <algorithm>

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/transient_window_manager.h"
#include "components/mus/ws/window_utils.h"

namespace mus {
namespace ws {

namespace {

// Populates |ancestors| with all transient ancestors of |window| that are
// siblings of |window|. Returns true if any ancestors were found, false if not.
bool GetAllTransientAncestors(ServerWindow* window,
                              ServerWindow::Windows* ancestors) {
  ServerWindow* parent = window->parent();
  for (; window; window = GetTransientParent(window)) {
    if (window->parent() == parent)
      ancestors->push_back(window);
  }
  return !ancestors->empty();
}

// Replaces |window1| and |window2| with their possible transient ancestors that
// are still siblings (have a common transient parent).  |window1| and |window2|
// are not modified if such ancestors cannot be found.
void FindCommonTransientAncestor(ServerWindow** window1,
                                 ServerWindow** window2) {
  DCHECK(window1);
  DCHECK(window2);
  DCHECK(*window1);
  DCHECK(*window2);
  // Assemble chains of ancestors of both windows.
  ServerWindow::Windows ancestors1;
  ServerWindow::Windows ancestors2;
  if (!GetAllTransientAncestors(*window1, &ancestors1) ||
      !GetAllTransientAncestors(*window2, &ancestors2)) {
    return;
  }
  // Walk the two chains backwards and look for the first difference.
  ServerWindow::Windows::reverse_iterator it1 = ancestors1.rbegin();
  ServerWindow::Windows::reverse_iterator it2 = ancestors2.rbegin();
  for (; it1 != ancestors1.rend() && it2 != ancestors2.rend(); ++it1, ++it2) {
    if (*it1 != *it2) {
      *window1 = *it1;
      *window2 = *it2;
      break;
    }
  }
}

}  // namespace

// static
TransientWindowStackingClient* TransientWindowStackingClient::instance_ = NULL;

TransientWindowStackingClient::TransientWindowStackingClient() {
  instance_ = this;
}

TransientWindowStackingClient::~TransientWindowStackingClient() {
  if (instance_ == this)
    instance_ = NULL;
}

bool TransientWindowStackingClient::AdjustStacking(
    ServerWindow** child,
    ServerWindow** target,
    mojom::OrderDirection* direction) {
  const TransientWindowManager* transient_manager =
      TransientWindowManager::Get(static_cast<const ServerWindow*>(*child));
  if (transient_manager && transient_manager->IsStackingTransient(*target))
    return true;

  // For windows that have transient children stack the transient ancestors that
  // are siblings. This prevents one transient group from being inserted in the
  // middle of another.
  FindCommonTransientAncestor(child, target);

  // When stacking above skip to the topmost transient descendant of the target.
  if (*direction == mojom::ORDER_DIRECTION_ABOVE &&
      !HasTransientAncestor(*child, *target)) {
    const ServerWindow::Windows& siblings((*child)->parent()->children());
    size_t target_i =
        std::find(siblings.begin(), siblings.end(), *target) - siblings.begin();
    while (target_i + 1 < siblings.size() &&
           HasTransientAncestor(siblings[target_i + 1], *target)) {
      ++target_i;
    }
    *target = siblings[target_i];
  }

  return *child != *target;
}

}  // namespace ws
}  // namespace mus
