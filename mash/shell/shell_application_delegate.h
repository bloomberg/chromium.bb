// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELL_SHELL_APPLICATION_DELEGATE_H_
#define MASH_SHELL_SHELL_APPLICATION_DELEGATE_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace mojo {
class ApplicationConnection;
}

namespace mash {
namespace shell {

class ShellApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  ShellApplicationDelegate();
  ~ShellApplicationDelegate() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  void StartWindowManager();
  void StartWallpaper();
  void StartShelf();
  void StartBrowserDriver();
  void StartQuickLaunch();

  // Starts the application at |url|, running |restart_callback| if the
  // connection to the application is closed.
  void StartRestartableService(const std::string& url,
                               const base::Closure& restart_callback);

  mojo::ApplicationImpl* app_;
  std::map<std::string, scoped_ptr<mojo::ApplicationConnection>> connections_;

  DISALLOW_COPY_AND_ASSIGN(ShellApplicationDelegate);
};

}  // namespace shell
}  // namespace mash

#endif  // MASH_SHELL_SHELL_APPLICATION_DELEGATE_H_
