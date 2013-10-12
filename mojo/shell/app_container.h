// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APP_CONTAINER_H_
#define MOJO_SHELL_APP_CONTAINER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/system/core.h"

namespace base {
class Thread;
}

namespace mojo {
namespace shell {

// A container class that runs an app on its own thread.
class AppContainer {
 public:
  AppContainer();
  ~AppContainer();

  void LaunchApp(const base::FilePath& app_path);

 private:
  void AppCompleted();

  base::WeakPtrFactory<AppContainer> weak_factory_;

  scoped_ptr<base::Thread> thread_;

  // Following members are valid only on app thread.
  Handle shell_handle_;

  DISALLOW_COPY_AND_ASSIGN(AppContainer);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APP_CONTAINER_H_
