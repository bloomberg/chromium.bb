// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blob_reader.h"

#include <limits>
#include <utility>

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

BlobReader::BlobReader(content::BrowserContext* browser_context,
                       const std::string& blob_uuid,
                       BlobReadCallback callback)
    : callback_(callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL blob_url;
  if (base::StartsWith(blob_uuid, "blob:blobinternal",
                       base::CompareCase::SENSITIVE)) {
    // TODO(michaeln): remove support for deprecated blob urls
    blob_url = GURL(blob_uuid);
  } else {
    blob_url = GURL(std::string("blob:uuid/") + blob_uuid);
  }
  DCHECK(blob_url.is_valid());

  // This network request is annotated with NO_TRAFFIC_ANNOTATION_YET as
  // it is scheduled to be removed in (crbug.com/701851).
  fetcher_ = net::URLFetcher::Create(blob_url, net::URLFetcher::GET, this,
                                     NO_TRAFFIC_ANNOTATION_YET);
  fetcher_->SetRequestContext(
      content::BrowserContext::GetDefaultStoragePartition(browser_context)
          ->GetURLRequestContext());
}

BlobReader::~BlobReader() { DCHECK_CURRENTLY_ON(content::BrowserThread::UI); }

void BlobReader::SetByteRange(int64_t offset, int64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_GE(offset, 0);
  CHECK_GT(length, 0);
  CHECK_LE(offset, std::numeric_limits<int64_t>::max() - length);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      base::StringPrintf("bytes=%" PRId64 "-%" PRId64, offset,
                         offset + length - 1));
  fetcher_->SetExtraRequestHeaders(headers.ToString());
}

void BlobReader::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  fetcher_->Start();
}

// Overridden from net::URLFetcherDelegate.
void BlobReader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<std::string> response(new std::string);
  int64_t first = 0, last = 0, length = 0;
  source->GetResponseAsString(response.get());
  source->GetResponseHeaders()->GetContentRangeFor206(&first, &last, &length);
  callback_.Run(std::move(response), length);

  delete this;
}
