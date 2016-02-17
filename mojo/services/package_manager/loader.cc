// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/package_manager/loader.h"

#include "mojo/services/package_manager/package_manager.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace package_manager {

Loader::Loader(base::TaskRunner* blocking_pool)
    : blocking_pool_(blocking_pool) {}
Loader::~Loader() {}

void Loader::Load(const GURL& url,
                  mojo::shell::mojom::ShellClientRequest request) {
  client_.reset(new package_manager::PackageManager(blocking_pool_));
  connection_.reset(new mojo::ShellConnection(client_.get(),
                                              std::move(request)));
}

}  // namespace package_manager
