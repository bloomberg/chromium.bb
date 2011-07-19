// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NET_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_SYNC_ENGINE_NET_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"

namespace sync_api {

class HttpPostProviderFactory;

// This provides HTTP Post functionality through the interface provided
// to the sync API by the application hosting the syncer backend.
class SyncAPIBridgedPost
    : public browser_sync::ServerConnectionManager::Post {
 public:
  SyncAPIBridgedPost(browser_sync::ServerConnectionManager* scm,
                     HttpPostProviderFactory* factory);

  virtual ~SyncAPIBridgedPost();

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
                                 HttpPostProviderFactory* factory);
  virtual ~SyncAPIServerConnectionManager();

 protected:
  virtual Post* MakePost();

 private:
  // A factory creating concrete HttpPostProviders for use whenever we need to
  // issue a POST to sync servers.
  scoped_ptr<HttpPostProviderFactory> post_provider_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncAPIServerConnectionManager);
};

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_ENGINE_NET_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
