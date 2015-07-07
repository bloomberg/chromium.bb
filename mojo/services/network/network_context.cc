// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_context.h"

#include <algorithm>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "mojo/common/user_agent.h"
#include "mojo/services/network/url_loader_impl.h"
#include "net/log/net_log_util.h"
#include "net/log/write_to_file_net_log_observer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace mojo {

namespace {
// Logs network information to the specified file.
const char kLogNetLog[] = "log-net-log";
}

class NetworkContext::MojoNetLog : public net::NetLog {
 public:
  MojoNetLog() {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(kLogNetLog))
      return;

    base::FilePath log_path = command_line->GetSwitchValuePath(kLogNetLog);
    base::ScopedFILE file;
#if defined(OS_WIN)
    file.reset(_wfopen(log_path.value().c_str(), L"w"));
#elif defined(OS_POSIX)
    file.reset(fopen(log_path.value().c_str(), "w"));
#endif
    if (!file) {
      LOG(ERROR) << "Could not open file " << log_path.value()
                 << " for net logging";
    } else {
      write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
      write_to_file_observer_->set_capture_mode(
          net::NetLogCaptureMode::IncludeCookiesAndCredentials());
      write_to_file_observer_->StartObserving(this, file.Pass(), nullptr,
                                              nullptr);
    }
  }

  ~MojoNetLog() override {
    if (write_to_file_observer_)
      write_to_file_observer_->StopObserving(nullptr);
  }

 private:
  scoped_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;

  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

NetworkContext::NetworkContext(
    scoped_ptr<net::URLRequestContext> url_request_context)
    : net_log_(new MojoNetLog),
      url_request_context_(url_request_context.Pass()),
      in_shutdown_(false) {
  url_request_context_->set_net_log(net_log_.get());
}

NetworkContext::NetworkContext(const base::FilePath& base_path)
    : NetworkContext(MakeURLRequestContext(base_path)) {
}

NetworkContext::~NetworkContext() {
  in_shutdown_ = true;
  // TODO(darin): Be careful about destruction order of member variables?

  // Call each URLLoaderImpl and ask it to release its net::URLRequest, as the
  // corresponding net::URLRequestContext is going away with this
  // NetworkContext. The loaders can be deregistering themselves in Cleanup(),
  // so iterate over a copy.
  for (auto& url_loader : url_loaders_) {
    url_loader->Cleanup();
  }
}

void NetworkContext::RegisterURLLoader(URLLoaderImpl* url_loader) {
  DCHECK(url_loaders_.count(url_loader) == 0);
  url_loaders_.insert(url_loader);
}

void NetworkContext::DeregisterURLLoader(URLLoaderImpl* url_loader) {
  if (!in_shutdown_) {
    size_t removed_count = url_loaders_.erase(url_loader);
    DCHECK(removed_count);
  }
}

size_t NetworkContext::GetURLLoaderCountForTesting() {
  return url_loaders_.size();
}

// static
scoped_ptr<net::URLRequestContext> NetworkContext::MakeURLRequestContext(
    const base::FilePath& base_path) {
  net::URLRequestContextBuilder builder;
  builder.set_accept_language("en-us,en");
  builder.set_user_agent(mojo::common::GetUserAgent());
  builder.set_proxy_service(net::ProxyService::CreateDirect());
  builder.set_transport_security_persister_path(base_path);

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
#if defined(OS_ANDROID)
  // On Android, we store the cache on disk becase we can run only a single
  // instance of the shell at a time.
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
  cache_params.path = base_path.Append(FILE_PATH_LITERAL("Cache"));
#else
  // On desktop, we store the cache in memory so we can run many shells
  // in parallel when running tests, otherwise the network services in each
  // shell will corrupt the disk cache.
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
#endif

  builder.EnableHttpCache(cache_params);
  builder.set_file_enabled(true);

  return make_scoped_ptr(builder.Build());
}

}  // namespace mojo
