// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace aura {
namespace client {
class WindowTreeClient;
}
class WindowTreeHost;
}

namespace gfx {
class Screen;
}

namespace mojo {

class Shell;

namespace services {
namespace view_manager {

class RootNodeManager;

// RootViewManager binds the root node to an actual display.
class MOJO_VIEW_MANAGER_EXPORT RootViewManager {
 public:
  RootViewManager(Shell* shell, RootNodeManager* root_node);
  virtual ~RootViewManager();

  // See description above field for details.
  bool in_setup() const { return in_setup_; }

 private:
  void OnCompositorCreated();

  Shell* shell_;
  RootNodeManager* root_node_manager_;

  GLES2Initializer gles_initializer_;

  // Returns true if adding the root node's window to |window_tree_host_|.
  bool in_setup_;

  scoped_ptr<gfx::Screen> screen_;
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;

  DISALLOW_COPY_AND_ASSIGN(RootViewManager);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
