// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PACKAGE_MANAGER_LOADER_H_
#define MOJO_SERVICES_PACKAGE_MANAGER_LOADER_H_

#include "mojo/shell/loader.h"

namespace base {
class BlockingPool;
}

namespace mojo {
class ShellClient;
class ShellConnection;
}

namespace package_manager {

class ApplicationCatalogStore;

class Loader : public mojo::shell::Loader {
 public:
  Loader(base::TaskRunner* blocking_pool,
         scoped_ptr<package_manager::ApplicationCatalogStore> app_catalog);
  ~Loader() override;

  // mojo::shell::Loader:
  void Load(const std::string& name,
            mojo::shell::mojom::ShellClientRequest request) override;

 private:
  base::TaskRunner* blocking_pool_;
  scoped_ptr<package_manager::ApplicationCatalogStore> app_catalog_;
  scoped_ptr<mojo::ShellClient> client_;
  scoped_ptr<mojo::ShellConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

}  // namespace package_manager

#endif  // MOJO_SERVICES_PACKAGE_MANAGER_LOADER_H_
