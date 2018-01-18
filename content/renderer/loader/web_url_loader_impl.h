// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_
#define CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/interfaces/url_loader.mojom.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderFactory.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
struct ResourceResponseInfo;
}

namespace content {

class ChildURLLoaderFactoryGetter;
class ResourceDispatcher;

// PlzNavigate: Used to override parameters of the navigation request.
struct CONTENT_EXPORT StreamOverrideParameters {
 public:
  StreamOverrideParameters();
  ~StreamOverrideParameters();

  GURL stream_url;
  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;
  network::ResourceResponseHead response;
  std::vector<GURL> redirects;
  std::vector<network::ResourceResponseInfo> redirect_responses;
  std::vector<net::RedirectInfo> redirect_infos;

  // Called when this struct is deleted. Used to notify the browser that it can
  // release its associated StreamHandle.
  base::OnceCallback<void(const GURL&)> on_delete;
};

// Default implementation of WebURLLoaderFactory.
class CONTENT_EXPORT WebURLLoaderFactoryImpl
    : public blink::WebURLLoaderFactory {
 public:
  WebURLLoaderFactoryImpl(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      scoped_refptr<ChildURLLoaderFactoryGetter> loader_factory_getter);
  ~WebURLLoaderFactoryImpl() override;

  // Creates a test-only factory which can be used only for data URLs.
  static std::unique_ptr<WebURLLoaderFactoryImpl> CreateTestOnlyFactory();

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  base::WeakPtr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<ChildURLLoaderFactoryGetter> loader_factory_getter_;
  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderFactoryImpl);
};

class CONTENT_EXPORT WebURLLoaderImpl : public blink::WebURLLoader {
 public:
  WebURLLoaderImpl(ResourceDispatcher* resource_dispatcher,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   scoped_refptr<SharedURLLoaderFactory> url_loader_factory);
  // When non-null |keep_alive_handle| is specified, this loader prolongs
  // this render process's lifetime.
  WebURLLoaderImpl(ResourceDispatcher* resource_dispatcher,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   scoped_refptr<SharedURLLoaderFactory> url_loader_factory,
                   mojom::KeepAliveHandlePtr keep_alive_handle);
  ~WebURLLoaderImpl() override;

  static void PopulateURLResponse(const blink::WebURL& url,
                                  const network::ResourceResponseInfo& info,
                                  blink::WebURLResponse* response,
                                  bool report_security_info);
  // WebURLLoader methods:
  void LoadSynchronously(const blink::WebURLRequest& request,
                         blink::WebURLResponse& response,
                         base::Optional<blink::WebURLError>& error,
                         blink::WebData& data,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length) override;
  void LoadAsynchronously(const blink::WebURLRequest& request,
                          blink::WebURLLoaderClient* client) override;
  void Cancel() override;
  void SetDefersLoading(bool value) override;
  void DidChangePriority(blink::WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
 private:
  class Context;
  class RequestPeerImpl;
  class SinkPeer;
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_
