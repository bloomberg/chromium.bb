// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_CATALOG_H_
#define MOJO_SERVICES_CATALOG_CATALOG_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/catalog/entry.h"
#include "mojo/services/catalog/public/interfaces/catalog.mojom.h"
#include "mojo/services/catalog/public/interfaces/resolver.mojom.h"
#include "mojo/services/catalog/reader.h"
#include "mojo/services/catalog/store.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/shell_resolver.mojom.h"

namespace catalog {

class Store;

class Catalog : public mojom::Resolver,
                public mojo::shell::mojom::ShellResolver,
                public mojom::Catalog {
 public:
  Catalog(base::TaskRunner* blocking_pool, scoped_ptr<Store> store);
  ~Catalog() override;

  void BindResolver(mojom::ResolverRequest request);
  void BindShellResolver(mojo::shell::mojom::ShellResolverRequest request);
  void BindCatalog(mojom::CatalogRequest request);

 private:
  using MojoNameAliasMap =
      std::map<std::string, std::pair<std::string, std::string>>;

  // mojom::Resolver:
  void ResolveResponse(
      mojo::URLResponsePtr response,
      const ResolveResponseCallback& callback) override;
  void ResolveInterfaces(mojo::Array<mojo::String> interfaces,
                         const ResolveInterfacesCallback& callback) override;
  void ResolveMIMEType(const mojo::String& mime_type,
                       const ResolveMIMETypeCallback& callback) override;
  void ResolveProtocolScheme(
      const mojo::String& scheme,
      const ResolveProtocolSchemeCallback& callback) override;

  // mojo::shell::mojom::ShellResolver:
  void ResolveMojoName(const mojo::String& mojo_name,
                       const ResolveMojoNameCallback& callback) override;

  // mojom::Catalog:
  void GetEntries(mojo::Array<mojo::String> names,
                  const GetEntriesCallback& callback) override;

  // Completes resolving a Mojo name from the Shell after the resolved name has
  // been added to the catalog and the manifest read.
  void CompleteResolveMojoName(const std::string& resolved_name,
                               const std::string& qualifier,
                               const ResolveMojoNameCallback& callback);

  bool IsNameInCatalog(const std::string& name) const;

  // Populate/serialize the catalog from/to the supplied store.
  void DeserializeCatalog();
  void SerializeCatalog();

  // Callback for Reader, receives an Entry constructed from the manifest for
  // |name|.
  static void OnReadEntry(base::WeakPtr<Catalog> catalog,
                          const std::string& name,
                          const ResolveMojoNameCallback& callback,
                          scoped_ptr<Entry> entry);
  void OnReadEntryImpl(const std::string& name,
                       const ResolveMojoNameCallback& callback,
                       scoped_ptr<Entry> entry);

  // Construct a catalog entry from |dictionary|.
  scoped_ptr<Entry> DeserializeApplication(
      const base::DictionaryValue* dictionary);

  scoped_ptr<Reader> reader_;
  base::FilePath package_path_;

  mojo::BindingSet<mojom::Resolver> resolver_bindings_;
  mojo::BindingSet<mojo::shell::mojom::ShellResolver> shell_resolver_bindings_;
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  scoped_ptr<Store> store_;
  std::map<std::string, Entry> catalog_;

  // Used when an app handles multiple names. Maps from app (as name) to name of
  // app that is responsible for handling it. The value is a pair of the name of
  // the handler along with a qualifier.
  MojoNameAliasMap mojo_name_aliases_;

  std::map<std::string, std::string> qualifiers_;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_CATALOG_H_
