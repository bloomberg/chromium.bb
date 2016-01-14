// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_USER_WINDOW_CONTROLLER_IMPL_H_
#define MASH_WM_USER_WINDOW_CONTROLLER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_observer.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"

namespace mash {
namespace wm {

class WindowManagerApplication;

class UserWindowControllerImpl : public mash::wm::mojom::UserWindowController,
                                 public mus::WindowObserver {
 public:
  UserWindowControllerImpl();
  ~UserWindowControllerImpl() override;

  void Initialize(WindowManagerApplication* state);

 private:
  // A helper to get the container for user windows.
  mus::Window* GetUserWindowContainer() const;

  // mus::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;

  // mash::wm::mojom::UserWindowController:
  void AddUserWindowObserver(
      mash::wm::mojom::UserWindowObserverPtr observer) override;
  void FocusUserWindow(uint32_t window_id) override;

  WindowManagerApplication* state_;
  mash::wm::mojom::UserWindowObserverPtr user_window_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserWindowControllerImpl);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_USER_WINDOW_CONTROLLER_IMPL_H_
