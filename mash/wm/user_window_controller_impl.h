// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_USER_WINDOW_CONTROLLER_IMPL_H_
#define MASH_WM_USER_WINDOW_CONTROLLER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_connection_observer.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"

namespace mash {
namespace wm {

class RootWindowController;
class WindowTitleObserver;

class UserWindowControllerImpl : public mojom::UserWindowController,
                                 public mus::WindowObserver,
                                 public mus::WindowTreeConnectionObserver {
 public:
  UserWindowControllerImpl();
  ~UserWindowControllerImpl() override;

  mojom::UserWindowObserver* user_window_observer() const {
    return user_window_observer_.get();
  }

  void Initialize(RootWindowController* root_controller);

 private:
  // A helper to get the container for user windows.
  mus::Window* GetUserWindowContainer() const;

  // mus::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;

  // mus::WindowTreeConnectionObserver:
  void OnWindowTreeFocusChanged(mus::Window* gained_focus,
                                mus::Window* lost_focus) override;

  // mojom::UserWindowController:
  void AddUserWindowObserver(mojom::UserWindowObserverPtr observer) override;
  void FocusUserWindow(uint32_t window_id) override;

  RootWindowController* root_controller_;
  mojom::UserWindowObserverPtr user_window_observer_;
  scoped_ptr<WindowTitleObserver> window_title_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserWindowControllerImpl);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_USER_WINDOW_CONTROLLER_IMPL_H_
