// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
#define MOJO_SERVICES_PACKAGE_MANAGER_PACKAGE_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/package_manager/public/interfaces/catalog.mojom.h"
#include "mojo/services/package_manager/public/interfaces/resolver.mojom.h"
#include "mojo/services/package_manager/public/interfaces/shell_resolver.mojom.h"
#include "mojo/shell/public/cpp/interface_factory.h"
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

  GURL url;
  std::string name;
  CapabilityFilter base_filter;
};

// Implemented by an object that provides storage for the application catalog
// (e.g. in Chrome, preferences). The PackageManagerImpl is the canonical owner
// of the contents of the store, so no one else must modify its contents.
class ApplicationCatalogStore {
 public:
  // Value is a string.
  static const char kUrlKey[];
  // Value is a string.
  static const char kNameKey[];
  // Value is a dictionary that maps from the filter to a list of string
  // interfaces.
  static const char kCapabilitiesKey[];

  virtual ~ApplicationCatalogStore() {}

  // Called during initialization to construct the PackageManagerImpl's catalog.
  // Returns a serialized list of the apps. Each entry in the returned list
  // corresponds to an app (as a dictionary). Each dictionary has a url, name
  // and capabilities. The return value is owned by the caller.
  virtual const base::ListValue* GetStore() = 0;

  // Write the catalog to the store. Called when the PackageManagerImpl learns
  // of a newly encountered application.
  virtual void UpdateStore(scoped_ptr<base::ListValue> store) = 0;
};

class PackageManager : public mojo::ShellClient,
                       public mojo::InterfaceFactory<mojom::Resolver>,
                       public mojo::InterfaceFactory<mojom::ShellResolver>,
                       public mojo::InterfaceFactory<mojom::Catalog>,
                       public mojom::Resolver,
                       public mojom::ShellResolver,
                       public mojom::Catalog {
 public:
  // If |register_schemes| is true, mojo: and exe: schemes are registered as
  // "standard".
  PackageManager(base::TaskRunner* blocking_pool,
                 bool register_schemes,
                 scoped_ptr<ApplicationCatalogStore> catalog);
  ~PackageManager() override;

 private:
  using MojoURLAliasMap = std::map<GURL, std::pair<GURL, std::string>>;

  // mojo::ShellClient:
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<mojom::Resolver>:
  void Create(mojo::Connection* connection,
              mojom::ResolverRequest request) override;

  // mojo::InterfaceFactory<mojom::ShellResolver>:
  void Create(mojo::Connection* connection,
              mojom::ShellResolverRequest request) override;

  // mojo::InterfaceFactory<mojom::Catalog>:
  void Create(mojo::Connection* connection,
              mojom::CatalogRequest request) override;

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

  // mojom::Catalog:
  void GetEntries(mojo::Array<mojo::String> urls,
                  const GetEntriesCallback& callback) override;

  // Completes resolving a Mojo URL from the Shell after the resolved URL has
  // been added to the catalog and the manifest read.
  void CompleteResolveMojoURL(const GURL& resolved_url,
                              const std::string& qualifier,
                              const ResolveMojoURLCallback& callback);

  bool IsURLInCatalog(const GURL& url) const;

  // Called from ResolveMojoURL().
  // If |url| is not in the catalog, attempts to load a manifest for it.
  void EnsureURLInCatalog(const GURL& url,
                          const std::string& qualifier,
                          const ResolveMojoURLCallback& callback);

  // Populate/serialize the catalog from/to the supplied store.
  void DeserializeCatalog();
  void SerializeCatalog();

  // Construct a catalog entry from |dictionary|.
  const ApplicationInfo& DeserializeApplication(
      const base::DictionaryValue* dictionary);

  GURL GetManifestURL(const GURL& url);

  // Called once the manifest has been read. |pm| may be null at this point,
  // but |callback| must be run.
  static void OnReadManifest(base::WeakPtr<PackageManager> pm,
                             const GURL& url,
                             const std::string& qualifier,
                             const ResolveMojoURLCallback& callback,
                             scoped_ptr<base::Value> manifest);

  // Called once the manifest is read and |this| hasn't been deleted.
  void OnReadManifestImpl(const GURL& url,
                          const std::string& qualifier,
                          const ResolveMojoURLCallback& callback,
                          scoped_ptr<base::Value> manifest);

  base::TaskRunner* blocking_pool_;
  GURL system_package_dir_;

  mojo::BindingSet<mojom::Resolver> resolver_bindings_;
  mojo::BindingSet<mojom::ShellResolver> shell_resolver_bindings_;
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  scoped_ptr<ApplicationCatalogStore> catalog_store_;
  std::map<GURL, ApplicationInfo> catalog_;

  // Used when an app handles multiple urls. Maps from app (as url) to url of
  // app that is responsible for handling it. The value is a pair of the
  // url of the handler along with a qualifier.
  MojoURLAliasMap mojo_url_aliases_;

  base::WeakPtrFactory<PackageManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PackageManager);
};

}  // namespace package_manager

#endif  // MOJO_SERVICES_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
