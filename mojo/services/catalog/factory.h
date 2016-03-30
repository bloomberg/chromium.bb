// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_FACTORY_H_
#define MOJO_SERVICES_CATALOG_FACTORY_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/catalog/public/interfaces/catalog.mojom.h"
#include "mojo/services/catalog/public/interfaces/resolver.mojom.h"
#include "mojo/services/catalog/types.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/shell/public/interfaces/shell_resolver.mojom.h"

namespace base {
class TaskRunner;
}

namespace mojo{
class ShellConnection;
}

namespace catalog {

class Catalog;
class Store;

// Creates and owns an instance of the catalog. Exposes a ShellClientPtr that
// can be passed to the Shell, potentially in a different process.
class Factory
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<mojom::Catalog>,
      public mojo::InterfaceFactory<mojom::Resolver>,
      public mojo::InterfaceFactory<mojo::shell::mojom::ShellResolver> {
 public:
  Factory(base::TaskRunner* file_task_runner, scoped_ptr<Store> store);
  ~Factory() override;

  mojo::shell::mojom::ShellClientPtr TakeShellClient();

 private:
  // mojo::ShellClient:
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<mojom::Resolver>:
  void Create(mojo::Connection* connection,
              mojom::ResolverRequest request) override;

  // mojo::InterfaceFactory<mojo::shell::mojom::ShellResolver>:
  void Create(mojo::Connection* connection,
              mojo::shell::mojom::ShellResolverRequest request) override;

  // mojo::InterfaceFactory<mojom::Catalog>:
  void Create(mojo::Connection* connection,
              mojom::CatalogRequest request) override;

  Catalog* GetCatalogForUserId(const std::string& user_id);

  base::TaskRunner* file_task_runner_;
  scoped_ptr<Store> store_;

  mojo::shell::mojom::ShellClientPtr shell_client_;
  scoped_ptr<mojo::ShellConnection> shell_connection_;

  std::map<std::string, scoped_ptr<Catalog>> catalogs_;

  EntryCache system_catalog_;

  base::WeakPtrFactory<Factory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Factory);
};

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_FACTORY_H_
