// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

class ResourceRequesterInfo;

// This class is an implementation of mojom::URLLoaderFactory that creates
// a mojom::URLLoader. This class is instantiated only for Service Worker
// navigation preload or test caseses.
class URLLoaderFactoryImpl final : public mojom::URLLoaderFactory {
 public:
  ~URLLoaderFactoryImpl() override;

  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojom::URLLoaderFactoryRequest request) override;

  static void CreateLoaderAndStart(
      ResourceRequesterInfo* requester_info,
      mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& url_request,
      mojom::URLLoaderClientPtr client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Creates a URLLoaderFactoryImpl instance. The instance is held by the
  // StrongBinding in it, so this function doesn't return the instance.
  CONTENT_EXPORT static void Create(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      mojom::URLLoaderFactoryRequest request,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner);

 private:
  explicit URLLoaderFactoryImpl(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner);

  void OnConnectionError();

  mojo::BindingSet<mojom::URLLoaderFactory> bindings_;

  scoped_refptr<ResourceRequesterInfo> requester_info_;

  // Task runner for the IO thead.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
