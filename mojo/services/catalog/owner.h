// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_OWNER_H_
#define MOJO_SERVICES_CATALOG_OWNER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace base {
class TaskRunner;
}

namespace mojo{
class ShellClient;
class ShellConnection;
}

namespace catalog {

class Store;

// Creates and owns an instance of the catalog. Exposes a ShellClientPtr that
// can be passed to the Shell, potentially in a different process.
class Owner {
 public:
  Owner(base::TaskRunner* file_task_runner, scoped_ptr<Store> store);
  ~Owner();

  mojo::shell::mojom::ShellClientPtr TakeShellClient();

 private:
  mojo::shell::mojom::ShellClientPtr shell_client_;
  scoped_ptr<mojo::ShellClient> catalog_shell_client_;
  scoped_ptr<mojo::ShellConnection> catalog_connection_;

  DISALLOW_COPY_AND_ASSIGN(Owner);
};

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_OWNER_H_
