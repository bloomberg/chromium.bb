// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_
#define CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/resource_response.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/redirect_info.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "url/gurl.h"

namespace content {

class ResourceDispatcher;
struct ResourceResponseInfo;

// PlzNavigate: Used to override parameters of the navigation request.
struct CONTENT_EXPORT StreamOverrideParameters {
 public:
  StreamOverrideParameters();
  ~StreamOverrideParameters();

  GURL stream_url;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ResourceResponseHead response;
  std::vector<GURL> redirects;
  std::vector<ResourceResponseInfo> redirect_responses;
  std::vector<net::RedirectInfo> redirect_infos;

  // The delta between the actual transfer size and the one reported by the
  // AsyncResourceLoader due to not having the ResourceResponse.
  int total_transfer_size_delta;

  int total_transferred = 0;

  // Called when this struct is deleted. Used to notify the browser that it can
  // release its associated StreamHandle.
  base::OnceCallback<void(const GURL&)> on_delete;
};

class CONTENT_EXPORT WebURLLoaderImpl
    : public NON_EXPORTED_BASE(blink::WebURLLoader) {
 public:

  // Takes ownership of |web_task_runner|.
  WebURLLoaderImpl(ResourceDispatcher* resource_dispatcher,
                   mojom::URLLoaderFactory* url_loader_factory);
  ~WebURLLoaderImpl() override;

  static void PopulateURLResponse(const GURL& url,
                                  const ResourceResponseInfo& info,
                                  blink::WebURLResponse* response,
                                  bool report_security_info);
  static blink::WebURLRequest PopulateURLRequestForRedirect(
      const blink::WebURLRequest& request,
      const net::RedirectInfo& redirect_info,
      blink::WebURLRequest::ServiceWorkerMode service_worker_mode);

  // WebURLLoader methods:
  void LoadSynchronously(const blink::WebURLRequest& request,
                         blink::WebURLResponse& response,
                         blink::WebURLError& error,
                         blink::WebData& data,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length) override;
  void LoadAsynchronously(const blink::WebURLRequest& request,
                          blink::WebURLLoaderClient* client) override;
  void Cancel() override;
  void SetDefersLoading(bool value) override;
  void DidChangePriority(blink::WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
  void SetLoadingTaskRunner(
      base::SingleThreadTaskRunner* loading_task_runner) override;

 private:
  class Context;
  class RequestPeerImpl;
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_
