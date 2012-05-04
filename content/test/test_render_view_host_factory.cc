// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host_factory.h"

#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/render_process_host_factory.h"

namespace content {

TestRenderViewHostFactory::TestRenderViewHostFactory(
    content::RenderProcessHostFactory* rph_factory)
    : render_process_host_factory_(rph_factory) {
  RenderViewHostFactory::RegisterFactory(this);
}

TestRenderViewHostFactory::~TestRenderViewHostFactory() {
  RenderViewHostFactory::UnregisterFactory();
}

void TestRenderViewHostFactory::set_render_process_host_factory(
    content::RenderProcessHostFactory* rph_factory) {
  render_process_host_factory_ = rph_factory;
}

content::RenderViewHost* TestRenderViewHostFactory::CreateRenderViewHost(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    int routing_id,
    bool swapped_out,
    SessionStorageNamespace* session_storage) {
  // See declaration of render_process_host_factory_ below.
  static_cast<SiteInstanceImpl*>(instance)->
      set_render_process_host_factory(render_process_host_factory_);
  return new TestRenderViewHost(instance, delegate, routing_id, swapped_out);
}

}  // namespace content
