// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/factory.h"

#include "base/bind.h"
#include "mojo/services/catalog/catalog.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace catalog {

Factory::Factory(base::TaskRunner* file_task_runner, scoped_ptr<Store> store)
    : file_task_runner_(file_task_runner),
      store_(std::move(store)),
      weak_factory_(this) {
  mojo::shell::mojom::ShellClientRequest request = GetProxy(&shell_client_);
  shell_connection_.reset(new mojo::ShellConnection(this, std::move(request)));
}
Factory::~Factory() {}

mojo::shell::mojom::ShellClientPtr Factory::TakeShellClient() {
  return std::move(shell_client_);
}

bool Factory::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<mojom::Catalog>(this);
  connection->AddInterface<mojom::Resolver>(this);
  connection->AddInterface<mojo::shell::mojom::ShellResolver>(this);
  return true;
}

void Factory::Create(mojo::Connection* connection,
                     mojom::ResolverRequest request) {
  Catalog* instance =
      GetCatalogForUserId(connection->GetRemoteIdentity().user_id());
  instance->BindResolver(std::move(request));
}

void Factory::Create(mojo::Connection* connection,
                     mojo::shell::mojom::ShellResolverRequest request) {
  Catalog* instance =
      GetCatalogForUserId(connection->GetRemoteIdentity().user_id());
  instance->BindShellResolver(std::move(request));
}

void Factory::Create(mojo::Connection* connection,
                     mojom::CatalogRequest request) {
  Catalog* instance =
      GetCatalogForUserId(connection->GetRemoteIdentity().user_id());
  instance->BindCatalog(std::move(request));
}

Catalog* Factory::GetCatalogForUserId(const std::string& user_id) {
  auto it = catalogs_.find(user_id);
  if (it != catalogs_.end())
    return it->second.get();

  // TODO(beng): There needs to be a way to load the store from different users.
  Catalog* instance =
      new Catalog(std::move(store_), file_task_runner_, &system_catalog_);
  catalogs_[user_id] = make_scoped_ptr(instance);
  return instance;
}

}  // namespace catalog
