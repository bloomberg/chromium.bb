// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_

#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace appcache {
class AppCacheService;
}

namespace net {
class HostResolver;
class URLRequestContext;
}

namespace content {

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread. It must be destructed on the IO thread.
class CONTENT_EXPORT ResourceContext : public base::SupportsUserData {
 public:
  static appcache::AppCacheService* GetAppCacheService(
      ResourceContext* resource_context);

  ResourceContext();
  virtual ~ResourceContext();
  virtual net::HostResolver* GetHostResolver() = 0;
  virtual net::URLRequestContext* GetRequestContext() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
