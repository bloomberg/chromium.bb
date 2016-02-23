// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace safe_browsing {

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager()
    : SafeBrowsingDatabaseManager(NULL, V4ProtocolConfig()) {
}

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  // Instantiate a V4GetHashProtocolManager.
  if (request_context_getter) {
    v4_get_hash_protocol_manager_.reset(V4GetHashProtocolManager::Create(
        request_context_getter, config));
  }
}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
}

void SafeBrowsingDatabaseManager::CheckApiBlacklistUrl(const GURL& url,
                                                       Client* client) {
  // TODO(kcarattini): Implement this.
}

}  // namespace safe_browsing
