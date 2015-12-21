// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_
#define MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_

#include <stddef.h>

#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestContext;
}

namespace mojo {
class URLLoader;
class URLLoaderImpl;
class NetworkServiceDelegate;

class NetworkContext {
 public:
  explicit NetworkContext(
      scoped_ptr<net::URLRequestContext> url_request_context);
  NetworkContext(
      const base::FilePath& base_path,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      NetworkServiceDelegate* delegate);
  ~NetworkContext();

  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

  // These are called by individual url loaders as they are being created and
  // destroyed.
  void RegisterURLLoader(URLLoaderImpl* url_loader);
  void DeregisterURLLoader(URLLoaderImpl* url_loader);

 private:
  friend class UrlLoaderImplTest;
  size_t GetURLLoaderCountForTesting();

  static scoped_ptr<net::URLRequestContext> MakeURLRequestContext(
      const base::FilePath& base_path,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      NetworkServiceDelegate* delegate);

  class MojoNetLog;
  scoped_ptr<class MojoNetLog> net_log_;

  scoped_ptr<net::URLRequestContext> url_request_context_;
  // URLLoaderImpls register themselves with the NetworkContext so that they can
  // be cleaned up when the NetworkContext goes away. This is needed as
  // net::URLRequests held by URLLoaderImpls have to be gone when
  // net::URLRequestContext (held by NetworkContext) is destroyed.
  std::set<URLLoaderImpl*> url_loaders_;

  // Set when entering the destructor, in order to avoid manipulations of the
  // |url_loaders_| (as a url_loader might delete itself in Cleanup()).
  bool in_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_
