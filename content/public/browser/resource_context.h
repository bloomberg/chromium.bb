// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

class GURL;

namespace appcache {
class AppCacheService;
}

namespace net {
class ClientCertStore;
class HostResolver;
class KeygenHandler;
class URLRequestContext;
}

namespace content {

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread. It must be destructed on the IO thread.
class CONTENT_EXPORT ResourceContext : public base::SupportsUserData {
 public:
#if defined(OS_IOS)
  virtual ~ResourceContext() {}
#else
  ResourceContext();
  virtual ~ResourceContext();
#endif
  virtual net::HostResolver* GetHostResolver() = 0;

  // DEPRECATED: This is no longer a valid given isolated apps/sites and
  // storage partitioning. This getter returns the default context associated
  // with a BrowsingContext.
  virtual net::URLRequestContext* GetRequestContext() = 0;

  // Get platform ClientCertStore. May return NULL.
  virtual scoped_ptr<net::ClientCertStore> CreateClientCertStore();

  // Create a platform KeygenHandler and pass it to |callback|. The |callback|
  // may be run synchronously.
  virtual void CreateKeygenHandler(
      uint32 key_size_in_bits,
      const std::string& challenge_string,
      const GURL& url,
      const base::Callback<void(scoped_ptr<net::KeygenHandler>)>& callback);

  // Returns true if microphone access is allowed for |origin|. Used to
  // determine what level of authorization is given to |origin| to access
  // resource metadata.
  virtual bool AllowMicAccess(const GURL& origin) = 0;

  // Returns true if web camera access is allowed for |origin|. Used to
  // determine what level of authorization is given to |origin| to access
  // resource metadata.
  virtual bool AllowCameraAccess(const GURL& origin) = 0;

  // Returns a random salt string that is used for creating media device IDs.
  // The salt should be stored in the current user profile and should be reset
  // if cookies are cleared. Returns an empty string per default.
  // TODO(perkj): Make this method pure virtual when crbug/315022 is fixed.
  virtual std::string GetMediaDeviceIDSalt();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
