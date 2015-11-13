// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_APPLICATION_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_APPLICATION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/mus/example/wm/public/interfaces/container.mojom.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/common/weak_binding_set.h"

class BackgroundLayout;
class ShelfLayout;
class WindowLayout;
class WindowManagerImpl;

namespace ui {
namespace mojo {
class UIInit;
}
}

namespace views {
class AuraInit;
}

class WindowManagerApplication
    : public mojo::ApplicationDelegate,
      public mus::WindowObserver,
      public mus::WindowTreeDelegate,
      public mojo::InterfaceFactory<mus::mojom::WindowManager>,
      // TODO(sky): make WindowManagerImpl implement this.
      public mus::WindowManagerDelegate {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  mus::Window* root() { return root_; }

  int window_count() { return window_count_; }
  void IncrementWindowCount() { ++window_count_; }

  mus::Window* GetWindowForContainer(ash::mojom::Container container);
  mus::Window* GetWindowById(mus::Id id);

  mojo::ApplicationImpl* app() { return app_; }

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // InterfaceFactory<mus::mojom::WindowManager>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mus::mojom::WindowManager> request) override;

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;

  // WindowManagerDelegate:
  bool OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(mus::Window* window,
                       const std::string& name,
                       scoped_ptr<std::vector<uint8_t>>* new_data) override;

  // Sets up the window containers used for z-space management.
  void CreateContainers();

  // nullptr until the Mus connection is established via OnEmbed().
  mus::Window* root_;
  int window_count_;

  mojo::ApplicationImpl* app_;

  mus::mojom::WindowTreeHostPtr host_;

  scoped_ptr<ui::mojo::UIInit> ui_init_;
  scoped_ptr<views::AuraInit> aura_init_;

  // |window_manager_| is created once OnEmbed() is called. Until that time
  // |requests_| stores any pending WindowManager interface requests.
  scoped_ptr<WindowManagerImpl> window_manager_;
  mojo::WeakBindingSet<mus::mojom::WindowManager> window_manager_binding_;
  ScopedVector<mojo::InterfaceRequest<mus::mojom::WindowManager>> requests_;

  scoped_ptr<BackgroundLayout> background_layout_;
  scoped_ptr<ShelfLayout> shelf_layout_;
  scoped_ptr<WindowLayout> window_layout_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_APPLICATION_H_
