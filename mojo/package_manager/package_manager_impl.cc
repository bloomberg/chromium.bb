// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/package_manager/package_manager_impl.h"

#include "base/bind.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/fetcher/about_fetcher.h"
#include "mojo/fetcher/data_fetcher.h"
#include "mojo/fetcher/local_fetcher.h"
#include "mojo/fetcher/network_fetcher.h"
#include "mojo/fetcher/switches.h"
#include "mojo/fetcher/update_fetcher.h"
#include "mojo/package_manager/content_handler_connection.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/query_util.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace package_manager {

PackageManagerImpl::PackageManagerImpl(
    const base::FilePath& shell_file_root,
    base::TaskRunner* task_runner)
    : application_manager_(nullptr),
      disable_cache_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCache)),
      content_handler_id_counter_(0u),
      task_runner_(task_runner) {
  if (!shell_file_root.empty()) {
    GURL mojo_root_file_url =
        util::FilePathToFileURL(shell_file_root).Resolve(std::string());
    url_resolver_.reset(new fetcher::URLResolver(mojo_root_file_url));
  }
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

void PackageManagerImpl::SetApplicationManager(
    shell::ApplicationManager* manager) {
  application_manager_ = manager;
}

void PackageManagerImpl::FetchRequest(
    URLRequestPtr request,
    const shell::Fetcher::FetchCallback& loader_callback) {
  GURL url(request->url);
  if (url.SchemeIs(fetcher::AboutFetcher::kAboutScheme)) {
    fetcher::AboutFetcher::Start(url, loader_callback);
    return;
  }

  if (url.SchemeIs(url::kDataScheme)) {
    fetcher::DataFetcher::Start(url, loader_callback);
    return;
  }

  GURL resolved_url = ResolveURL(url);
  if (resolved_url.SchemeIsFile()) {
    // LocalFetcher uses the network service to infer MIME types from URLs.
    // Skip this for mojo URLs to avoid recursively loading the network service.
    if (!network_service_ && !url.SchemeIs("mojo") && !url.SchemeIs("exe")) {
      shell::ConnectToService(application_manager_,
                              GURL("mojo:network_service"), &network_service_);
    }
    // Ownership of this object is transferred to |loader_callback|.
    // TODO(beng): this is eff'n weird.
    new fetcher::LocalFetcher(
        network_service_.get(), resolved_url,
        shell::GetBaseURLAndQuery(resolved_url, nullptr),
        loader_callback);
    return;
  }

#if 0
  // TODO(beng): figure out how this should be integrated now that mapped_url
  //             is toast.
  // TODO(scottmg): to quote someone I know, if you liked this you shouldda put
  //                a test on it.
  if (url.SchemeIs("mojo") &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseUpdater)) {
    shell::ConnectToService(application_manager_, GURL("mojo:updater"),
                            &updater_);
    // Ownership of this object is transferred to |loader_callback|.
    // TODO(beng): this is eff'n weird.
    new fetcher::UpdateFetcher(url, updater_.get(), loader_callback);
    return;
  }
#endif

  if (!url_loader_factory_) {
    shell::ConnectToService(application_manager_, GURL("mojo:network_service"),
                            &url_loader_factory_);
  }

  // Ownership of this object is transferred to |loader_callback|.
  // TODO(beng): this is eff'n weird.
  new fetcher::NetworkFetcher(disable_cache_, request.Pass(),
                              url_loader_factory_.get(), loader_callback);
}

uint32_t PackageManagerImpl::HandleWithContentHandler(
    shell::Fetcher* fetcher,
    const shell::Identity& source,
    const GURL& target_url,
    const shell::CapabilityFilter& target_filter,
    InterfaceRequest<Application>* application_request) {
  shell::Identity content_handler_identity;
  URLResponsePtr response;
  if (ShouldHandleWithContentHandler(fetcher,
                                     target_url,
                                     target_filter,
                                     &content_handler_identity,
                                     &response)) {
    ContentHandlerConnection* connection =
        GetContentHandler(content_handler_identity, source);
    connection->StartApplication(application_request->Pass(), response.Pass());
    return connection->id();
  }
  return Shell::kInvalidContentHandlerID;
}

GURL PackageManagerImpl::ResolveURL(const GURL& url) {
  return url_resolver_.get() ? url_resolver_->ResolveMojoURL(url) : url;
}

bool PackageManagerImpl::ShouldHandleWithContentHandler(
  shell::Fetcher* fetcher,
  const GURL& target_url,
  const shell::CapabilityFilter& target_filter,
  shell::Identity* content_handler_identity,
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
    *content_handler_identity = shell::Identity(
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
    *content_handler_identity = shell::Identity(
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
    *content_handler_identity = shell::Identity(
        alias_iter->second.first,
        use_real_qualifier ? alias_iter->second.second : std::string(),
        target_filter);
    return true;
  }

  return false;
}

ContentHandlerConnection* PackageManagerImpl::GetContentHandler(
    const shell::Identity& content_handler_identity,
    const shell::Identity& source_identity) {
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

}  // namespace package_manager
}  // namespace mojo
