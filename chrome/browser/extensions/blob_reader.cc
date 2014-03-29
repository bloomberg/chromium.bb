// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blob_reader.h"

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

BlobReader::BlobReader(Profile* profile,
                       const std::string& blob_uuid,
                       BlobReadCallback callback)
    : callback_(callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL blob_url;
  if (StartsWithASCII(blob_uuid, "blob:blobinternal", true)) {
    // TODO(michaeln): remove support for deprecated blob urls
    blob_url = GURL(blob_uuid);
  } else {
    blob_url = GURL(std::string("blob:uuid/") + blob_uuid);
  }
  DCHECK(blob_url.is_valid());
  fetcher_.reset(net::URLFetcher::Create(
      blob_url, net::URLFetcher::GET,
      this));
  fetcher_->SetRequestContext(profile->GetRequestContext());
}

BlobReader::~BlobReader() { DCHECK_CURRENTLY_ON(content::BrowserThread::UI); }

void BlobReader::SetByteRange(int64 offset, int64 length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_GE(offset, 0);
  CHECK_GT(length, 0);
  CHECK_LE(offset, kint64max - length);

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
  scoped_ptr<std::string> response(new std::string);
  int64 first = 0, last = 0, length = 0;
  source->GetResponseAsString(response.get());
  source->GetResponseHeaders()->GetContentRange(&first, &last, &length);
  callback_.Run(response.Pass(), length);

  delete this;
}
