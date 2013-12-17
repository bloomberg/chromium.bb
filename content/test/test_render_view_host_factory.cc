// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host_factory.h"

#include "content/browser/site_instance_impl.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/test/test_render_view_host.h"

namespace content {

TestRenderViewHostFactory::TestRenderViewHostFactory(
    RenderProcessHostFactory* rph_factory) {
  SiteInstanceImpl::set_render_process_host_factory(rph_factory);
  RenderViewHostFactory::RegisterFactory(this);
}

TestRenderViewHostFactory::~TestRenderViewHostFactory() {
  RenderViewHostFactory::UnregisterFactory();
  SiteInstanceImpl::set_render_process_host_factory(NULL);
}

void TestRenderViewHostFactory::set_render_process_host_factory(
    RenderProcessHostFactory* rph_factory) {
  SiteInstanceImpl::set_render_process_host_factory(rph_factory);
}

RenderViewHost* TestRenderViewHostFactory::CreateRenderViewHost(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int routing_id,
    int main_frame_routing_id,
    bool swapped_out) {
  return new TestRenderViewHost(
      instance, delegate, widget_delegate, routing_id, main_frame_routing_id,
      swapped_out);
}

}  // namespace content
