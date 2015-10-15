// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_APPLICATION_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_APPLICATION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"

class WindowManagerApplication
    : public mojo::ApplicationDelegate,
      public mus::ViewTreeDelegate,
      public mojo::InterfaceFactory<mus::mojom::WindowManager> {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  mus::View* root() { return root_;  }

  int window_count() { return window_count_; }
  void IncrementWindowCount() { ++window_count_; }

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // ViewTreeDelegate:
  void OnEmbed(mus::View* root) override;
  void OnConnectionLost(mus::ViewTreeConnection* connection) override;

  // InterfaceFactory<mus::mojom::WindowManager>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mus::mojom::WindowManager> request) override;

  // Sets up the window containers used for z-space management.
  void CreateContainers();

  // nullptr until the Mus connection is established via OnEmbed().
  mus::View* root_;
  int window_count_;

  mojo::ViewTreeHostPtr host_;
  ScopedVector<mojo::InterfaceRequest<mus::mojom::WindowManager>> requests_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_APPLICATION_H_
