// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_SCREEN_MANAGER_IMPL_H_
#define ATHENA_SCREEN_SCREEN_MANAGER_IMPL_H_

#include "athena/athena_export.h"
#include "athena/screen/public/screen_manager.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/window_tree_client.h"

namespace aura {
namespace client {
class FocusClient;
class ScreenPositionClient;
}
}

namespace wm {
class ScopedCaptureClient;
}

namespace athena {
class AcceleratorHandler;

class ATHENA_EXPORT ScreenManagerImpl : public ScreenManager,
                                        public aura::client::WindowTreeClient {
 public:
  explicit ScreenManagerImpl(aura::Window* root_window);
  ~ScreenManagerImpl() override;

  void Init();

  // Returns a container which has |priority|. Null if such container
  // doesn't exist.
  aura::Window* FindContainerByPriority(int priority);

 private:
  // ScreenManager:
  virtual aura::Window* CreateContainer(const ContainerParams& params) override;
  virtual aura::Window* GetContext() override;
  virtual void SetRotation(gfx::Display::Rotation rotation) override;
  virtual void SetRotationLocked(bool rotation_locked) override;

  // aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) override;

  int GetModalContainerPriority(aura::Window* window, aura::Window* parent);

  // Returns a container with |params.default_parent| == true.
  aura::Window* GetDefaultContainer();

  // Not owned.
  aura::Window* root_window_;

  scoped_ptr<aura::client::FocusClient> focus_client_;
  scoped_ptr<AcceleratorHandler> accelerator_handler_;
  scoped_ptr<::wm::ScopedCaptureClient> capture_client_;
  scoped_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  gfx::Display::Rotation last_requested_rotation_;
  bool rotation_locked_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerImpl);
};

}  // namespace athena

#endif  // ATHENA_SCREEN_SCREEN_MANAGER_IMPL_H_
