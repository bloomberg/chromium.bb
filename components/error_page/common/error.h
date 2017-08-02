// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_ERROR_H_
#define COMPONENTS_ERROR_PAGE_COMMON_ERROR_H_

#include <string>

#include "url/gurl.h"

namespace error_page {

// Represents an error info necessary to show an error page.
// This class is a copiable value class.
class Error {
 public:
  // Use net::kErroDomain for network errors.
  // For http errors.
  static const char kHttpErrorDomain[];
  // For DNS probe errors.
  static const char kDnsProbeErrorDomain[];

  // Returns a net::kErrorDomain error.
  static Error NetError(const GURL& url, int reason, bool stale_copy_in_cache);
  // Returns a kHttpErrorDomain error.
  static Error HttpError(const GURL& url, int status);
  // Returns a kDnsProbeErrorDomain error.
  static Error DnsProbeError(const GURL& url,
                             int status,
                             bool stale_copy_in_cache);

  // Returns the url that failed to load.
  const GURL& url() const { return url_; }
  // Returns the domain of this error.
  const std::string& domain() const { return domain_; }
  // Returns a numeric error code. The meaning of this code depends on the
  // domain string.
  int reason() const { return reason_; }
  // Returns true if chrome has a stale cache entry for the url.
  bool stale_copy_in_cache() const { return stale_copy_in_cache_; }

 private:
  Error(const GURL& url,
        const std::string& domain,
        int reason,
        bool stale_copy_in_cache);

  GURL url_;
  std::string domain_;
  int reason_;
  bool stale_copy_in_cache_;
};

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_ERROR_H_
