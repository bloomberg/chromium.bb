// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_SERVICE_REGISTRY_IMPL_H_
#define CONTENT_RENDERER_MOJO_BLINK_SERVICE_REGISTRY_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/public/platform/ServiceRegistry.h"

namespace content {

class ServiceRegistry;

// An implementation of blink::ServiceRegistry that forwards to a
// content::ServiceRegistry.
class BlinkServiceRegistryImpl : public blink::ServiceRegistry {
 public:
  explicit BlinkServiceRegistryImpl(
      base::WeakPtr<content::ServiceRegistry> service_registry);
  ~BlinkServiceRegistryImpl();

  // blink::ServiceRegistry override.
  void connectToRemoteService(const char* name,
                              mojo::ScopedMessagePipeHandle handle) override;

 private:
  const base::WeakPtr<content::ServiceRegistry> service_registry_;

  DISALLOW_COPY_AND_ASSIGN(BlinkServiceRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_SERVICE_REGISTRY_IMPL_H_
