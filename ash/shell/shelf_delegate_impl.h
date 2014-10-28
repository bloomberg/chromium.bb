// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELF_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELF_DELEGATE_IMPL_H_

#include "ash/shelf/shelf_delegate.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {
namespace shell {

class WindowWatcher;

class ShelfDelegateImpl : public ShelfDelegate {
 public:
  explicit ShelfDelegateImpl(WindowWatcher* watcher);
  ~ShelfDelegateImpl() override;

  void set_watcher(WindowWatcher* watcher) { watcher_ = watcher; }

  // ShelfDelegate overrides:
  void OnShelfCreated(Shelf* shelf) override;
  void OnShelfDestroyed(Shelf* shelf) override;
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  bool CanPin() const override;
  void UnpinAppWithID(const std::string& app_id) override;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELF_DELEGATE_IMPL_H_
