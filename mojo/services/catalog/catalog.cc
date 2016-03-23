// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/catalog.h"

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/task_runner_util.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/catalog/entry.h"
#include "mojo/services/catalog/store.h"
#include "mojo/shell/public/cpp/names.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace catalog {

////////////////////////////////////////////////////////////////////////////////
// Catalog, public:

Catalog::Catalog(base::TaskRunner* blocking_pool, scoped_ptr<Store> store)
    : store_(std::move(store)),
      weak_factory_(this) {
  PathService::Get(base::DIR_MODULE, &package_path_);
  reader_.reset(new Reader(package_path_, blocking_pool));
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

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojo::shell::mojom::ShellResolver:

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

  if (IsNameInCatalog(resolved_name)) {
    CompleteResolveMojoName(resolved_name, qualifier, callback);
  } else {
    reader_->Read(resolved_name,
                  base::Bind(&Catalog::OnReadEntry, weak_factory_.GetWeakPtr(),
                             resolved_name, callback));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojom::Catalog:

void Catalog::GetEntries(mojo::Array<mojo::String> names,
                         const GetEntriesCallback& callback) {
  mojo::Map<mojo::String, mojom::CatalogEntryPtr> entries;
  std::vector<mojo::String> names_vec = names.PassStorage();
  for (const std::string& name : names_vec) {
    if (catalog_.find(name) == catalog_.end())
      continue;
    const Entry& entry = catalog_[name];
    mojom::CatalogEntryPtr entry_ptr(mojom::CatalogEntry::New());
    entry_ptr->display_name = entry.display_name();
    entries[entry.name()] = std::move(entry_ptr);
  }
  callback.Run(std::move(entries));
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, private:

void Catalog::CompleteResolveMojoName(
    const std::string& resolved_name,
    const std::string& qualifier,
    const ResolveMojoNameCallback& callback) {
  auto entry_iter = catalog_.find(resolved_name);
  CHECK(entry_iter != catalog_.end());

  GURL package_url = mojo::util::AddTrailingSlashIfNeeded(
      mojo::util::FilePathToFileURL(package_path_));
  GURL file_url;
  std::string type = mojo::GetNameType(resolved_name);
  if (type == "mojo") {
    // It's still a mojo: URL, use the default mapping scheme.
    const std::string host = mojo::GetNamePath(resolved_name);
    file_url = package_url.Resolve(host + "/" + host + ".mojo");
  } else if (type == "exe") {
#if defined OS_WIN
    std::string extension = ".exe";
#else
    std::string extension;
#endif
    file_url = package_url.Resolve(
        mojo::GetNamePath(resolved_name) + extension);
  }

  mojo::shell::mojom::CapabilitySpecPtr capabilities_ptr =
      mojo::shell::mojom::CapabilitySpec::From(
          entry_iter->second.capabilities());

  callback.Run(resolved_name, qualifier, std::move(capabilities_ptr),
               file_url.spec());
}

bool Catalog::IsNameInCatalog(const std::string& name) const {
  return catalog_.find(name) != catalog_.end();
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
    scoped_ptr<Entry> entry = Entry::Deserialize(*dictionary);
    if (entry)
      catalog_[entry->name()] = *entry;
  }
}

void Catalog::SerializeCatalog() {
  scoped_ptr<base::ListValue> catalog(new base::ListValue);
  for (const auto& entry : catalog_)
    catalog->Append(entry.second.Serialize());
  if (store_)
    store_->UpdateStore(std::move(catalog));
}

// static
void Catalog::OnReadEntry(base::WeakPtr<Catalog> catalog,
                          const std::string& name,
                          const ResolveMojoNameCallback& callback,
                          scoped_ptr<Entry> entry) {
  if (!catalog) {
    callback.Run(name, mojo::GetNamePath(name), nullptr, nullptr);
    return;
  }
  catalog->OnReadEntryImpl(name, callback, std::move(entry));
}

void Catalog::OnReadEntryImpl(const std::string& name,
                              const ResolveMojoNameCallback& callback,
                              scoped_ptr<Entry> entry) {
  // TODO(beng): evaluate the conditions under which entry is null.
  if (!entry) {
    entry.reset(new Entry);
    entry->set_name(name);
    entry->set_display_name(name);
    entry->set_qualifier(mojo::GetNamePath(name));
  }

  if (catalog_.find(entry->name()) == catalog_.end()) {
    catalog_[entry->name()] = *entry;

    if (!entry->applications().empty()) {
      for (const auto& child : entry->applications()) {
        mojo_name_aliases_[child.name()] =
            std::make_pair(entry->name(), child.qualifier());
      }
    }
    qualifiers_[entry->name()] = entry->qualifier();
  }

  SerializeCatalog();

  auto qualifier_iter = qualifiers_.find(name);
  DCHECK(qualifier_iter != qualifiers_.end());
  std::string qualifier = qualifier_iter->second;
  CompleteResolveMojoName(name, qualifier, callback);
}

}  // namespace catalog
