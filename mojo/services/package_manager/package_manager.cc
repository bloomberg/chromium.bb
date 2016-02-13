// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/package_manager/package_manager.h"

#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/task_runner_util.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/util/filename_util.h"
#include "net/base/filename_util.h"

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
  CHECK(value.GetString("url", &info.url));
  CHECK(value.GetString("name", &info.name));
  const base::DictionaryValue* capabilities = nullptr;
  CHECK(value.GetDictionary("capabilities", &capabilities));
  info.base_filter = BuildCapabilityFilterFromDictionary(*capabilities);
  return info;
}

void SerializeEntry(const ApplicationInfo& entry,
                    base::DictionaryValue** value) {
  *value = new base::DictionaryValue;
  (*value)->SetString("url", entry.url);
  (*value)->SetString("name", entry.name);
  base::DictionaryValue* capabilities = new base::DictionaryValue;
  for (const auto& pair : entry.base_filter) {
    scoped_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& iface_name : pair.second)
      interfaces->AppendString(iface_name);
    capabilities->Set(pair.first, std::move(interfaces));
  }
  (*value)->Set("capabilities", make_scoped_ptr(capabilities));
}

}

ApplicationInfo::ApplicationInfo() {}
ApplicationInfo::~ApplicationInfo() {}

ApplicationCatalogStore::~ApplicationCatalogStore() {}

PackageManager::PackageManager(base::TaskRunner* blocking_pool)
    : blocking_pool_(blocking_pool) {
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);

  system_package_dir_ =
      mojo::util::FilePathToFileURL(shell_dir).Resolve(std::string());
}
PackageManager::~PackageManager() {}

void PackageManager::Initialize(mojo::Shell* shell, const std::string& url,
                                uint32_t id) {}

bool PackageManager::AcceptConnection(mojo::Connection* connection) {
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
  CHECK(resolved_url.SchemeIs("mojo") || resolved_url.SchemeIs("exe"));

  auto alias_iter = mojo_url_aliases_.find(mojo_url);
  if (alias_iter != mojo_url_aliases_.end())
    resolved_url = GURL(alias_iter->second.first);

  EnsureURLInCatalog(resolved_url, callback);
}

void PackageManager::CompleteResolveMojoURL(
    const GURL& resolved_url,
    const ResolveMojoURLCallback& callback) {
  auto info_iter = catalog_.find(resolved_url.spec());
  CHECK(info_iter != catalog_.end());

  // TODO(beng): Use the actual capability filter from |info|!
  mojo::shell::mojom::CapabilityFilterPtr filter(
      mojo::shell::mojom::CapabilityFilter::New());
  mojo::Array<mojo::String> all_interfaces;
  all_interfaces.push_back("*");
  filter->filter.insert("*", std::move(all_interfaces));

  callback.Run(resolved_url.spec(), std::move(filter));
}

bool PackageManager::IsURLInCatalog(const GURL& url) const {
  return catalog_.find(url.spec()) != catalog_.end();
}

void PackageManager::EnsureURLInCatalog(
    const GURL& url,
    const ResolveMojoURLCallback& callback) {
  if (IsURLInCatalog(url)) {
    CompleteResolveMojoURL(url, callback);
    return;
  }

  GURL manifest_url = GetManifestURL(url);
  if (manifest_url.is_empty())
    return;
  base::FilePath manifest_path;
  CHECK(net::FileURLToFilePath(manifest_url, &manifest_path));
  base::PostTaskAndReplyWithResult(
      blocking_pool_, FROM_HERE,
      base::Bind(&PackageManager::ReadManifest, base::Unretained(this),
                 manifest_path),
      base::Bind(&PackageManager::OnReadManifest,
                 base::Unretained(this), url, callback));
}

void PackageManager::DeserializeCatalog() {
  ApplicationInfo info;
  info.url = "mojo://shell/";
  info.name = "Mojo Shell";
  catalog_[info.url] = info;

  if (!catalog_store_)
    return;
  base::ListValue* catalog = nullptr;
  catalog_store_->GetStore(&catalog);
  CHECK(catalog);
  for (auto it = catalog->begin(); it != catalog->end(); ++it) {
    const base::DictionaryValue* dictionary = nullptr;
    const base::Value* v = *it;
    CHECK(v->GetAsDictionary(&dictionary));
    DeserializeApplication(dictionary);
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
  CHECK(catalog_.find(info.url) == catalog_.end());
  catalog_[info.url] = info;

  if (dictionary->HasKey("applications")) {
    const base::ListValue* applications = nullptr;
    dictionary->GetList("applications", &applications);
    for (size_t i = 0; i < applications->GetSize(); ++i) {
      const base::DictionaryValue* child = nullptr;
      applications->GetDictionary(i, &child);
      const ApplicationInfo& child_info = DeserializeApplication(child);
      mojo_url_aliases_[child_info.url] =
          std::make_pair(info.url, GURL(child_info.url).host());
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

scoped_ptr<base::Value> PackageManager::ReadManifest(
    const base::FilePath& manifest_path) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;
  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return deserializer.Deserialize(&error, &message);
}

void PackageManager::OnReadManifest(const GURL& url,
                                    const ResolveMojoURLCallback& callback,
                                    scoped_ptr<base::Value> manifest) {
  if (manifest) {
    base::DictionaryValue* dictionary = nullptr;
    CHECK(manifest->GetAsDictionary(&dictionary));
    DeserializeApplication(dictionary);
    SerializeCatalog();
  }
  CompleteResolveMojoURL(url, callback);
}


}  // namespace package_manager
