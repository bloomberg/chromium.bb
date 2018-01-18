// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SHARED_URL_LOADER_FACTORY_H_
#define CONTENT_PUBLIC_COMMON_SHARED_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/interfaces/url_loader.mojom.h"

namespace content {

class SharedURLLoaderFactory;

// SharedURLLoaderFactoryInfo contains necessary information to construct a
// SharedURLLoaderFactory. It is not sequence safe but can be passed across
// sequences. Please see the comments of SharedURLLoaderFactory for how this
// class is used.
class CONTENT_EXPORT SharedURLLoaderFactoryInfo {
 public:
  SharedURLLoaderFactoryInfo();
  virtual ~SharedURLLoaderFactoryInfo();

 protected:
  friend class SharedURLLoaderFactory;

  // Creates a SharedURLLoaderFactory. It should only be called by
  // SharedURLLoaderFactory::Create(), which makes sense that CreateFactory() is
  // never called multiple times for each SharedURLLoaderFactoryInfo instance.
  virtual scoped_refptr<SharedURLLoaderFactory> CreateFactory() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedURLLoaderFactoryInfo);
};

// A SharedURLLoaderFactory instance is supposed to be used on a single
// sequence. To use it on a different sequence, use Clone() and pass the
// resulting SharedURLLoaderFactoryInfo instance to the target sequence. On the
// target sequence, call SharedURLLoaderFactory::Create() to convert the info
// instance to a new SharedURLLoaderFactory.
class CONTENT_EXPORT SharedURLLoaderFactory
    : public base::RefCounted<SharedURLLoaderFactory> {
 public:
  struct Constraints {
    // Skip appcache and service worker if this flag is set to true.
    bool bypass_custom_network_loader = false;
  };
  static scoped_refptr<SharedURLLoaderFactory> Create(
      std::unique_ptr<SharedURLLoaderFactoryInfo> info);

  virtual void CreateLoaderAndStart(
      network::mojom::URLLoaderRequest loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const Constraints& constaints = kDefaultConstraints) = 0;

  virtual std::unique_ptr<SharedURLLoaderFactoryInfo> Clone() = 0;

 protected:
  friend class base::RefCounted<SharedURLLoaderFactory>;
  virtual ~SharedURLLoaderFactory();

  static const Constraints kDefaultConstraints;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SHARED_URL_LOADER_FACTORY_H_
