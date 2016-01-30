// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
class ApplicationImpl;
}

namespace mus {

class WindowManagerDelegate;
class WindowTreeDelegate;

// Uses |factory| to create a new |host|, providing the supplied |host_client|
// which may be null. |delegate| must not be null.
void CreateWindowTreeHost(mojom::WindowTreeHostFactory* factory,
                          mojom::WindowTreeHostClientPtr host_client,
                          WindowTreeDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate);

// Creates a single host with no client by connecting to the window manager
// application. Useful only for tests and trivial UIs.
void CreateSingleWindowTreeHost(
    mojo::ApplicationImpl* app,
    mojom::WindowTreeHostClientPtr host_client,
    WindowTreeDelegate* delegate,
    mojom::WindowTreeHostPtr* host,
    WindowManagerDelegate* window_manager_delegate);

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_
