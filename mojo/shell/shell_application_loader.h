// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_APPLICATION_LOADER_H_
#define MOJO_SHELL_SHELL_APPLICATION_LOADER_H_

#include "base/macros.h"
#include "mojo/shell/application_loader.h"

namespace mojo {
class ShellConnection;
namespace shell {

class ShellApplicationLoader : public ApplicationLoader {
 public:
  explicit ShellApplicationLoader(ApplicationManager* manager);
  ~ShellApplicationLoader() override;

 private:
  // Overridden from ApplicationLoader:
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override;

  scoped_ptr<ShellConnection> shell_connection_;

  ApplicationManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ShellApplicationLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_APPLICATION_LOADER_H_
