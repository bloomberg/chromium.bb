// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WINDOW_MANAGER_IMPL_H_
#define MASH_WM_WINDOW_MANAGER_IMPL_H_

#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"

class WindowManagerApplication;

using WindowManagerErrorCodeCallback =
    const mojo::Callback<void(mus::mojom::WindowManagerErrorCode)>;

class WindowManagerImpl : public mus::mojom::WindowManager,
                          public mus::WindowObserver {
 public:
  explicit WindowManagerImpl(WindowManagerApplication* state);
  ~WindowManagerImpl() override;

 private:
  gfx::Rect CalculateDefaultBounds(mus::Window* window) const;
  gfx::Rect GetMaximizedWindowBounds() const;

  // mus::mojom::WindowManager:
  void OpenWindow(
      mus::mojom::WindowTreeClientPtr client,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void GetConfig(const GetConfigCallback& callback) override;

  mus::Window* GetContainerForChild(mus::Window* child);

  WindowManagerApplication* state_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

#endif  // MASH_WM_WINDOW_MANAGER_IMPL_H_
