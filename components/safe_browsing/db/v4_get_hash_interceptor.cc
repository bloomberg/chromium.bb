// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/v4_get_hash_interceptor.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

// This method parses a request URL and returns a vector of HashPrefixes that
// were being requested. It does this by:
// 1. Finding the "req" query param.
// 2. Base64 decoding it.
// 3. Parsing the FindFullHashesRequest from the decoded string.
std::vector<HashPrefix> GetPrefixesForRequest(const GURL& url) {
  base::StringPiece query = url.query_piece();
  std::vector<base::StringPiece> split =
      base::SplitStringPiece(query, "&", base::TRIM_WHITESPACE,
                             base::SplitResult::SPLIT_WANT_NONEMPTY);
  base::StringPiece req_query = split[0];
  DCHECK(base::StartsWith(req_query, "$req=", base::CompareCase::SENSITIVE))
      << req_query;
  size_t req_begin = req_query.find("=") + 1u;
  size_t req_size = req_query.size() - req_begin;
  base::StringPiece req_64 = req_query.substr(req_begin, req_size);

  std::string decoded_output;
  bool success = base::Base64UrlDecode(
      req_64, base::Base64UrlDecodePolicy::REQUIRE_PADDING, &decoded_output);
  DCHECK(success);
  FindFullHashesRequest full_hash_req;
  success = full_hash_req.ParseFromString(decoded_output);
  DCHECK(success);

  const ThreatInfo& info = full_hash_req.threat_info();
  std::vector<HashPrefix> prefixes;
  for (int i = 0; i < info.threat_entries_size(); ++i) {
    prefixes.push_back(info.threat_entries(i).hash());
  }
  return prefixes;
}

}  // namespace

// static
void V4GetHashInterceptor::Register(
    std::unique_ptr<V4GetHashInterceptor> interceptor,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  io_task_runner->PostTask(FROM_HERE,
                           base::Bind(&V4GetHashInterceptor::AddInterceptor,
                                      base::Passed(&interceptor)));
}

void V4GetHashInterceptor::AddInterceptor(
    std::unique_ptr<V4GetHashInterceptor> interceptor) {
  const GURL url(kSbV4UrlPrefix);
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      url.scheme(), url.host(), std::move(interceptor));
}

V4GetHashInterceptor::V4GetHashInterceptor(const ResponseMap& response_map)
    : response_map_(response_map) {}

V4GetHashInterceptor::V4GetHashInterceptor(const GURL& url, ThreatMatch match)
    : V4GetHashInterceptor({{GetFullHash(url), match}}) {}

V4GetHashInterceptor::~V4GetHashInterceptor() {}

net::URLRequestJob* V4GetHashInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* delegate) const {
  const char kHeaders[] = "HTTP/1.1 200 OK\n\n";

  FindFullHashesResponse find_full_hashes_response;
  find_full_hashes_response.mutable_negative_cache_duration()->set_seconds(600);

  // Mock a response based on |response_map_| and the prefixes scraped from the
  // request URL.
  std::vector<HashPrefix> request_prefixes =
      GetPrefixesForRequest(request->url());
  for (const HashPrefix& prefix : request_prefixes) {
    auto it = response_map_.find(prefix);
    if (it == response_map_.end())
      continue;

    ThreatMatch* match = find_full_hashes_response.add_matches();
    *match = it->second;
  }
  std::string serialized_response;
  find_full_hashes_response.SerializeToString(&serialized_response);

  return new net::URLRequestTestJob(request, delegate, kHeaders,
                                    serialized_response,
                                    true /* auto_advance */);
}

}  // namespace safe_browsing
