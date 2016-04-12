// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/catalog.h"

#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/catalog/entry.h"
#include "mojo/services/catalog/manifest_provider.h"
#include "mojo/services/catalog/store.h"
#include "services/shell/public/cpp/names.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace catalog {
namespace {

base::FilePath GetManifestPath(const base::FilePath& package_dir,
                               const std::string& name) {
  // TODO(beng): think more about how this should be done for exe targets.
  std::string type = mojo::GetNameType(name);
  std::string path = mojo::GetNamePath(name);
  if (type == mojo::kNameType_Mojo) {
    return package_dir.AppendASCII("Mojo Applications").AppendASCII(
        path + "/manifest.json");
  }
  if (type == mojo::kNameType_Exe)
    return package_dir.AppendASCII(path + "_manifest.json");
  return base::FilePath();
}

base::FilePath GetPackagePath(const base::FilePath& package_dir,
                              const std::string& name) {
  std::string type = mojo::GetNameType(name);
  if (type == mojo::kNameType_Mojo) {
    // It's still a mojo: URL, use the default mapping scheme.
    const std::string host = mojo::GetNamePath(name);
    return package_dir.AppendASCII("Mojo Applications").AppendASCII(
        host + "/" + host + ".mojo");
  }
  if (type == mojo::kNameType_Exe) {
#if defined OS_WIN
    std::string extension = ".exe";
#else
    std::string extension;
#endif
    return package_dir.AppendASCII(mojo::GetNamePath(name) + extension);
  }
  return base::FilePath();
}

scoped_ptr<ReadManifestResult> ProcessManifest(
    const base::FilePath& user_package_dir,
    const base::FilePath& system_package_dir,
    const std::string& name,
    scoped_ptr<base::Value> manifest_root) {
  scoped_ptr<Entry> entry(new Entry(name));
  if (manifest_root) {
    const base::DictionaryValue* dictionary = nullptr;
    CHECK(manifest_root->GetAsDictionary(&dictionary));
    entry = Entry::Deserialize(*dictionary);
  }
  entry->set_path(GetPackagePath(system_package_dir, name));

  scoped_ptr<ReadManifestResult> result(new ReadManifestResult);
  // NOTE: This TypeConverter must run on a thread which allows IO.
  result->resolve_result = mojo::shell::mojom::ResolveResult::From(*entry);
  result->catalog_entry = std::move(entry);
  result->package_dir = system_package_dir;
  return result;
}

scoped_ptr<ReadManifestResult> ReadManifest(
    const base::FilePath& user_package_dir,
    const base::FilePath& system_package_dir,
    const std::string& name) {
  base::FilePath manifest_path = GetManifestPath(system_package_dir, name);
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;

  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return ProcessManifest(user_package_dir, system_package_dir, name,
                         deserializer.Deserialize(&error, &message));
}

void AddEntryToMap(const Entry& entry,
                   mojo::Map<mojo::String, mojom::CatalogEntryPtr>* map) {
  mojom::CatalogEntryPtr entry_ptr(mojom::CatalogEntry::New());
  entry_ptr->display_name = entry.display_name();
  (*map)[entry.name()] = std::move(entry_ptr);
}

}  // namespace

ReadManifestResult::ReadManifestResult() {}
ReadManifestResult::~ReadManifestResult() {}

////////////////////////////////////////////////////////////////////////////////
// Catalog, public:

Catalog::Catalog(scoped_ptr<Store> store,
                 base::TaskRunner* file_task_runner,
                 EntryCache* system_catalog,
                 ManifestProvider* manifest_provider)
    : manifest_provider_(manifest_provider),
      store_(std::move(store)),
      file_task_runner_(file_task_runner),
      system_catalog_(system_catalog),
      weak_factory_(this) {
  PathService::Get(base::DIR_MODULE, &system_package_dir_);
  DeserializeCatalog();
}

Catalog::~Catalog() {}

void Catalog::BindResolver(mojom::ResolverRequest request) {
  resolver_bindings_.AddBinding(this, std::move(request));
}

void Catalog::BindShellResolver(
    mojo::shell::mojom::ShellResolverRequest request) {
  shell_resolver_bindings_.AddBinding(this, std::move(request));
}

void Catalog::BindCatalog(mojom::CatalogRequest request) {
  catalog_bindings_.AddBinding(this, std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojom::Resolver:

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

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojo::shell::mojom::ShellResolver:

void Catalog::ResolveMojoName(const mojo::String& mojo_name,
                              const ResolveMojoNameCallback& callback) {
  std::string type = mojo::GetNameType(mojo_name);
  if (type != "mojo" && type != "exe") {
    scoped_ptr<Entry> entry(new Entry(mojo_name));
    callback.Run(mojo::shell::mojom::ResolveResult::From(*entry));
    return;
  }

  auto entry = user_catalog_.find(mojo_name);
  if (entry != user_catalog_.end()) {
    callback.Run(mojo::shell::mojom::ResolveResult::From(*entry->second));
    return;
  }
  entry = system_catalog_->find(mojo_name);
  if (entry != system_catalog_->end()) {
    callback.Run(mojo::shell::mojom::ResolveResult::From(*entry->second));
    return;
  }

  std::string manifest_contents;
  if (manifest_provider_ &&
      manifest_provider_->GetApplicationManifest(mojo_name.To<std::string>(),
                                                 &manifest_contents)) {
    scoped_ptr<base::Value> manifest_root =
        base::JSONReader::Read(manifest_contents);
    base::PostTaskAndReplyWithResult(
        file_task_runner_, FROM_HERE,
        base::Bind(&ProcessManifest, user_package_dir_, system_package_dir_,
                   mojo_name, base::Passed(&manifest_root)),
        base::Bind(&Catalog::OnReadManifest, weak_factory_.GetWeakPtr(),
                   mojo_name, callback));
  } else {
    base::PostTaskAndReplyWithResult(
        file_task_runner_, FROM_HERE,
        base::Bind(&ReadManifest, user_package_dir_, system_package_dir_,
                   mojo_name),
        base::Bind(&Catalog::OnReadManifest, weak_factory_.GetWeakPtr(),
                   mojo_name, callback));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojom::Catalog:

void Catalog::GetEntries(mojo::Array<mojo::String> names,
                         const GetEntriesCallback& callback) {
  mojo::Map<mojo::String, mojom::CatalogEntryPtr> entries;
  if (names.is_null()) {
    for (const auto& entry : user_catalog_)
      AddEntryToMap(*entry.second, &entries);
    for (const auto& entry : *system_catalog_)
      AddEntryToMap(*entry.second, &entries);
  } else {
    std::vector<mojo::String> names_vec = names.PassStorage();
    for (const std::string& name : names_vec) {
      Entry* entry = nullptr;
      if (user_catalog_.find(name) != user_catalog_.end())
        entry = user_catalog_[name].get();
      else if (system_catalog_->find(name) != system_catalog_->end())
        entry = (*system_catalog_)[name].get();
      else
        continue;
      AddEntryToMap(*entry, &entries);
    }
  }
  callback.Run(std::move(entries));
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, private:

void Catalog::DeserializeCatalog() {
  if (!store_)
    return;
  const base::ListValue* catalog = store_->GetStore();
  CHECK(catalog);
  // TODO(sky): make this handle aliases.
  // TODO(beng): implement this properly!
  for (auto it = catalog->begin(); it != catalog->end(); ++it) {
    const base::DictionaryValue* dictionary = nullptr;
    const base::Value* v = *it;
    CHECK(v->GetAsDictionary(&dictionary));
    scoped_ptr<Entry> entry = Entry::Deserialize(*dictionary);
    if (entry)
      user_catalog_[entry->name()] = std::move(entry);
  }
}

void Catalog::SerializeCatalog() {
  // TODO(beng): system catalog?
  scoped_ptr<base::ListValue> catalog(new base::ListValue);
  for (const auto& entry : user_catalog_)
    catalog->Append(entry.second->Serialize());
  if (store_)
    store_->UpdateStore(std::move(catalog));
}

// static
void Catalog::OnReadManifest(base::WeakPtr<Catalog> catalog,
                             const std::string& name,
                             const ResolveMojoNameCallback& callback,
                             scoped_ptr<ReadManifestResult> result) {
  callback.Run(std::move(result ->resolve_result));
  if (catalog) {
    catalog->AddEntryToCatalog(
        std::move(result->catalog_entry),
        result->package_dir == catalog->system_package_dir_);
  }
}

void Catalog::AddEntryToCatalog(scoped_ptr<Entry> entry,
                                bool is_system_catalog) {
  DCHECK(entry);
  EntryCache* catalog = is_system_catalog ? system_catalog_ : &user_catalog_;
  if (catalog->end() != catalog->find(entry->name()))
    return;
  for (auto child : entry->applications())
    AddEntryToCatalog(make_scoped_ptr(child), is_system_catalog);
  (*catalog)[entry->name()] = std::move(entry);
  SerializeCatalog();
}

}  // namespace catalog
