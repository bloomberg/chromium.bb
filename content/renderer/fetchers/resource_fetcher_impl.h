// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
#define CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/renderer/resource_fetcher.h"
#include "net/http/http_request_headers.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace content {

class ResourceFetcherImpl : public ResourceFetcher {
 public:
  // ResourceFetcher implementation:
  void SetMethod(const std::string& method) override;
  void SetBody(const std::string& body) override;
  void SetHeader(const std::string& header, const std::string& value) override;
  void Start(blink::WebLocalFrame* frame,
             blink::WebURLRequest::RequestContext request_context,
             const Callback& callback) override;
  void Start(blink::WebLocalFrame* frame,
             blink::WebURLRequest::RequestContext request_context,
             mojom::URLLoaderFactory* url_loader_factory,
             const net::NetworkTrafficAnnotationTag& annotation_tag,
             const Callback& callback,
             size_t maximum_download_size) override;
  void SetTimeout(const base::TimeDelta& timeout) override;

 private:
  friend class ResourceFetcher;

  class ClientImpl;

  explicit ResourceFetcherImpl(const GURL& url);

  ~ResourceFetcherImpl() override;

  void OnLoadComplete();
  void Cancel() override;

  std::unique_ptr<ClientImpl> client_;

  // Request to send.
  ResourceRequest request_;

  // HTTP headers to build a header string for |request_|.
  // TODO(toyoshim): Remove this member once ResourceRequest uses
  // net::HttpRequestHeaders instead of std::string for headers.
  net::HttpRequestHeaders headers_;

  // Limit how long to wait for the server.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
