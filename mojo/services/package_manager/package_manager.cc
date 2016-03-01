// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/package_manager/package_manager.h"

#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_split.h"
#include "base/task_runner_util.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/shell/public/cpp/names.h"
#include "mojo/util/filename_util.h"
#include "net/base/filename_util.h"
#include "url/url_util.h"

namespace package_manager {
namespace {

CapabilityFilter BuildCapabilityFilterFromDictionary(
    const base::DictionaryValue& value) {
  CapabilityFilter filter;
  base::DictionaryValue::Iterator it(value);
  for (; !it.IsAtEnd(); it.Advance()) {
    const base::ListValue* values = nullptr;
    CHECK(it.value().GetAsList(&values));
    AllowedInterfaces interfaces;
    for (auto i = values->begin(); i != values->end(); ++i) {
      std::string iface_name;
      const base::Value* v = *i;
      CHECK(v->GetAsString(&iface_name));
      interfaces.insert(iface_name);
    }
    filter[it.key()] = interfaces;
  }
  return filter;
}

ApplicationInfo BuildApplicationInfoFromDictionary(
    const base::DictionaryValue& value) {
  ApplicationInfo info;
  std::string name_string;
  CHECK(value.GetString(ApplicationCatalogStore::kNameKey, &name_string));
  CHECK(mojo::IsValidName(name_string)) << "Invalid Name: " << name_string;
  info.name = name_string;
  CHECK(value.GetString(ApplicationCatalogStore::kDisplayNameKey,
                        &info.display_name));
  const base::DictionaryValue* capabilities = nullptr;
  CHECK(value.GetDictionary(ApplicationCatalogStore::kCapabilitiesKey,
                            &capabilities));
  info.base_filter = BuildCapabilityFilterFromDictionary(*capabilities);
  return info;
}

void SerializeEntry(const ApplicationInfo& entry,
                    base::DictionaryValue** value) {
  *value = new base::DictionaryValue;
  (*value)->SetString(ApplicationCatalogStore::kNameKey, entry.name);
  (*value)->SetString(ApplicationCatalogStore::kDisplayNameKey,
                      entry.display_name);
  base::DictionaryValue* capabilities = new base::DictionaryValue;
  for (const auto& pair : entry.base_filter) {
    scoped_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& iface_name : pair.second)
      interfaces->AppendString(iface_name);
    capabilities->Set(pair.first, std::move(interfaces));
  }
  (*value)->Set(ApplicationCatalogStore::kCapabilitiesKey,
                make_scoped_ptr(capabilities));
}

scoped_ptr<base::Value> ReadManifest(const base::FilePath& manifest_path) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;
  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return deserializer.Deserialize(&error, &message);
}

}  // namespace

// static
const char ApplicationCatalogStore::kNameKey[] = "name";
// static
const char ApplicationCatalogStore::kDisplayNameKey[] = "display_name";
// static
const char ApplicationCatalogStore::kCapabilitiesKey[] = "capabilities";

ApplicationInfo::ApplicationInfo() {}
ApplicationInfo::ApplicationInfo(const ApplicationInfo& other) = default;
ApplicationInfo::~ApplicationInfo() {}

PackageManager::PackageManager(base::TaskRunner* blocking_pool,
                               scoped_ptr<ApplicationCatalogStore> catalog)
    : blocking_pool_(blocking_pool),
      catalog_store_(std::move(catalog)),
      weak_factory_(this) {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);

  system_package_dir_ =
      mojo::util::FilePathToFileURL(shell_dir).Resolve(std::string());
  system_package_dir_ =
      mojo::util::AddTrailingSlashIfNeeded(system_package_dir_);

  DeserializeCatalog();
}
PackageManager::~PackageManager() {}

bool PackageManager::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<mojom::Catalog>(this);
  connection->AddInterface<mojom::Resolver>(this);
  if (connection->GetRemoteApplicationName() == "mojo:shell")
    connection->AddInterface<mojom::ShellResolver>(this);
  return true;
}

void PackageManager::Create(mojo::Connection* connection,
                            mojom::ResolverRequest request) {
  resolver_bindings_.AddBinding(this, std::move(request));
}

void PackageManager::Create(mojo::Connection* connection,
                            mojom::ShellResolverRequest request) {
  shell_resolver_bindings_.AddBinding(this, std::move(request));
}

void PackageManager::Create(mojo::Connection* connection,
                            mojom::CatalogRequest request) {
  catalog_bindings_.AddBinding(this, std::move(request));
}

void PackageManager::ResolveResponse(mojo::URLResponsePtr response,
                                     const ResolveResponseCallback& callback) {
  // TODO(beng): implement.
}

void PackageManager::ResolveInterfaces(
    mojo::Array<mojo::String> interfaces,
    const ResolveInterfacesCallback& callback) {
  // TODO(beng): implement.
}

void PackageManager::ResolveMIMEType(const mojo::String& mime_type,
                                     const ResolveMIMETypeCallback& callback) {
  // TODO(beng): implement.
}

void PackageManager::ResolveProtocolScheme(
    const mojo::String& scheme,
    const ResolveProtocolSchemeCallback& callback) {
  // TODO(beng): implement.
}

void PackageManager::ResolveMojoName(const mojo::String& mojo_name,
                                     const ResolveMojoNameCallback& callback) {
  std::string resolved_name = mojo_name;
  auto alias_iter = mojo_name_aliases_.find(resolved_name);
  std::string qualifier;
  if (alias_iter != mojo_name_aliases_.end()) {
    resolved_name = alias_iter->second.first;
    qualifier = alias_iter->second.second;
  } else {
    qualifier = mojo::GetNamePath(resolved_name);
  }

  EnsureNameInCatalog(resolved_name, qualifier, callback);
}

void PackageManager::GetEntries(
    mojo::Array<mojo::String> names,
    const GetEntriesCallback& callback) {
  mojo::Map<mojo::String, mojom::CatalogEntryPtr> entries;
  std::vector<mojo::String> names_vec = names.PassStorage();
  for (const std::string& name : names_vec) {
    if (catalog_.find(name) == catalog_.end())
      continue;
    const ApplicationInfo& info = catalog_[name];
    mojom::CatalogEntryPtr entry(mojom::CatalogEntry::New());
    entry->display_name = info.display_name;
    entries[info.name] = std::move(entry);
  }
  callback.Run(std::move(entries));
}

void PackageManager::CompleteResolveMojoName(
    const std::string& resolved_name,
    const std::string& qualifier,
    const ResolveMojoNameCallback& callback) {
  auto info_iter = catalog_.find(resolved_name);
  CHECK(info_iter != catalog_.end());

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

  mojo::shell::mojom::CapabilityFilterPtr filter(
      mojo::shell::mojom::CapabilityFilter::New());
  filter->filter = mojo::Map<mojo::String, mojo::Array<mojo::String>>();
  for (const auto& entry : info_iter->second.base_filter) {
    mojo::Array<mojo::String> interfaces;
    for (auto interface_name : entry.second)
      interfaces.push_back(interface_name);
    filter->filter.insert(entry.first, std::move(interfaces));
  }
  callback.Run(resolved_name, qualifier, std::move(filter),
               file_url.spec());
}

bool PackageManager::IsNameInCatalog(const std::string& name) const {
  return catalog_.find(name) != catalog_.end();
}

void PackageManager::EnsureNameInCatalog(
    const std::string& name,
    const std::string& qualifier,
    const ResolveMojoNameCallback& callback) {
  if (IsNameInCatalog(name)) {
    CompleteResolveMojoName(name, qualifier, callback);
    return;
  }

  GURL manifest_url = GetManifestURL(name);
  if (manifest_url.is_empty()) {
    // The name is of some form that can't be resolved to a manifest (e.g. some
    // scheme used for tests). Just pass it back to the caller so it can be
    // loaded with a custom loader.
    callback.Run(name, name, nullptr, nullptr);
    return;
  }

  std::string type = mojo::GetNameType(name);
  CHECK(type == "mojo" || type == "exe");
  base::FilePath manifest_path;
  CHECK(net::FileURLToFilePath(manifest_url, &manifest_path));
  base::PostTaskAndReplyWithResult(
      blocking_pool_, FROM_HERE, base::Bind(&ReadManifest, manifest_path),
      base::Bind(&PackageManager::OnReadManifest, weak_factory_.GetWeakPtr(),
                 name, qualifier, callback));
}

void PackageManager::DeserializeCatalog() {
  if (!catalog_store_)
    return;
  const base::ListValue* catalog = catalog_store_->GetStore();
  CHECK(catalog);
  // TODO(sky): make this handle aliases.
  for (auto it = catalog->begin(); it != catalog->end(); ++it) {
    const base::DictionaryValue* dictionary = nullptr;
    const base::Value* v = *it;
    CHECK(v->GetAsDictionary(&dictionary));
    const ApplicationInfo app_info =
        BuildApplicationInfoFromDictionary(*dictionary);
    catalog_[app_info.name] = app_info;
  }
}

void PackageManager::SerializeCatalog() {
  scoped_ptr<base::ListValue> catalog(new base::ListValue);
  for (const auto& info : catalog_) {
    base::DictionaryValue* dictionary = nullptr;
    SerializeEntry(info.second, &dictionary);
    catalog->Append(make_scoped_ptr(dictionary));
  }
  if (catalog_store_)
    catalog_store_->UpdateStore(std::move(catalog));
}

const ApplicationInfo& PackageManager::DeserializeApplication(
    const base::DictionaryValue* dictionary) {
  ApplicationInfo info = BuildApplicationInfoFromDictionary(*dictionary);
  if (catalog_.find(info.name) == catalog_.end()) {
    catalog_[info.name] = info;

    if (dictionary->HasKey("applications")) {
      const base::ListValue* applications = nullptr;
      dictionary->GetList("applications", &applications);
      for (size_t i = 0; i < applications->GetSize(); ++i) {
        const base::DictionaryValue* child = nullptr;
        applications->GetDictionary(i, &child);
        const ApplicationInfo& child_info = DeserializeApplication(child);
        mojo_name_aliases_[child_info.name] =
            std::make_pair(info.name, mojo::GetNamePath(child_info.name));
      }
    }
  }
  return catalog_[info.name];
}

GURL PackageManager::GetManifestURL(const std::string& name) {
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
void PackageManager::OnReadManifest(base::WeakPtr<PackageManager> pm,
                                    const std::string& name,
                                    const std::string& qualifier,
                                    const ResolveMojoNameCallback& callback,
                                    scoped_ptr<base::Value> manifest) {
  if (!pm) {
    // The PackageManager was destroyed, we're likely in shutdown. Run the
    // callback so we don't trigger a DCHECK.
    callback.Run(name, name, nullptr, nullptr);
    return;
  }
  pm->OnReadManifestImpl(name, qualifier, callback, std::move(manifest));
}

void PackageManager::OnReadManifestImpl(const std::string& name,
                                        const std::string& qualifier,
                                        const ResolveMojoNameCallback& callback,
                                        scoped_ptr<base::Value> manifest) {
  if (manifest) {
    base::DictionaryValue* dictionary = nullptr;
    CHECK(manifest->GetAsDictionary(&dictionary));
    DeserializeApplication(dictionary);
  } else {
    ApplicationInfo info;
    info.name = name;
    info.display_name = name;
    catalog_[info.name] = info;
  }
  SerializeCatalog();
  CompleteResolveMojoName(name, qualifier, callback);
}

}  // namespace package_manager
