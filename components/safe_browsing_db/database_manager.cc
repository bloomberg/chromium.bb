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

  v4_get_hash_protocol_manager_ = V4GetHashProtocolManager::Create(
      request_context_getter, config);
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
  // TODO(kcarattini): Call back clients with pending requests.
}

bool SafeBrowsingDatabaseManager::CheckApiBlacklistUrl(const GURL& url,
                                                       Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(v4_get_hash_protocol_manager_);

  // Make sure we can check this url.
  if (!(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    return true;
  }

  // Compute a list of hashes for this url.
  std::vector<SBFullHash> full_hashes;
  UrlToFullHashes(url, false, &full_hashes);
  if (full_hashes.empty())
    return true;

  // Copy to prefixes.
  std::vector<SBPrefix> prefixes;
  for (const SBFullHash& full_hash : full_hashes) {
    prefixes.push_back(full_hash.prefix);
  }
  // Multiple full hashes could share a prefix, remove duplicates.
  std::sort(prefixes.begin(), prefixes.end());
  prefixes.erase(std::unique(prefixes.begin(), prefixes.end()), prefixes.end());
  DCHECK(!prefixes.empty());

  // TODO(kcarattini): Track checks in a map.
  std::shared_ptr<SafeBrowsingApiCheck> check(
      new SafeBrowsingApiCheck(url, full_hashes, client));

  // TODO(kcarattini): Implement cache compliance.
  v4_get_hash_protocol_manager_->GetFullHashesWithApis(prefixes,
      base::Bind(&SafeBrowsingDatabaseManager::HandleGetHashesWithApisResults,
                 base::Unretained(this), check));

  return false;
}

void SafeBrowsingDatabaseManager::HandleGetHashesWithApisResults(
    std::shared_ptr<SafeBrowsingApiCheck> check,
    const std::vector<SBFullHashResult>& full_hash_results,
    const base::TimeDelta& negative_cache_duration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(kcarattini): Implement response handler.
}

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::SafeBrowsingApiCheck(
    const GURL& url, const std::vector<SBFullHash>& full_hashes, Client* client)
        : url_(url), full_hashes_(full_hashes), client_(client) {
}

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::~SafeBrowsingApiCheck() {
}

}  // namespace safe_browsing
