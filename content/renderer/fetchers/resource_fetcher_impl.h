// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_
#define CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/public/renderer/resource_fetcher.h"
#include "content/renderer/fetchers/web_url_loader_client_impl.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

class GURL;

namespace blink {
class WebFrame;
class WebURLLoader;
struct WebURLError;
}

namespace content {

class ResourceFetcherImpl : public ResourceFetcher,
                            public WebURLLoaderClientImpl {
 public:
  // ResourceFetcher implementation:
  virtual void SetMethod(const std::string& method) OVERRIDE;
  virtual void SetBody(const std::string& body) OVERRIDE;
  virtual void SetHeader(const std::string& header,
                         const std::string& value) OVERRIDE;
  virtual void Start(blink::WebFrame* frame,
                     blink::WebURLRequest::RequestContext request_context,
                     blink::WebURLRequest::FrameType frame_type,
                     LoaderType loader_type,
                     const Callback& callback) OVERRIDE;
  virtual void SetTimeout(const base::TimeDelta& timeout) OVERRIDE;

 private:
  friend class ResourceFetcher;

  explicit ResourceFetcherImpl(const GURL& url);

  virtual ~ResourceFetcherImpl();

  // Callback for timer that limits how long we wait for the server.  If this
  // timer fires and the request hasn't completed, we kill the request.
  void TimeoutFired();

  // WebURLLoaderClientImpl methods:
  virtual void OnLoadComplete() OVERRIDE;
  virtual void Cancel() OVERRIDE;

  scoped_ptr<blink::WebURLLoader> loader_;

  // Request to send.  Released once Start() is called.
  blink::WebURLRequest request_;

  // Callback when we're done.
  Callback callback_;

  // Limit how long to wait for the server.
  base::OneShotTimer<ResourceFetcherImpl> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_H_
