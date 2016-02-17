// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
#define MOJO_SERVICES_PACKAGE_MANAGER_PACKAGE_MANAGER_H_

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/services/package_manager/public/interfaces/resolver.mojom.h"
#include "mojo/services/package_manager/public/interfaces/shell_resolver.mojom.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "url/gurl.h"

namespace package_manager {
// A set of names of interfaces that may be exposed to an application.
using AllowedInterfaces = std::set<std::string>;
// A map of allowed applications to allowed interface sets. See shell.mojom for
// more details.
using CapabilityFilter = std::map<std::string, AllowedInterfaces>;

// Static information about an application package known to the PackageManager.
struct ApplicationInfo {
  ApplicationInfo();
  ~ApplicationInfo();

  std::string url;
  std::string name;
  CapabilityFilter base_filter;
};

// Implemented by an object that provides storage for the application catalog
// (e.g. in Chrome, preferences). The PackageManagerImpl is the canonical owner
// of the contents of the store, so no one else must modify its contents.
class ApplicationCatalogStore {
 public:
  // Called during initialization to construct the PackageManagerImpl's catalog.
  virtual void GetStore(base::ListValue** store) = 0;

  // Write the catalog to the store. Called when the PackageManagerImpl learns
  // of a newly encountered application.
  virtual void UpdateStore(scoped_ptr<base::ListValue> store) = 0;

 protected:
  virtual ~ApplicationCatalogStore();
};

class PackageManager : public mojo::ShellClient,
                       public mojo::InterfaceFactory<mojom::Resolver>,
                       public mojo::InterfaceFactory<mojom::ShellResolver>,
                       public mojom::Resolver,
                       public mojom::ShellResolver {
 public:
  explicit PackageManager(base::TaskRunner* blocking_pool);
  ~PackageManager() override;

 private:
  using MojoURLAliasMap =
      std::map<std::string, std::pair<std::string, std::string>>;

  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<mojom::Resolver>:
  void Create(mojo::Connection* connection,
              mojom::ResolverRequest request) override;

  // mojo::InterfaceFactory<mojom::ShellResolver>:
  void Create(mojo::Connection* connection,
              mojom::ShellResolverRequest request) override;

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

  // mojom::ShellResolver:
  void ResolveMojoURL(const mojo::String& mojo_url,
                      const ResolveMojoURLCallback& callback) override;

  // Completes resolving a Mojo URL from the Shell after the resolved URL has
  // been added to the catalog and the manifest read.
  void CompleteResolveMojoURL(const GURL& resolved_url,
                              const ResolveMojoURLCallback& callback);

  bool IsURLInCatalog(const GURL& url) const;

  // Called from ResolveMojoURL().
  // If |url| is not in the catalog, attempts to load a manifest for it.
  void EnsureURLInCatalog(const GURL& url,
                          const ResolveMojoURLCallback& callback);

  // Populate/serialize the catalog from/to the supplied store.
  void DeserializeCatalog();
  void SerializeCatalog();

  // Construct a catalog entry from |dictionary|.
  const ApplicationInfo& DeserializeApplication(
      const base::DictionaryValue* dictionary);

  GURL GetManifestURL(const GURL& url);

  // Reads a manifest in the blocking pool and returns a base::Value with its
  // contents via OnReadManifest().
  scoped_ptr<base::Value> ReadManifest(const base::FilePath& manifest_path);
  void OnReadManifest(const GURL& url,
                      const ResolveMojoURLCallback& callback,
                      scoped_ptr<base::Value> manifest);

  base::TaskRunner* blocking_pool_;
  GURL system_package_dir_;

  mojo::WeakBindingSet<mojom::Resolver> resolver_bindings_;
  mojo::WeakBindingSet<mojom::ShellResolver> shell_resolver_bindings_;

  ApplicationCatalogStore* catalog_store_;
  std::map<std::string, ApplicationInfo> catalog_;

  MojoURLAliasMap mojo_url_aliases_;

  DISALLOW_COPY_AND_ASSIGN(PackageManager);
};

}  // namespace package_manager

#endif  // MOJO_SERVICES_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
