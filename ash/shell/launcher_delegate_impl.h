// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_LAUNCHER_DELEGATE_IMPL_H_
#define ASH_SHELL_LAUNCHER_DELEGATE_IMPL_H_

#include "ash/launcher/launcher_delegate.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {
namespace shell {

class WindowWatcher;

class LauncherDelegateImpl : public ash::LauncherDelegate {
 public:
  explicit LauncherDelegateImpl(WindowWatcher* watcher);
  virtual ~LauncherDelegateImpl();

  void set_watcher(WindowWatcher* watcher) { watcher_ = watcher; }

  // LauncherDelegate overrides:
  virtual void OnLauncherCreated(Launcher* launcher) OVERRIDE;
  virtual void OnLauncherDestroyed(Launcher* launcher) OVERRIDE;
  virtual LauncherID GetLauncherIDForAppID(const std::string& app_id) OVERRIDE;
  virtual const std::string& GetAppIDForLauncherID(LauncherID id) OVERRIDE;
  virtual void PinAppWithID(const std::string& app_id) OVERRIDE;
  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE;
  virtual bool CanPin() const OVERRIDE;
  virtual void UnpinAppWithID(const std::string& app_id) OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(LauncherDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_LAUNCHER_DELEGATE_IMPL_H_
