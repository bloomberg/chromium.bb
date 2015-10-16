// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_

#include <set>

#include "base/macros.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

class WindowManagerApplication;

using WindowManagerErrorCodeCallback =
    const mojo::Callback<void(mus::mojom::WindowManagerErrorCode)>;

class WindowManagerImpl : public mus::mojom::WindowManager,
                          public mus::WindowObserver {
 public:
   WindowManagerImpl(WindowManagerApplication* state,
                     mojo::InterfaceRequest<mus::mojom::WindowManager> request);
   ~WindowManagerImpl() override;

 private:
  // mus::mojom::WindowManager:
  void OpenWindow(mus::mojom::WindowTreeClientPtr client) override;
  void CenterWindow(mus::Id window_id,
                    mojo::SizePtr size,
                    const WindowManagerErrorCodeCallback& callback) override;
  void SetBounds(mus::Id window_id,
                 mojo::RectPtr bounds,
                 const WindowManagerErrorCodeCallback& callback) override;
  void GetDisplays(const GetDisplaysCallback& callback) override;

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;

  WindowManagerApplication* state_;
  mojo::StrongBinding<mus::mojom::WindowManager> binding_;
  std::set<mus::Id> windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_
