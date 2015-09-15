// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "mojo/fetcher/url_resolver.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/shell/application_fetcher.h"

namespace mojo {
namespace fetcher {

// This is the default implementation of shell::ApplicationFetcher. It loads
// http/s urls off the network as well as providing special handling for mojo:
// and about: urls.
class BaseApplicationFetcher : public shell::ApplicationFetcher {
 public:
  // mojo: urls are only supported if |shell_file_root| is non-empty.
  BaseApplicationFetcher(const base::FilePath& shell_file_root);
  ~BaseApplicationFetcher() override;

 private:
  // Overridden from shell::ApplicationFetcher:
  void SetApplicationManager(shell::ApplicationManager* manager) override;
  GURL ResolveURL(const GURL& url) override;
  void FetchRequest(
      URLRequestPtr request,
      const shell::Fetcher::FetchCallback& loader_callback) override;

  shell::ApplicationManager* application_manager_;
  scoped_ptr<URLResolver> url_resolver_;
  const bool disable_cache_;
  NetworkServicePtr network_service_;
  URLLoaderFactoryPtr url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(BaseApplicationFetcher);
};

}  // namespace fetcher
}  // namespace mojo
