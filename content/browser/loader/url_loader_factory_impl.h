// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {

class ResourceRequesterInfo;

// This class is an implementation of network::mojom::URLLoaderFactory that
// creates a network::mojom::URLLoader. This class is instantiated only for
// Service Worker navigation preload or test cases.
class URLLoaderFactoryImpl final : public network::mojom::URLLoaderFactory {
 public:
  ~URLLoaderFactoryImpl() override;

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  static void CreateLoaderAndStart(
      ResourceRequesterInfo* requester_info,
      network::mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      network::mojom::URLLoaderClientPtr client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Creates a URLLoaderFactoryImpl instance. The instance is held by the
  // StrongBinding in it, so this function doesn't return the instance.
  CONTENT_EXPORT static void Create(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      network::mojom::URLLoaderFactoryRequest request,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner);

 private:
  explicit URLLoaderFactoryImpl(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner);

  void OnConnectionError();

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  scoped_refptr<ResourceRequesterInfo> requester_info_;

  // Task runner for the IO thead.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
