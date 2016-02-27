// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_APPLICATION_MANAGER_APPLICATION_LOADER_H_
#define SHELL_APPLICATION_MANAGER_APPLICATION_LOADER_H_

#include "base/callback.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace mojo {
namespace shell {

// Interface to implement special loading behavior for a particular name.
class ApplicationLoader {
 public:
  virtual ~ApplicationLoader() {}

  virtual void Load(const std::string& name,
                    mojom::ShellClientRequest request) = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_APPLICATION_MANAGER_APPLICATION_LOADER_H_
