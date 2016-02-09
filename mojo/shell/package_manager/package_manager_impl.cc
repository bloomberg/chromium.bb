// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/package_manager/package_manager_impl.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/fetcher/about_fetcher.h"
#include "mojo/shell/fetcher/data_fetcher.h"
#include "mojo/shell/fetcher/local_fetcher.h"
#include "mojo/shell/fetcher/network_fetcher.h"
#include "mojo/shell/fetcher/switches.h"
#include "mojo/shell/package_manager/content_handler_connection.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"
#include "mojo/shell/query_util.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {
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

PackageManagerImpl::PackageManagerImpl(
    const base::FilePath& shell_file_root,
    base::TaskRunner* task_runner,
    ApplicationCatalogStore* catalog_store)
    : application_manager_(nullptr),
      disable_cache_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCache)),
      content_handler_id_counter_(0u),
      task_runner_(task_runner),
      shell_file_root_(shell_file_root),
      catalog_store_(catalog_store) {
  if (!shell_file_root.empty()) {
    GURL mojo_root_file_url =
        util::FilePathToFileURL(shell_file_root).Resolve(std::string());
    url_resolver_.reset(new URLResolver(mojo_root_file_url));
  }
  DeserializeCatalog();
}

PackageManagerImpl::~PackageManagerImpl() {
  IdentityToContentHandlerMap identity_to_content_handler(
      identity_to_content_handler_);
  for (auto& pair : identity_to_content_handler)
    pair.second->CloseConnection();
}

void PackageManagerImpl::RegisterContentHandler(
    const std::string& mime_type,
    const GURL& content_handler_url) {
  DCHECK(content_handler_url.is_valid())
      << "Content handler URL is invalid for mime type " << mime_type;
  mime_type_to_url_[mime_type] = content_handler_url;
}

void PackageManagerImpl::RegisterApplicationPackageAlias(
    const GURL& alias,
    const GURL& content_handler_package,
    const std::string& qualifier) {
  application_package_alias_[alias] =
      std::make_pair(content_handler_package, qualifier);
}

void PackageManagerImpl::SetApplicationManager(ApplicationManager* manager) {
  application_manager_ = manager;
}

void PackageManagerImpl::FetchRequest(
    URLRequestPtr request,
    const Fetcher::FetchCallback& loader_callback) {
  GURL url(request->url.get());
  if (url.SchemeIs(AboutFetcher::kAboutScheme)) {
    AboutFetcher::Start(url, loader_callback);
    return;
  }

  if (url.SchemeIs(url::kDataScheme)) {
    DataFetcher::Start(url, loader_callback);
    return;
  }

  GURL resolved_url = ResolveURL(url);
  if (resolved_url.SchemeIsFile()) {
    // LocalFetcher uses the network service to infer MIME types from URLs.
    // Skip this for mojo URLs to avoid recursively loading the network service.
    if (!network_service_ && !url.SchemeIs("mojo") && !url.SchemeIs("exe")) {
      ConnectToService(application_manager_, GURL("mojo:network_service"),
                       &network_service_);
    }
    // Ownership of this object is transferred to |loader_callback|.
    // TODO(beng): this is eff'n weird.
    new LocalFetcher(network_service_.get(), resolved_url,
                     GetBaseURLAndQuery(resolved_url, nullptr),
                     shell_file_root_, loader_callback);

    // TODO(beng): Determine if this is in the right place, and block
    //             establishing the connection on receiving a complete manifest.
    EnsureURLInCatalog(url);
    return;
  }

  if (!url_loader_factory_) {
    ConnectToService(application_manager_, GURL("mojo:network_service"),
                     &url_loader_factory_);
  }

  // Ownership of this object is transferred to |loader_callback|.
  // TODO(beng): this is eff'n weird.
  new NetworkFetcher(disable_cache_, std::move(request),
                     url_loader_factory_.get(), loader_callback);
}

uint32_t PackageManagerImpl::HandleWithContentHandler(
    Fetcher* fetcher,
    const Identity& source,
    const GURL& target_url,
    const CapabilityFilter& target_filter,
    InterfaceRequest<mojom::ShellClient>* request) {
  Identity content_handler_identity;
  URLResponsePtr response;
  if (ShouldHandleWithContentHandler(fetcher,
                                     target_url,
                                     target_filter,
                                     &content_handler_identity,
                                     &response)) {
    ContentHandlerConnection* connection =
        GetContentHandler(content_handler_identity, source);
    connection->StartApplication(std::move(*request), std::move(response));
    return connection->id();
  }
  return mojom::Shell::kInvalidApplicationID;
}

bool PackageManagerImpl::IsURLInCatalog(const std::string& url) const {
  return catalog_.find(url) != catalog_.end();
}

std::string PackageManagerImpl::GetApplicationName(
    const std::string& url) const {
  auto it = catalog_.find(url);
  return it != catalog_.end() ? it->second.name : url;
}

GURL PackageManagerImpl::ResolveURL(const GURL& url) {
  return url_resolver_.get() ? url_resolver_->ResolveMojoURL(url) : url;
}

bool PackageManagerImpl::ShouldHandleWithContentHandler(
  Fetcher* fetcher,
  const GURL& target_url,
  const CapabilityFilter& target_filter,
  Identity* content_handler_identity,
  URLResponsePtr* response) const {
  // TODO(beng): it seems like some delegate should/would want to have a say in
  //             configuring the qualifier also.
  // Why can't we use the real qualifier in single process mode? Because of
  // base::AtExitManager. If you link in ApplicationRunner into your code, and
  // then we make initialize multiple copies of the application, we end up
  // with multiple AtExitManagers and will check on the second one being
  // created.
  //
  // Why doesn't that happen when running different apps? Because
  // your_thing.mojo!base::AtExitManager and
  // my_thing.mojo!base::AtExitManager are different symbols.
  bool use_real_qualifier = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMultiprocess);

  GURL content_handler_url;
  // The response begins with a #!mojo <content-handler-url>.
  std::string shebang;
  if (fetcher->PeekContentHandler(&shebang, &content_handler_url)) {
    *response = fetcher->AsURLResponse(task_runner_,
                                       static_cast<int>(shebang.size()));
    *content_handler_identity = Identity(
        content_handler_url,
        use_real_qualifier ? (*response)->site.To<std::string>()
                           : std::string(),
        target_filter);
    return true;
  }

  // The response MIME type matches a registered content handler.
  auto iter = mime_type_to_url_.find(fetcher->MimeType());
  if (iter != mime_type_to_url_.end()) {
    *response = fetcher->AsURLResponse(task_runner_, 0);
    *content_handler_identity = Identity(
        iter->second,
        use_real_qualifier ? (*response)->site.To<std::string>()
                           : std::string(),
        target_filter);
    return true;
  }

  // The response URL matches a registered content handler.
  auto alias_iter = application_package_alias_.find(target_url);
  if (alias_iter != application_package_alias_.end()) {
    // We replace the qualifier with the one our package alias requested.
    *response = URLResponse::New();
    (*response)->url = target_url.spec();
    *content_handler_identity = Identity(
        alias_iter->second.first,
        use_real_qualifier ? alias_iter->second.second : std::string(),
        target_filter);
    return true;
  }

  return false;
}

ContentHandlerConnection* PackageManagerImpl::GetContentHandler(
    const Identity& content_handler_identity,
    const Identity& source_identity) {
  auto it = identity_to_content_handler_.find(content_handler_identity);
  if (it != identity_to_content_handler_.end())
    return it->second;

  ContentHandlerConnection* connection = new ContentHandlerConnection(
      application_manager_, source_identity,
      content_handler_identity,
      ++content_handler_id_counter_,
      base::Bind(&PackageManagerImpl::OnContentHandlerConnectionClosed,
                 base::Unretained(this)));
  identity_to_content_handler_[content_handler_identity] = connection;
  return connection;
}

void PackageManagerImpl::OnContentHandlerConnectionClosed(
    ContentHandlerConnection* connection) {
  // Remove the mapping.
  auto it = identity_to_content_handler_.find(connection->identity());
  DCHECK(it != identity_to_content_handler_.end());
  identity_to_content_handler_.erase(it);
}

void PackageManagerImpl::EnsureURLInCatalog(const GURL& url) {
  if (IsURLInCatalog(url.spec()))
    return;

  GURL manifest_url = url_resolver_->ResolveMojoManifest(url);
  if (manifest_url.is_empty())
    return;
  base::FilePath manifest_path;
  CHECK(net::FileURLToFilePath(manifest_url, &manifest_path));
  base::PostTaskAndReplyWithResult(
      task_runner_, FROM_HERE,
      base::Bind(&PackageManagerImpl::ReadManifest, base::Unretained(this),
                 manifest_path),
      base::Bind(&PackageManagerImpl::OnReadManifest,
                 base::Unretained(this)));
}

void PackageManagerImpl::DeserializeCatalog() {
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

void PackageManagerImpl::SerializeCatalog() {
  scoped_ptr<base::ListValue> catalog(new base::ListValue);
  for (const auto& info : catalog_) {
    base::DictionaryValue* dictionary = nullptr;
    SerializeEntry(info.second, &dictionary);
    catalog->Append(make_scoped_ptr(dictionary));
  }
  if (catalog_store_)
    catalog_store_->UpdateStore(std::move(catalog));
}

const ApplicationInfo& PackageManagerImpl::DeserializeApplication(
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
      GURL child_url(child_info.url);
      RegisterApplicationPackageAlias(child_url, GURL(info.url),
                                      child_url.host());
    }
  }
  return catalog_[info.url];
}

scoped_ptr<base::Value> PackageManagerImpl::ReadManifest(
    const base::FilePath& manifest_path) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;
  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return deserializer.Deserialize(&error, &message);
}

void PackageManagerImpl::OnReadManifest(scoped_ptr<base::Value> manifest) {
  if (!manifest)
    return;

  base::DictionaryValue* dictionary = nullptr;
  CHECK(manifest->GetAsDictionary(&dictionary));
  DeserializeApplication(dictionary);
  SerializeCatalog();
}

}  // namespace shell
}  // namespace mojo
