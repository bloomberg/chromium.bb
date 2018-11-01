// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_
#define COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace base {
struct Feature;
}

namespace safe_search_api {

// The SafeSearch API classification of a URL.
enum class Classification { SAFE, UNSAFE };

// Visible for testing.
extern const base::Feature kAllowAllGoogleUrls;

// This class uses the SafeSearch API to check the SafeSearch classification
// of the content on a given URL and returns the result asynchronously
// via a callback.
class URLChecker {
 public:
  // Returns whether |url| should be blocked. Called from CheckURL.
  using CheckCallback = base::OnceCallback<
      void(const GURL&, Classification classification, bool /* uncertain */)>;

  // |country| should be a two-letter country code (ISO 3166-1 alpha-2), e.g.,
  // "us".
  URLChecker(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             const std::string& country);
  URLChecker(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             const std::string& country,
             size_t cache_size);
  URLChecker(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             const std::string& country,
             size_t cache_size,
             const std::string& api_key);
  ~URLChecker();

  // Returns whether |callback| was run synchronously.
  bool CheckURL(const GURL& url, CheckCallback callback);

  void SetCacheTimeoutForTesting(const base::TimeDelta& timeout) {
    cache_timeout_ = timeout;
  }

 private:
  struct Check;
  struct CheckResult {
    CheckResult(Classification classification, bool uncertain);
    Classification classification;
    bool uncertain;
    base::TimeTicks timestamp;
  };
  using CheckList = std::list<std::unique_ptr<Check>>;

  void OnSimpleLoaderComplete(CheckList::iterator it,
                              std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  const std::string country_;
  const std::string api_key_;

  CheckList checks_in_progress_;

  base::MRUCache<GURL, CheckResult> cache_;
  base::TimeDelta cache_timeout_;

  DISALLOW_COPY_AND_ASSIGN(URLChecker);
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_
