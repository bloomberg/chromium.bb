// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_
#define CONTENT_PUBLIC_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_

#include <memory>

#include "content/common/content_export.h"

class PrefService;

namespace net {
class URLRequestContext;
}

namespace content {

// This owns a net::URLRequestContext and other state that's used with it.
struct CONTENT_EXPORT URLRequestContextOwner {
  URLRequestContextOwner();
  ~URLRequestContextOwner();
  URLRequestContextOwner(URLRequestContextOwner&& other);
  URLRequestContextOwner& operator=(URLRequestContextOwner&& other);

  // This needs to be destroyed after the URLRequestContext.
  std::unique_ptr<PrefService> pref_service;

  std::unique_ptr<net::URLRequestContext> url_request_context;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_
