// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/catalog.h"

#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_split.h"
#include "base/task_runner_util.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/catalog/builder.h"
#include "mojo/services/catalog/entry.h"
#include "mojo/services/catalog/store.h"
#include "mojo/shell/public/cpp/names.h"
#include "mojo/util/filename_util.h"
#include "net/base/filename_util.h"
#include "url/url_util.h"

namespace catalog {
namespace {

scoped_ptr<base::Value> ReadManifest(const base::FilePath& manifest_path) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;
  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return deserializer.Deserialize(&error, &message);
}

}  // namespace

Catalog::Catalog(base::TaskRunner* blocking_pool,
                 scoped_ptr<Store> catalog)
    : blocking_pool_(blocking_pool),
      store_(std::move(catalog)),
      weak_factory_(this) {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);

  system_package_dir_ =
      mojo::util::FilePathToFileURL(shell_dir).Resolve(std::string());
  system_package_dir_ =
      mojo::util::AddTrailingSlashIfNeeded(system_package_dir_);

  DeserializeCatalog();
}
Catalog::~Catalog() {}

bool Catalog::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<mojom::Catalog>(this);
  connection->AddInterface<mojom::Resolver>(this);
  if (connection->GetRemoteIdentity().name() == "mojo:shell")
    connection->AddInterface<mojo::shell::mojom::ShellResolver>(this);
  return true;
}

void Catalog::Create(mojo::Connection* connection,
                     mojom::ResolverRequest request) {
  resolver_bindings_.AddBinding(this, std::move(request));
}

void Catalog::Create(mojo::Connection* connection,
                     mojo::shell::mojom::ShellResolverRequest request) {
  shell_resolver_bindings_.AddBinding(this, std::move(request));
}

void Catalog::Create(mojo::Connection* connection,
                     mojom::CatalogRequest request) {
  catalog_bindings_.AddBinding(this, std::move(request));
}

void Catalog::ResolveResponse(mojo::URLResponsePtr response,
                              const ResolveResponseCallback& callback) {
  // TODO(beng): implement.
}

void Catalog::ResolveInterfaces(mojo::Array<mojo::String> interfaces,
                                const ResolveInterfacesCallback& callback) {
  // TODO(beng): implement.
}

void Catalog::ResolveMIMEType(const mojo::String& mime_type,
                              const ResolveMIMETypeCallback& callback) {
  // TODO(beng): implement.
}

void Catalog::ResolveProtocolScheme(
    const mojo::String& scheme,
    const ResolveProtocolSchemeCallback& callback) {
  // TODO(beng): implement.
}

void Catalog::ResolveMojoName(const mojo::String& mojo_name,
                              const ResolveMojoNameCallback& callback) {
  std::string resolved_name = mojo_name;
  auto alias_iter = mojo_name_aliases_.find(resolved_name);
  if (alias_iter != mojo_name_aliases_.end())
    resolved_name = alias_iter->second.first;

  std::string qualifier = mojo::GetNamePath(resolved_name);
  auto qualifier_iter = qualifiers_.find(resolved_name);
  if (qualifier_iter != qualifiers_.end())
    qualifier = qualifier_iter->second;

  if (IsNameInCatalog(resolved_name))
    CompleteResolveMojoName(resolved_name, qualifier, callback);
  else
    AddNameToCatalog(resolved_name, callback);
}

void Catalog::GetEntries(mojo::Array<mojo::String> names,
                         const GetEntriesCallback& callback) {
  mojo::Map<mojo::String, mojom::CatalogEntryPtr> entries;
  std::vector<mojo::String> names_vec = names.PassStorage();
  for (const std::string& name : names_vec) {
    if (catalog_.find(name) == catalog_.end())
      continue;
    const Entry& entry = catalog_[name];
    mojom::CatalogEntryPtr entry_ptr(mojom::CatalogEntry::New());
    entry_ptr->display_name = entry.display_name;
    entries[entry.name] = std::move(entry_ptr);
  }
  callback.Run(std::move(entries));
}

void Catalog::CompleteResolveMojoName(
    const std::string& resolved_name,
    const std::string& qualifier,
    const ResolveMojoNameCallback& callback) {
  auto entry_iter = catalog_.find(resolved_name);
  CHECK(entry_iter != catalog_.end());

  GURL file_url;
  std::string type = mojo::GetNameType(resolved_name);
  if (type == "mojo") {
    // It's still a mojo: URL, use the default mapping scheme.
    const std::string host = mojo::GetNamePath(resolved_name);
    file_url = system_package_dir_.Resolve(host + "/" + host + ".mojo");
  } else if (type == "exe") {
#if defined OS_WIN
    std::string extension = ".exe";
#else
    std::string extension;
#endif
    file_url = system_package_dir_.Resolve(
        mojo::GetNamePath(resolved_name) + extension);
  }

  mojo::shell::mojom::CapabilitySpecPtr capabilities_ptr =
      mojo::shell::mojom::CapabilitySpec::From(entry_iter->second.capabilities);

  callback.Run(resolved_name, qualifier, std::move(capabilities_ptr),
               file_url.spec());
}

bool Catalog::IsNameInCatalog(const std::string& name) const {
  return catalog_.find(name) != catalog_.end();
}

void Catalog::AddNameToCatalog(const std::string& name,
                               const ResolveMojoNameCallback& callback) {
  GURL manifest_url = GetManifestURL(name);
  if (manifest_url.is_empty()) {
    // The name is of some form that can't be resolved to a manifest (e.g. some
    // scheme used for tests). Just pass it back to the caller so it can be
    // loaded with a custom loader.
    callback.Run(name, mojo::GetNamePath(name), nullptr, nullptr);
    return;
  }

  std::string type = mojo::GetNameType(name);
  CHECK(type == "mojo" || type == "exe");
  base::FilePath manifest_path;
  CHECK(net::FileURLToFilePath(manifest_url, &manifest_path));
  base::PostTaskAndReplyWithResult(
      blocking_pool_, FROM_HERE, base::Bind(&ReadManifest, manifest_path),
      base::Bind(&Catalog::OnReadManifest, weak_factory_.GetWeakPtr(),
                 name, callback));
}

void Catalog::DeserializeCatalog() {
  if (!store_)
    return;
  const base::ListValue* catalog = store_->GetStore();
  CHECK(catalog);
  // TODO(sky): make this handle aliases.
  for (auto it = catalog->begin(); it != catalog->end(); ++it) {
    const base::DictionaryValue* dictionary = nullptr;
    const base::Value* v = *it;
    CHECK(v->GetAsDictionary(&dictionary));
    const Entry entry = BuildEntry(*dictionary);
    catalog_[entry.name] = entry;
  }
}

void Catalog::SerializeCatalog() {
  scoped_ptr<base::ListValue> catalog(new base::ListValue);
  for (const auto& entry : catalog_)
    catalog->Append(SerializeEntry(entry.second));
  if (store_)
    store_->UpdateStore(std::move(catalog));
}

const Entry& Catalog::DeserializeApplication(
    const base::DictionaryValue* dictionary) {
  Entry entry = BuildEntry(*dictionary);
  if (catalog_.find(entry.name) == catalog_.end()) {
    catalog_[entry.name] = entry;

    if (dictionary->HasKey("applications")) {
      const base::ListValue* applications = nullptr;
      dictionary->GetList("applications", &applications);
      for (size_t i = 0; i < applications->GetSize(); ++i) {
        const base::DictionaryValue* child_value = nullptr;
        applications->GetDictionary(i, &child_value);
        const Entry& child = DeserializeApplication(child_value);
        mojo_name_aliases_[child.name] =
            std::make_pair(entry.name, child.qualifier);
      }
    }
    qualifiers_[entry.name] = entry.qualifier;
  }
  return catalog_[entry.name];
}

GURL Catalog::GetManifestURL(const std::string& name) {
  // TODO(beng): think more about how this should be done for exe targets.
  std::string type = mojo::GetNameType(name);
  std::string path = mojo::GetNamePath(name);
  if (type == "mojo")
    return system_package_dir_.Resolve(path + "/manifest.json");
  else if (type == "exe")
    return system_package_dir_.Resolve(path + "_manifest.json");
  return GURL();
}

// static
void Catalog::OnReadManifest(base::WeakPtr<Catalog> catalog,
                             const std::string& name,
                             const ResolveMojoNameCallback& callback,
                             scoped_ptr<base::Value> manifest) {
  if (!catalog) {
    // The Catalog was destroyed, we're likely in shutdown. Run the
    // callback so we don't trigger a DCHECK.
    callback.Run(name, mojo::GetNamePath(name), nullptr, nullptr);
    return;
  }
  catalog->OnReadManifestImpl(name, callback, std::move(manifest));
}

void Catalog::OnReadManifestImpl(const std::string& name,
                                 const ResolveMojoNameCallback& callback,
                                 scoped_ptr<base::Value> manifest) {
  if (manifest) {
    base::DictionaryValue* dictionary = nullptr;
    CHECK(manifest->GetAsDictionary(&dictionary));
    DeserializeApplication(dictionary);
  } else {
    Entry entry;
    entry.name = name;
    entry.display_name = name;
    catalog_[entry.name] = entry;
    qualifiers_[entry.name] = mojo::GetNamePath(name);
  }
  SerializeCatalog();

  auto qualifier_iter = qualifiers_.find(name);
  DCHECK(qualifier_iter != qualifiers_.end());
  std::string qualifier = qualifier_iter->second;
  CompleteResolveMojoName(name, qualifier, callback);
}

}  // namespace catalog
