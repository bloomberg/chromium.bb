// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/duplicate_content_resource_handler.h"

#include <set>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"

namespace content {
namespace {

class GlobalDuplicateRecords {
 public:
  static GlobalDuplicateRecords* GetInstance() {
    static base::LazyInstance<GlobalDuplicateRecords>::Leaky records;
    return records.Pointer();
  }

  std::set<MH_UINT32>* content_matches() {
    return &content_matches_;
  }

  std::set<MH_UINT32>* content_and_url_matches() {
    return &content_and_url_matches_;
  }

  GlobalDuplicateRecords() {}
  ~GlobalDuplicateRecords() {}

 private:
  std::set<MH_UINT32> content_matches_;
  std::set<MH_UINT32> content_and_url_matches_;
};

}  // namespace

DuplicateContentResourceHandler::DuplicateContentResourceHandler(
    scoped_ptr<ResourceHandler> next_handler,
    ResourceType::Type resource_type,
    net::URLRequest* request)
    : LayeredResourceHandler(next_handler.Pass()),
      resource_type_(resource_type),
      bytes_read_(0),
      request_(request),
      pmurhash_ph1_(0),
      pmurhash_pcarry_(0) {
}

DuplicateContentResourceHandler::~DuplicateContentResourceHandler() {
}

bool DuplicateContentResourceHandler::OnWillRead(int request_id,
                                                 net::IOBuffer** buf,
                                                 int* buf_size,
                                                 int min_size) {
  DCHECK_EQ(-1, min_size);

  if (!next_handler_->OnWillRead(request_id, buf, buf_size, min_size))
    return false;
  read_buffer_ = *buf;
  return true;
}

bool DuplicateContentResourceHandler::OnReadCompleted(int request_id,
                                                      int bytes_read,
                                                      bool* defer) {
  PMurHash32_Process(&pmurhash_ph1_, &pmurhash_pcarry_,
                      read_buffer_->data(), bytes_read);
  bytes_read_ += bytes_read;
  return next_handler_->OnReadCompleted(request_id, bytes_read, defer);
}

bool DuplicateContentResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {

  if (status.is_success())
    RecordContentMetrics();

  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void DuplicateContentResourceHandler::RecordContentMetrics() {
  MH_UINT32 contents_hash = PMurHash32_Result(pmurhash_ph1_,
                                              pmurhash_pcarry_, bytes_read_);

  bool is_http_or_https = request_->url().SchemeIs("http") ||
                          request_->url().SchemeIs("https");
  UMA_HISTOGRAM_BOOLEAN("Duplicate.IsHttpOrHttps", is_http_or_https);

  // Combine the contents_hash with the url, so we can test if future content
  // identical resources have the same original url or not.
  MH_UINT32 hashed_with_url;
  const std::string& url_spec = request_->url().spec();
  PMurHash32_Process(&pmurhash_ph1_, &pmurhash_pcarry_,
                     url_spec.data(), url_spec.length());
  hashed_with_url = PMurHash32_Result(pmurhash_ph1_, pmurhash_pcarry_,
                                      url_spec.length() + bytes_read_);

  DVLOG(4) << "url: " << url_spec;
  DVLOG(4) << "contents hash: " << contents_hash;
  DVLOG(4) << "hash with url: " << hashed_with_url;

  std::set<MH_UINT32>* content_matches =
      GlobalDuplicateRecords::GetInstance()->content_matches();
  std::set<MH_UINT32>* content_and_url_matches =
      GlobalDuplicateRecords::GetInstance()->content_and_url_matches();

  const bool did_match_contents = content_matches->count(contents_hash) > 0;
  const bool did_match_contents_and_url =
      content_and_url_matches->count(hashed_with_url) > 0;

  UMA_HISTOGRAM_BOOLEAN("Duplicate.Hits", did_match_contents);
  UMA_HISTOGRAM_BOOLEAN("Duplicate.HitsSameUrl",
                         did_match_contents && did_match_contents_and_url);
  UMA_HISTOGRAM_ENUMERATION("Duplicate.ResourceType.All", resource_type_,
                             ResourceType::LAST_TYPE);
  if (did_match_contents && !did_match_contents_and_url) {
    content_and_url_matches->insert(hashed_with_url);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Duplicate.Size.HashHitUrlMiss", bytes_read_,
                                 1, 0x7FFFFFFF, 50);
    UMA_HISTOGRAM_ENUMERATION("Duplicate.ResourceType.HashHitUrlMiss",
                               resource_type_, ResourceType::LAST_TYPE);
  }
  content_matches->insert(contents_hash);
  content_and_url_matches->insert(hashed_with_url);

  bytes_read_ = 0;
  read_buffer_ = NULL;
}

}  // namespace content
