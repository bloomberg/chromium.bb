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
  virtual ~ShelfDelegateImpl();

  void set_watcher(WindowWatcher* watcher) { watcher_ = watcher; }

  // ShelfDelegate overrides:
  virtual void OnShelfCreated(Shelf* shelf) OVERRIDE;
  virtual void OnShelfDestroyed(Shelf* shelf) OVERRIDE;
  virtual ShelfID GetShelfIDForAppID(const std::string& app_id) OVERRIDE;
  virtual const std::string& GetAppIDForShelfID(ShelfID id) OVERRIDE;
  virtual void PinAppWithID(const std::string& app_id) OVERRIDE;
  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE;
  virtual bool CanPin() const OVERRIDE;
  virtual void UnpinAppWithID(const std::string& app_id) OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELF_DELEGATE_IMPL_H_
