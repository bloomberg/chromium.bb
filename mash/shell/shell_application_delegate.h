// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELL_SHELL_APPLICATION_DELEGATE_H_
#define MASH_SHELL_SHELL_APPLICATION_DELEGATE_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mash/shell/public/interfaces/shell.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/common/weak_interface_ptr_set.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mojo {
class ApplicationConnection;
}

namespace mash {
namespace shell {

class ShellApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mash::shell::mojom::Shell,
      public mojo::InterfaceFactory<mash::shell::mojom::Shell> {
 public:
  ShellApplicationDelegate();
  ~ShellApplicationDelegate() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mash::shell::mojom::Shell:
  void AddScreenlockStateListener(
      mojom::ScreenlockStateListenerPtr listener) override;
  void LockScreen() override;
  void UnlockScreen() override;

  // mojo::InterfaceFactory<mash::shell::mojom::Shell>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mash::shell::mojom::Shell> r) override;

  void StartWindowManager();
  void StartWallpaper();
  void StartShelf();
  void StartBrowserDriver();
  void StartQuickLaunch();

  void StartScreenlock();
  void StopScreenlock();

  // Starts the application at |url|, running |restart_callback| if the
  // connection to the application is closed.
  void StartRestartableService(const std::string& url,
                               const base::Closure& restart_callback);

  mojo::ApplicationImpl* app_;
  std::map<std::string, scoped_ptr<mojo::ApplicationConnection>> connections_;
  bool screen_locked_;
  mojo::WeakBindingSet<mash::shell::mojom::Shell> bindings_;
  mojo::WeakInterfacePtrSet<mojom::ScreenlockStateListener>
      screenlock_listeners_;

  DISALLOW_COPY_AND_ASSIGN(ShellApplicationDelegate);
};

}  // namespace shell
}  // namespace mash

#endif  // MASH_SHELL_SHELL_APPLICATION_DELEGATE_H_
