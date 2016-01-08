// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PACKAGE_MANAGER_PACKAGE_MANAGER_IMPL_H_
#define MOJO_SHELL_PACKAGE_MANAGER_PACKAGE_MANAGER_IMPL_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/shell/fetcher/url_resolver.h"
#include "mojo/shell/package_manager.h"

namespace base {
class TaskRunner;
}

namespace mojo {
class ContentHandler;
namespace shell {
class ContentHandlerConnection;
class Fetcher;
class Identity;

// This is the default implementation of PackageManager. It loads http/s urls
// off the network as well as providing special handling for mojo: and about:
// urls.
class PackageManagerImpl : public PackageManager {
 public:
  // mojo: urls are only supported if |shell_file_root| is non-empty.
  // |task_runner| is used by Fetchers created by the PackageManager to complete
  // file copies needed to obtain library paths that the ApplicationManager can
  // load. This can be null only in tests where application loading is handled
  // by custom ApplicationLoader implementations.
  PackageManagerImpl(const base::FilePath& shell_file_root,
                     base::TaskRunner* task_runner);
  ~PackageManagerImpl() override;

  // Register a content handler to handle content of |mime_type|.
  void RegisterContentHandler(const std::string& mime_type,
                              const GURL& content_handler_url);

  // Registers a package alias. When attempting to load |alias|, it will
  // instead redirect to |content_handler_package|, which is a content handler
  // which will be passed the |alias| as the URLResponse::url. Different values
  // of |alias| with the same |qualifier| that are in the same
  // |content_handler_package| will run in the same process in multi-process
  // mode.
  void RegisterApplicationPackageAlias(
      const GURL& alias,
      const GURL& content_handler_package,
      const std::string& qualifier);

 private:
  using ApplicationPackagedAlias = std::map<GURL, std::pair<GURL, std::string>>;
  using MimeTypeToURLMap = std::map<std::string, GURL>;
  using IdentityToContentHandlerMap =
      std::map<Identity, ContentHandlerConnection*>;

  // Overridden from PackageManager:
  void SetApplicationManager(ApplicationManager* manager) override;
  void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) override;
  uint32_t HandleWithContentHandler(
      Fetcher* fetcher,
      const Identity& source,
      const GURL& target_url,
      const CapabilityFilter& target_filter,
      InterfaceRequest<Application>* application_request) override;

  GURL ResolveURL(const GURL& url);
  bool ShouldHandleWithContentHandler(
      Fetcher* fetcher,
      const GURL& target_url,
      const CapabilityFilter& target_filter,
      Identity* content_handler_identity,
      URLResponsePtr* response) const;

  // Returns a running ContentHandler for |content_handler_identity|, if there
  // is not one running one is started for |source_identity|.
  ContentHandlerConnection* GetContentHandler(
      const Identity& content_handler_identity,
      const Identity& source_identity);

  void OnContentHandlerConnectionClosed(
      ContentHandlerConnection* content_handler);

  ApplicationManager* application_manager_;
  scoped_ptr<URLResolver> url_resolver_;
  const bool disable_cache_;
  NetworkServicePtr network_service_;
  URLLoaderFactoryPtr url_loader_factory_;
  ApplicationPackagedAlias application_package_alias_;
  MimeTypeToURLMap mime_type_to_url_;
  IdentityToContentHandlerMap identity_to_content_handler_;
  // Counter used to assign ids to content handlers.
  uint32_t content_handler_id_counter_;
  base::TaskRunner* task_runner_;
  base::FilePath shell_file_root_;

  DISALLOW_COPY_AND_ASSIGN(PackageManagerImpl);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_PACKAGE_MANAGER_PACKAGE_MANAGER_IMPL_H_
