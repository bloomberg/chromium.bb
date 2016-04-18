// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager()
    : v4_get_hash_protocol_manager_(NULL) {
}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  DCHECK(v4_get_hash_protocol_manager_ == NULL);
}

void SafeBrowsingDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (request_context_getter) {
    // Instantiate a V4GetHashProtocolManager.
    v4_get_hash_protocol_manager_ = V4GetHashProtocolManager::Create(
        request_context_getter, config);
  }
}

// |shutdown| not used. Destroys the v4 protocol managers. This may be called
// multiple times during the life of the DatabaseManager.
// Must be called on IO thread.
void SafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // This cancels all in-flight GetHash requests.
  if (v4_get_hash_protocol_manager_) {
    delete v4_get_hash_protocol_manager_;
    v4_get_hash_protocol_manager_ = NULL;
  }
}

void SafeBrowsingDatabaseManager::CheckApiBlacklistUrl(const GURL& url,
                                                       Client* client) {
  // TODO(kcarattini): Implement this.
}

}  // namespace safe_browsing
