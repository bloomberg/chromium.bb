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

class MoveLoop;
class WindowManagerApplication;

using WindowManagerErrorCodeCallback =
    const mojo::Callback<void(mus::mojom::WindowManagerErrorCode)>;

class WindowManagerImpl : public mus::mojom::WindowManager,
                          public mus::WindowObserver {
 public:
  explicit WindowManagerImpl(WindowManagerApplication* state);
  ~WindowManagerImpl() override;

 private:
  // mus::mojom::WindowManager:
  void OpenWindow(
      mus::mojom::WindowTreeClientPtr client,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void SetPreferredSize(
      mus::Id window_id,
      mojo::SizePtr size,
      const WindowManagerErrorCodeCallback& callback) override;
  void SetShowState(mus::Id window_id,
                    mus::mojom::ShowState show_state,
                    const WindowManagerErrorCodeCallback& callback) override;
  void SetResizeBehavior(uint32_t window_id,
                         mus::mojom::ResizeBehavior resize_behavior) override;
  void GetConfig(const GetConfigCallback& callback) override;

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;
  void OnWindowInputEvent(mus::Window* window,
                          const mus::mojom::EventPtr& event) override;

  mus::Window* GetContainerForChild(mus::Window* child);

  WindowManagerApplication* state_;
  scoped_ptr<MoveLoop> move_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_MANAGER_IMPL_H_
