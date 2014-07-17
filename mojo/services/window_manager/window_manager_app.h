// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "mojo/aura/window_tree_host_mojo_delegate.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"

namespace aura {
class Window;
}

namespace wm {
class ScopedCaptureClient;
}

namespace mojo {

class AuraInit;
class WindowManagerServiceImpl;
class WindowTreeHostMojo;

class WindowManagerApp : public ApplicationDelegate,
                         public view_manager::ViewManagerDelegate,
                         public WindowTreeHostMojoDelegate {
 public:
  WindowManagerApp();
  virtual ~WindowManagerApp();

  void AddConnection(WindowManagerServiceImpl* connection);
  void RemoveConnection(WindowManagerServiceImpl* connection);

  view_manager::Id OpenWindow();
  void SetCapture(view_manager::Id node);

  bool IsReady() const;

 private:
  typedef std::set<WindowManagerServiceImpl*> Connections;

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* impl) MOJO_OVERRIDE;
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) MOJO_OVERRIDE;
  virtual void OnViewManagerDisconnected(
      view_manager::ViewManager* view_manager) MOJO_OVERRIDE;

  // Overridden from WindowTreeHostMojoDelegate:
  virtual void CompositorContentsChanged(const SkBitmap& bitmap) MOJO_OVERRIDE;

  aura::Window* GetWindowForNodeId(view_manager::Id node) const;

  view_manager::ViewManager* view_manager_;
  view_manager::Node* root_;

  scoped_ptr<AuraInit> aura_init_;
  scoped_ptr<WindowTreeHostMojo> window_tree_host_;

  scoped_ptr<wm::ScopedCaptureClient> capture_client_;

  Connections connections_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApp);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
