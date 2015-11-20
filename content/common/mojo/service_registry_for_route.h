// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SERVICE_REGISTRY_FOR_ROUTE_H_
#define CONTENT_COMMON_MOJO_SERVICE_REGISTRY_FOR_ROUTE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/service_registry.h"

namespace content {

class ServiceRegistryImpl;

// A ServiceRegistry which associates all service exposure and connection with a
// single route on an underlying ServiceRegistryImpl.
class CONTENT_EXPORT ServiceRegistryForRoute : public ServiceRegistry {
 public:
  ~ServiceRegistryForRoute() override;

  // ServiceRegistry:
  void Add(
      const std::string& service_name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)>
          service_factory) override;
  void Remove(const std::string& service_name) override;
  void Connect(const base::StringPiece& name,
               mojo::ScopedMessagePipeHandle handle) override;

 private:
  friend class ServiceRegistryImpl;

  ServiceRegistryForRoute(
      base::WeakPtr<ServiceRegistryImpl> registry,
      int routing_id,
      const base::Closure& destruction_callback);

  const base::WeakPtr<ServiceRegistryImpl> registry_;
  const int routing_id_;
  const base::Closure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryForRoute);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SERVICE_REGISTRY_FOR_ROUTE_H_
