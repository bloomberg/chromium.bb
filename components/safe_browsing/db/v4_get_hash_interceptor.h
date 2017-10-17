// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_INTERCEPTOR_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_INTERCEPTOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "net/url_request/url_request_interceptor.h"

class GURL;

namespace safe_browsing {

// This class intercepts requests to the V4 server, and responds with
// predetermined responses.
//
// Currently, it only supports a single response per HashPrefix. In reality,
// this class expects requested hash prefixes to be full hashes, which is
// relatively common in safe browsing test infrastructure. The class should be
// relatively easily changed to support true prefixes if necessary.
//
// Like all URLRequestInterceptors, it must be registered on the IO thread. The
// class has a helper method to do this for you.
class V4GetHashInterceptor : public net::URLRequestInterceptor {
 public:
  using ResponseMap = std::map<FullHash, ThreatMatch>;
  explicit V4GetHashInterceptor(const ResponseMap& response_map);

  // Convenience contructor if the caller only wants to test a single URL.
  V4GetHashInterceptor(const GURL& url, ThreatMatch match);
  ~V4GetHashInterceptor() override;

  // Helper function to post a task to the IO thread and register this
  // interceptor. To Be called on the UI thread.
  static void Register(std::unique_ptr<V4GetHashInterceptor> interceptor,
                       scoped_refptr<base::SequencedTaskRunner> io_task_runner);

 private:
  static void AddInterceptor(std::unique_ptr<V4GetHashInterceptor> interceptor);

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* delegate) const override;

  // This map holds responses the Safe Browsing server should be sending in
  // response to various full hash requests.
  ResponseMap response_map_;

  DISALLOW_COPY_AND_ASSIGN(V4GetHashInterceptor);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_GET_HASH_INTERCEPTOR_H_
