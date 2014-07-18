// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "mojo/aura/window_tree_host_mojo_delegate.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
namespace client {
class ActivationClient;
class FocusClient;
}
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
                         public view_manager::NodeObserver,
                         public WindowTreeHostMojoDelegate,
                         public aura::client::FocusChangeObserver,
                         public aura::client::ActivationChangeObserver {
 public:
  WindowManagerApp();
  virtual ~WindowManagerApp();

  void AddConnection(WindowManagerServiceImpl* connection);
  void RemoveConnection(WindowManagerServiceImpl* connection);

  view_manager::Id OpenWindow();
  view_manager::Id OpenWindowWithURL(const String& url);
  void SetCapture(view_manager::Id node);
  void FocusWindow(view_manager::Id node);
  void ActivateWindow(view_manager::Id node);

  bool IsReady() const;

 private:
  typedef std::set<WindowManagerServiceImpl*> Connections;
  typedef std::map<view_manager::Id, aura::Window*> NodeIdToWindowMap;

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* impl) MOJO_OVERRIDE;
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) MOJO_OVERRIDE;
  virtual void OnViewManagerDisconnected(
      view_manager::ViewManager* view_manager) MOJO_OVERRIDE;

  // Overridden from view_manager::NodeObserver:
  virtual void OnTreeChanged(
      const view_manager::NodeObserver::TreeChangeParams& params) MOJO_OVERRIDE;

  // Overridden from WindowTreeHostMojoDelegate:
  virtual void CompositorContentsChanged(const SkBitmap& bitmap) MOJO_OVERRIDE;

  // Overridden from aura::client::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) MOJO_OVERRIDE;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) MOJO_OVERRIDE;

  aura::Window* GetWindowForNodeId(view_manager::Id node) const;

  // Creates an aura::Window for every node in the hierarchy beneath |id|,
  // and adds to the registry so that it can be retrieved later via
  // GetWindowForNodeId().
  // TODO(beng): perhaps Node should have a property bag.
  void RegisterSubtree(view_manager::Id id, aura::Window* parent);
  // Deletes the aura::Windows associated with the hierarchy beneath |id|,
  // and removes from the registry.
  void UnregisterSubtree(view_manager::Id id);

  view_manager::ViewManager* view_manager_;
  view_manager::Node* root_;

  scoped_ptr<AuraInit> aura_init_;
  scoped_ptr<WindowTreeHostMojo> window_tree_host_;

  scoped_ptr<wm::ScopedCaptureClient> capture_client_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  aura::client::ActivationClient* activation_client_;

  Connections connections_;
  NodeIdToWindowMap node_id_to_window_map_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApp);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
