// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_USER_WINDOW_CONTROLLER_IMPL_H_
#define ASH_MUS_USER_WINDOW_CONTROLLER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "ash/public/interfaces/user_window_controller.mojom.h"
#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_client_observer.h"

namespace ash {
namespace mus {

class RootWindowController;
class WindowPropertyObserver;

class UserWindowControllerImpl : public mojom::UserWindowController,
                                 public ::mus::WindowObserver,
                                 public ::mus::WindowTreeClientObserver {
 public:
  UserWindowControllerImpl();
  ~UserWindowControllerImpl() override;

  mojom::UserWindowObserver* user_window_observer() const {
    return user_window_observer_.get();
  }

  void Initialize(RootWindowController* root_controller);

 private:
  void AssignIdIfNecessary(::mus::Window* window);

  // Removes observers from the window and client.
  void RemoveObservers(::mus::Window* user_container);

  // Returns the window with the specified user id.
  ::mus::Window* GetUserWindowById(uint32_t id);

  // A helper to get the container for user windows.
  ::mus::Window* GetUserWindowContainer() const;

  // mus::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;
  void OnWindowDestroying(::mus::Window* window) override;

  // mus::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(::mus::Window* gained_focus,
                                ::mus::Window* lost_focus) override;

  // mojom::UserWindowController:
  void AddUserWindowObserver(mojom::UserWindowObserverPtr observer) override;
  void FocusUserWindow(uint32_t window_id) override;

  RootWindowController* root_controller_;
  mojom::UserWindowObserverPtr user_window_observer_;
  std::unique_ptr<WindowPropertyObserver> window_property_observer_;
  uint32_t next_id_ = 1u;

  DISALLOW_COPY_AND_ASSIGN(UserWindowControllerImpl);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_USER_WINDOW_CONTROLLER_IMPL_H_
