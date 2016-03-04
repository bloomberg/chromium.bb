// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_BINDING_H_
#define COMPONENTS_MUS_WS_DISPLAY_BINDING_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/display.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ConnectionManager;
class ServerWindow;
class WindowTreeImpl;

// DisplayBinding manages the binding between a Display and it's mojo clients.
// DisplayBinding is used when a Display is created via a
// WindowTreeHostFactory.
//
// DisplayBinding is owned by Display.
class DisplayBinding {
 public:
  virtual ~DisplayBinding() {}

  virtual WindowTreeImpl* CreateWindowTree(ServerWindow* root) = 0;
};

// Live implementation of DisplayBinding.
class DisplayBindingImpl : public DisplayBinding {
 public:
  DisplayBindingImpl(mojom::WindowTreeHostRequest request,
                     Display* display,
                     mojom::WindowTreeClientPtr client,
                     ConnectionManager* connection_manager);
  ~DisplayBindingImpl() override;

 private:
  // DisplayBinding:
  WindowTreeImpl* CreateWindowTree(ServerWindow* root) override;

  ConnectionManager* connection_manager_;
  mojo::Binding<mojom::WindowTreeHost> binding_;
  mojom::WindowTreeClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(DisplayBindingImpl);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_BINDING_H_
