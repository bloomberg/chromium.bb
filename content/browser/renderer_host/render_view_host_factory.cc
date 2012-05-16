// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_factory.h"

#include "base/logging.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

using content::RenderViewHost;
using content::RenderViewHostImpl;
using content::SessionStorageNamespace;
using content::SiteInstance;

// static
RenderViewHostFactory* RenderViewHostFactory::factory_ = NULL;

// static
RenderViewHost* RenderViewHostFactory::Create(
    SiteInstance* instance,
    content::RenderViewHostDelegate* delegate,
    content::RenderWidgetHostDelegate* widget_delegate,
    int routing_id,
    bool swapped_out,
    SessionStorageNamespace* session_storage_namespace) {
  if (factory_) {
    return factory_->CreateRenderViewHost(instance, delegate, widget_delegate,
                                          routing_id, swapped_out,
                                          session_storage_namespace);
  }
  return new RenderViewHostImpl(instance, delegate, widget_delegate, routing_id,
                                swapped_out, session_storage_namespace);
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
