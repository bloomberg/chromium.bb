// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_view_host_factory.h"

#include "base/logging.h"
#include "chrome/browser/renderer_host/render_view_host.h"

// static
RenderViewHostFactory* RenderViewHostFactory::factory_ = NULL;

// static
RenderViewHost* RenderViewHostFactory::Create(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    int routing_id,
    int64 session_storage_namespace_id) {
  if (factory_) {
    return factory_->CreateRenderViewHost(instance, delegate, routing_id,
                                          session_storage_namespace_id);
  }
  return new RenderViewHost(instance, delegate, routing_id,
                            session_storage_namespace_id);
}

// static
void RenderViewHostFactory::RegisterFactory(RenderViewHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderViewHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = NULL;
}
