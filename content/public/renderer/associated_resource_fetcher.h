// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_ASSOCIATED_RESOURCE_FETCHER_H_
#define CONTENT_PUBLIC_RENDERER_ASSOCIATED_RESOURCE_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCachePolicy.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

class GURL;

namespace blink {
class WebLocalFrame;
class WebURLResponse;
struct WebAssociatedURLLoaderOptions;
}

namespace content {

// Interface to download resources asynchronously.
class CONTENT_EXPORT AssociatedResourceFetcher {
 public:
  virtual ~AssociatedResourceFetcher() {}

  // This will be called asynchronously after the URL has been fetched,
  // successfully or not.  If there is a failure, response and data will both be
  // empty.  |response| and |data| are both valid until the URLFetcher instance
  // is destroyed.
  typedef base::Callback<void(const blink::WebURLResponse& response,
                              const std::string& data)>
      Callback;

  // Creates a AssociatedResourceFetcher for the specified resource.  Caller
  // takes
  // ownership of the returned object.  Deleting the AssociatedResourceFetcher
  // will cancel
  // the request, and the callback will never be run.
  static AssociatedResourceFetcher* Create(const GURL& url);

  virtual void SetServiceWorkerMode(
      blink::WebURLRequest::ServiceWorkerMode service_worker_mode) = 0;
  virtual void SetCachePolicy(blink::WebCachePolicy policy) = 0;

  // Associate the corresponding WebURLLoaderOptions to the loader. Must be
  // called before Start. Used if the LoaderType is FRAME_ASSOCIATED_LOADER.
  virtual void SetLoaderOptions(
      const blink::WebAssociatedURLLoaderOptions& options) = 0;

  // Starts the request using the specified frame.  Calls |callback| when
  // done.
  //
  // |fetch_request_mode| is the mode to use. See
  // https://fetch.spec.whatwg.org/#concept-request-mode.
  //
  // |fetch_credentials_mode| is the credentials mode to use. See
  // https://fetch.spec.whatwg.org/#concept-request-credentials-mode
  virtual void Start(
      blink::WebLocalFrame* frame,
      blink::WebURLRequest::RequestContext request_context,
      blink::WebURLRequest::FetchRequestMode fetch_request_mode,
      blink::WebURLRequest::FetchCredentialsMode fetch_credentials_mode,
      blink::WebURLRequest::FrameType frame_type,
      const Callback& callback) = 0;

  // Manually cancel the request.
  virtual void Cancel() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_ASSOCIATED_RESOURCE_FETCHER_H_
