// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/alt_error_page_resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time/time.h"
#include "content/public/renderer/resource_fetcher.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

using blink::WebFrame;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

// Number of seconds to wait for the alternate error page server.  If it takes
// too long, just use the local error page.
static const int kDownloadTimeoutSec = 3;

AltErrorPageResourceFetcher::AltErrorPageResourceFetcher(
    const GURL& url,
    WebFrame* frame,
    const WebURLRequest& original_request,
    const WebURLError& original_error,
    const Callback& callback)
    : frame_(frame),
      callback_(callback),
      original_request_(original_request),
      original_error_(original_error) {
  fetcher_.reset(ResourceFetcher::Create(
      url, frame, WebURLRequest::TargetIsMainFrame,
      base::Bind(&AltErrorPageResourceFetcher::OnURLFetchComplete,
                 base::Unretained(this))));
  fetcher_->SetTimeout(base::TimeDelta::FromSeconds(kDownloadTimeoutSec));
}

AltErrorPageResourceFetcher::~AltErrorPageResourceFetcher() {
}

void AltErrorPageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  // A null response indicates a network error.
  if (!response.isNull() && response.httpStatusCode() == 200) {
    callback_.Run(frame_, original_request_, original_error_, data);
  } else {
    callback_.Run(frame_, original_request_, original_error_, std::string());
  }
}

}  // namespace content
