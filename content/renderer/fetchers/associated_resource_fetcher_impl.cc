// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/associated_resource_fetcher_impl.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoader.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

namespace content {

// static
AssociatedResourceFetcher* AssociatedResourceFetcher::Create(const GURL& url) {
  return new AssociatedResourceFetcherImpl(url);
}

class AssociatedResourceFetcherImpl::ClientImpl
    : public blink::WebAssociatedURLLoaderClient {
 public:
  explicit ClientImpl(const Callback& callback)
      : completed_(false), status_(LOADING), callback_(callback) {}

  ~ClientImpl() override {}

  virtual void Cancel() { OnLoadCompleteInternal(LOAD_FAILED); }

  bool completed() const { return completed_; }

 private:
  enum LoadStatus {
    LOADING,
    LOAD_FAILED,
    LOAD_SUCCEEDED,
  };

  void OnLoadCompleteInternal(LoadStatus status) {
    DCHECK(!completed_);
    DCHECK_EQ(status_, LOADING);

    completed_ = true;
    status_ = status;

    if (callback_.is_null())
      return;

    // Take a reference to the callback as running the callback may lead to our
    // destruction.
    Callback callback = callback_;
    callback.Run(status_ == LOAD_FAILED ? blink::WebURLResponse() : response_,
                 status_ == LOAD_FAILED ? std::string() : data_);
  }

  // WebAssociatedURLLoaderClient methods:
  void DidReceiveResponse(const blink::WebURLResponse& response) override {
    DCHECK(!completed_);
    response_ = response;
  }
  void DidReceiveCachedMetadata(const char* data, int data_length) override {
    DCHECK(!completed_);
    DCHECK_GT(data_length, 0);
  }
  void DidReceiveData(const char* data, int data_length) override {
    // The WebAssociatedURLLoader will continue after a load failure.
    // For example, for an Access Control error.
    if (completed_)
      return;
    DCHECK_GT(data_length, 0);

    data_.append(data, data_length);
  }
  void DidFinishLoading(double finishTime) override {
    // The WebAssociatedURLLoader will continue after a load failure.
    // For example, for an Access Control error.
    if (completed_)
      return;
    OnLoadCompleteInternal(LOAD_SUCCEEDED);
  }
  void DidFail(const blink::WebURLError& error) override {
    OnLoadCompleteInternal(LOAD_FAILED);
  }

 private:
  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  blink::WebURLResponse response_;

  LoadStatus status_;

  // Callback when we're done.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

AssociatedResourceFetcherImpl::AssociatedResourceFetcherImpl(const GURL& url)
    : request_(url) {}

AssociatedResourceFetcherImpl::~AssociatedResourceFetcherImpl() {
  if (!loader_)
    return;

  DCHECK(client_);

  if (!client_->completed())
    loader_->Cancel();
}

void AssociatedResourceFetcherImpl::SetServiceWorkerMode(
    blink::WebURLRequest::ServiceWorkerMode service_worker_mode) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  request_.SetServiceWorkerMode(service_worker_mode);
}

void AssociatedResourceFetcherImpl::SetCacheMode(
    blink::mojom::FetchCacheMode mode) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  request_.SetCacheMode(mode);
}

void AssociatedResourceFetcherImpl::SetLoaderOptions(
    const blink::WebAssociatedURLLoaderOptions& options) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  options_ = options;
}

void AssociatedResourceFetcherImpl::Start(
    blink::WebLocalFrame* frame,
    blink::WebURLRequest::RequestContext request_context,
    network::mojom::FetchRequestMode fetch_request_mode,
    network::mojom::FetchCredentialsMode fetch_credentials_mode,
    network::mojom::RequestContextFrameType frame_type,
    const Callback& callback) {
  DCHECK(!loader_);
  DCHECK(!client_);
  DCHECK(!request_.IsNull());
  if (!request_.HttpBody().IsNull())
    DCHECK_NE("GET", request_.HttpMethod().Utf8()) << "GETs can't have bodies.";

  request_.SetRequestContext(request_context);
  request_.SetFrameType(frame_type);
  request_.SetSiteForCookies(frame->GetDocument().SiteForCookies());
  request_.SetFetchRequestMode(fetch_request_mode);
  request_.SetFetchCredentialsMode(fetch_credentials_mode);

  client_.reset(new ClientImpl(callback));

  loader_.reset(frame->CreateAssociatedURLLoader(options_));
  loader_->LoadAsynchronously(request_, client_.get());

  // No need to hold on to the request; reset it now.
  request_ = blink::WebURLRequest();
}

void AssociatedResourceFetcherImpl::Cancel() {
  loader_->Cancel();
  client_->Cancel();
}

}  // namespace content
