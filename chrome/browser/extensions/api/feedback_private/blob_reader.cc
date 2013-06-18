// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/blob_reader.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

BlobReader::BlobReader(Profile* profile,
                       const GURL& blob_url,
                       BlobReadCallback callback)
    : callback_(callback) {
  fetcher_ = net::URLFetcher::Create(
      blob_url, net::URLFetcher::GET,
      this);
  fetcher_->SetRequestContext(profile->GetRequestContext());
}

BlobReader::~BlobReader() {
}

void BlobReader::Start() {
  fetcher_->Start();
}

// Overridden from net::URLFetcherDelegate.
void BlobReader::OnURLFetchComplete(const net::URLFetcher* source) {
  scoped_ptr<std::string> response(new std::string);
  source->GetResponseAsString(response.get());
  callback_.Run(response.Pass());

  delete this;
}
