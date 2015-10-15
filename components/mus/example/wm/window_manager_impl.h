// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_

#include "base/macros.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

class WindowManagerApplication;

class WindowManagerImpl : public mus::mojom::WindowManager {
 public:
   WindowManagerImpl(WindowManagerApplication* state,
                     mojo::InterfaceRequest<mus::mojom::WindowManager> request);
   ~WindowManagerImpl() override;

 private:
  // mus::mojom::WindowManager:
  void OpenWindow(mojo::ViewTreeClientPtr client) override;
  void CenterWindow(uint32_t view_id, mojo::SizePtr size) override;

  WindowManagerApplication* state_;
  mojo::StrongBinding<mus::mojom::WindowManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_
