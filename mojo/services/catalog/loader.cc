// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/loader.h"

#include "mojo/services/catalog/catalog.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace catalog {

Loader::Loader(base::TaskRunner* blocking_pool, scoped_ptr<Store> store)
    : blocking_pool_(blocking_pool),
      store_(std::move(store)) {}
Loader::~Loader() {}

void Loader::Load(const std::string& name,
                  mojo::shell::mojom::ShellClientRequest request) {
  client_.reset(new catalog::Catalog(blocking_pool_, std::move(store_)));
  connection_.reset(new mojo::ShellConnection(client_.get(),
                                              std::move(request)));
}

}  // namespace catalog
