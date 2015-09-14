// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/fetcher/base_application_fetcher.h"

#include "mojo/fetcher/about_fetcher.h"
#include "mojo/fetcher/local_fetcher.h"
#include "mojo/fetcher/network_fetcher.h"
#include "mojo/fetcher/switches.h"
#include "mojo/fetcher/update_fetcher.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/query_util.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace mojo {
namespace fetcher {

BaseApplicationFetcher::BaseApplicationFetcher(
    const base::FilePath& shell_file_root)
    : application_manager_(nullptr),
      disable_cache_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCache)) {
  if (!shell_file_root.empty()) {
    GURL mojo_root_file_url =
        util::FilePathToFileURL(shell_file_root).Resolve(std::string());
    url_resolver_.reset(new URLResolver(mojo_root_file_url));
  }
}

BaseApplicationFetcher::~BaseApplicationFetcher() {
}

void BaseApplicationFetcher::SetApplicationManager(
    shell::ApplicationManager* manager) {
  application_manager_ = manager;
}

GURL BaseApplicationFetcher::ResolveURL(const GURL& url) {
  return url_resolver_.get() ? url_resolver_->ResolveMojoURL(url) : url;
}

void BaseApplicationFetcher::FetchRequest(
    URLRequestPtr request,
    const shell::Fetcher::FetchCallback& loader_callback) {
  GURL url(request->url);
  if (url.SchemeIs(AboutFetcher::kAboutScheme)) {
    AboutFetcher::Start(url, loader_callback);
    return;
  }

  GURL resolved_url = ResolveURL(url);

  if (resolved_url.SchemeIsFile()) {
    // LocalFetcher uses the network service to infer MIME types from URLs.
    // Skip this for mojo URLs to avoid recursively loading the network service.
    if (!network_service_ && !url.SchemeIs("mojo")) {
      application_manager_->ConnectToService(GURL("mojo:network_service"),
                                            &network_service_);
    }
    // Ownership of this object is transferred to |loader_callback|.
    // TODO(beng): this is eff'n weird.
    new LocalFetcher(
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
    application_manager_->ConnectToService(GURL("mojo:updater"), &updater_);
    // Ownership of this object is transferred to |loader_callback|.
    // TODO(beng): this is eff'n weird.
    new UpdateFetcher(url, updater_.get(), loader_callback);
    return;
  }
#endif

  if (!url_loader_factory_) {
    application_manager_->ConnectToService(GURL("mojo:network_service"),
                                           &url_loader_factory_);
  }

  // Ownership of this object is transferred to |loader_callback|.
  // TODO(beng): this is eff'n weird.
  new NetworkFetcher(disable_cache_, request.Pass(), url_loader_factory_.get(),
                     loader_callback);
}

}  // namespace fetcher
}  // namespace mojo
