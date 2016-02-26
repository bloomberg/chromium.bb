// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/package_manager/package_manager.h"

#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/task_runner_util.h"
#include "mojo/common/mojo_scheme_register.h"
#include "mojo/common/url_type_converters.h"
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
  std::string url_string;
  CHECK(value.GetString(ApplicationCatalogStore::kUrlKey, &url_string));
  info.url = GURL(url_string);
  CHECK(value.GetString(ApplicationCatalogStore::kNameKey, &info.name));
  const base::DictionaryValue* capabilities = nullptr;
  CHECK(value.GetDictionary(ApplicationCatalogStore::kCapabilitiesKey,
                            &capabilities));
  info.base_filter = BuildCapabilityFilterFromDictionary(*capabilities);
  return info;
}

void SerializeEntry(const ApplicationInfo& entry,
                    base::DictionaryValue** value) {
  *value = new base::DictionaryValue;
  (*value)->SetString(ApplicationCatalogStore::kUrlKey, entry.url.spec());
  (*value)->SetString(ApplicationCatalogStore::kNameKey, entry.name);
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
const char ApplicationCatalogStore::kUrlKey[] = "url";
// static
const char ApplicationCatalogStore::kNameKey[] = "name";
// static
const char ApplicationCatalogStore::kCapabilitiesKey[] = "capabilities";

ApplicationInfo::ApplicationInfo() {}
ApplicationInfo::~ApplicationInfo() {}

PackageManager::PackageManager(base::TaskRunner* blocking_pool,
                               bool register_schemes,
                               scoped_ptr<ApplicationCatalogStore> catalog)
    : blocking_pool_(blocking_pool),
      catalog_store_(std::move(catalog)),
      weak_factory_(this) {
  if (register_schemes)
    mojo::RegisterMojoSchemes();

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
  if (connection->GetRemoteApplicationURL() == "mojo://shell/")
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

void PackageManager::ResolveMojoURL(const mojo::String& mojo_url,
                                    const ResolveMojoURLCallback& callback) {
  GURL resolved_url = mojo_url.To<GURL>();
  auto alias_iter = mojo_url_aliases_.find(resolved_url);
  std::string qualifier;
  if (alias_iter != mojo_url_aliases_.end()) {
    resolved_url = alias_iter->second.first;
    qualifier = alias_iter->second.second;
  } else {
    qualifier = resolved_url.host();
  }

  EnsureURLInCatalog(resolved_url, qualifier, callback);
}

void PackageManager::GetEntries(
    mojo::Array<mojo::String> urls,
    const GetEntriesCallback& callback) {
  mojo::Map<mojo::String, mojom::CatalogEntryPtr> entries;
  std::vector<mojo::String> urls_vec = urls.PassStorage();
  for (const std::string& url_string : urls_vec) {
    const GURL url(url_string);
    if (catalog_.find(url) == catalog_.end())
      continue;
    const ApplicationInfo& info = catalog_[url];
    mojom::CatalogEntryPtr entry(mojom::CatalogEntry::New());
    entry->name = info.name;
    entries[info.url.spec()] = std::move(entry);
  }
  callback.Run(std::move(entries));
}

void PackageManager::CompleteResolveMojoURL(
    const GURL& resolved_url,
    const std::string& qualifier,
    const ResolveMojoURLCallback& callback) {
  auto info_iter = catalog_.find(resolved_url);
  CHECK(info_iter != catalog_.end());

  GURL file_url;
  if (resolved_url.SchemeIs("mojo")) {
    // It's still a mojo: URL, use the default mapping scheme.
    const std::string host = resolved_url.host();
    file_url = system_package_dir_.Resolve(host + "/" + host + ".mojo");
  } else if (resolved_url.SchemeIs("exe")) {
#if defined OS_WIN
    std::string extension = ".exe";
#else
    std::string extension;
#endif
    file_url = system_package_dir_.Resolve(resolved_url.host() + extension);
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
  callback.Run(resolved_url.spec(), qualifier, std::move(filter),
               file_url.spec());
}

bool PackageManager::IsURLInCatalog(const GURL& url) const {
  return catalog_.find(url) != catalog_.end();
}

void PackageManager::EnsureURLInCatalog(
    const GURL& url,
    const std::string& qualifier,
    const ResolveMojoURLCallback& callback) {
  if (IsURLInCatalog(url)) {
    CompleteResolveMojoURL(url, qualifier, callback);
    return;
  }

  GURL manifest_url = GetManifestURL(url);
  if (manifest_url.is_empty()) {
    // The URL is of some form that can't be resolved to a manifest (e.g. some
    // scheme used for tests). Just pass it back to the caller so it can be
    // loaded with a custom loader.
    callback.Run(url.spec(), url.spec(), nullptr, nullptr);
    return;
  }

  CHECK(url.SchemeIs("mojo") || url.SchemeIs("exe"));
  base::FilePath manifest_path;
  CHECK(net::FileURLToFilePath(manifest_url, &manifest_path));
  base::PostTaskAndReplyWithResult(
      blocking_pool_, FROM_HERE, base::Bind(&ReadManifest, manifest_path),
      base::Bind(&PackageManager::OnReadManifest, weak_factory_.GetWeakPtr(),
                 url, qualifier, callback));
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
    catalog_[app_info.url] = app_info;
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
  if (catalog_.find(info.url) == catalog_.end()) {
    catalog_[info.url] = info;

    if (dictionary->HasKey("applications")) {
      const base::ListValue* applications = nullptr;
      dictionary->GetList("applications", &applications);
      for (size_t i = 0; i < applications->GetSize(); ++i) {
        const base::DictionaryValue* child = nullptr;
        applications->GetDictionary(i, &child);
        const ApplicationInfo& child_info = DeserializeApplication(child);
        mojo_url_aliases_[child_info.url] =
            std::make_pair(info.url, child_info.url.host());
      }
    }
  }
  return catalog_[info.url];
}

GURL PackageManager::GetManifestURL(const GURL& url) {
  // TODO(beng): think more about how this should be done for exe targets.
  if (url.SchemeIs("mojo"))
    return system_package_dir_.Resolve(url.host() + "/manifest.json");
  else if (url.SchemeIs("exe"))
    return system_package_dir_.Resolve(url.host() + "_manifest.json");
  return GURL();
}

// static
void PackageManager::OnReadManifest(base::WeakPtr<PackageManager> pm,
                                    const GURL& url,
                                    const std::string& qualifier,
                                    const ResolveMojoURLCallback& callback,
                                    scoped_ptr<base::Value> manifest) {
  if (!pm) {
    // The PackageManager was destroyed, we're likely in shutdown. Run the
    // callback so we don't trigger a DCHECK.
    callback.Run(url.spec(), url.spec(), nullptr, nullptr);
    return;
  }
  pm->OnReadManifestImpl(url, qualifier, callback, std::move(manifest));
}

void PackageManager::OnReadManifestImpl(const GURL& url,
                                        const std::string& qualifier,
                                        const ResolveMojoURLCallback& callback,
                                        scoped_ptr<base::Value> manifest) {
  if (manifest) {
    base::DictionaryValue* dictionary = nullptr;
    CHECK(manifest->GetAsDictionary(&dictionary));
    DeserializeApplication(dictionary);
  } else {
    ApplicationInfo info;
    info.url = url;
    info.name = url.spec();
    catalog_[info.url] = info;
  }
  SerializeCatalog();
  CompleteResolveMojoURL(url, qualifier, callback);
}

}  // namespace package_manager
