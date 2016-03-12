// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/owner.h"

#include "mojo/services/catalog/catalog.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace catalog {

Owner::Owner(base::TaskRunner* file_task_runner, scoped_ptr<Store> store)
    : catalog_shell_client_(new Catalog(file_task_runner, std::move(store))) {
  mojo::shell::mojom::ShellClientRequest request = GetProxy(&shell_client_);
  catalog_connection_.reset(new mojo::ShellConnection(
      catalog_shell_client_.get(), std::move(request)));
}
Owner::~Owner() {}

mojo::shell::mojom::ShellClientPtr Owner::TakeShellClient() {
  return std::move(shell_client_);
}

}  // namespace catalog
