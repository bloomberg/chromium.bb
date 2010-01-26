// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NET_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_SYNC_ENGINE_NET_SYNCAPI_SERVER_CONNECTION_MANAGER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"

namespace sync_api {

class HttpPostProviderFactory;

// This provides HTTP Post functionality through the interface provided
// to the sync API by the application hosting the syncer backend.
class SyncAPIBridgedPost
    : public browser_sync::ServerConnectionManager::Post {
 public:
  SyncAPIBridgedPost(browser_sync::ServerConnectionManager* scm,
                     HttpPostProviderFactory* factory)
      : Post(scm), factory_(factory) {
  }

  virtual ~SyncAPIBridgedPost() { }

  virtual bool Init(const char* path,
                    const std::string& auth_token,
                    const std::string& payload,
                    browser_sync::HttpResponse* response);

 private:
  // Pointer to the factory we use for creating HttpPostProviders. We do not
  // own |factory_|.
  HttpPostProviderFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncAPIBridgedPost);
};

// A ServerConnectionManager subclass used by the syncapi layer. We use a
// subclass so that we can override MakePost() to generate a POST object using
// an instance of the HttpPostProviderFactory class.
class SyncAPIServerConnectionManager
    : public browser_sync::ServerConnectionManager {
 public:
  // Takes ownership of factory.
  SyncAPIServerConnectionManager(const std::string& server,
                                 int port,
                                 bool use_ssl,
                                 const std::string& client_version,
                                 const std::string& client_id,
                                 HttpPostProviderFactory* factory)
      : ServerConnectionManager(server, port, use_ssl, client_version,
                                client_id),
        post_provider_factory_(factory) {
    DCHECK(post_provider_factory_.get());
  }

  virtual ~SyncAPIServerConnectionManager();
 protected:
  virtual Post* MakePost() {
    return new SyncAPIBridgedPost(this, post_provider_factory_.get());
  }
 private:
  // A factory creating concrete HttpPostProviders for use whenever we need to
  // issue a POST to sync servers.
  scoped_ptr<HttpPostProviderFactory> post_provider_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncAPIServerConnectionManager);
};

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_ENGINE_NET_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
