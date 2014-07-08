// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_
#define MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestContext;
}

namespace mojo {

class NetworkContext {
 public:
  explicit NetworkContext(const base::FilePath& base_path);
  ~NetworkContext();

  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

 private:
  scoped_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_CONTEXT_H_
