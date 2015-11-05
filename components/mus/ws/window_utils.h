// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_UTILS_H_
#define COMPONENTS_MUS_WS_WINDOW_UTILS_H_

#include <vector>

namespace mus {
namespace ws {

class ServerWindow;

// Convenience functions that get the TransientWindowManager for the window and
// redirect appropriately. These are preferable to calling functions on
// TransientWindowManager as they handle the appropriate nullptr checks.
ServerWindow* GetTransientParent(ServerWindow* window);

const ServerWindow* GetTransientParent(const ServerWindow* window);

const std::vector<ServerWindow*>& GetTransientChildren(
    const ServerWindow* window);

void AddTransientChild(ServerWindow* parent, ServerWindow* child);

void RemoveTransientChild(ServerWindow* parent, ServerWindow* child);

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
bool HasTransientAncestor(const ServerWindow* window,
                          const ServerWindow* ancestor);

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_UTILS_H_
