// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host_factory.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"

namespace content {

TestRenderViewHostFactory::TestRenderViewHostFactory(
    RenderProcessHostFactory* rph_factory) {
  RenderProcessHostImpl::set_render_process_host_factory_for_testing(
      rph_factory);
  RenderViewHostFactory::RegisterFactory(this);
}

TestRenderViewHostFactory::~TestRenderViewHostFactory() {
  RenderViewHostFactory::UnregisterFactory();
  RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
}

void TestRenderViewHostFactory::set_render_process_host_factory(
    RenderProcessHostFactory* rph_factory) {
  RenderProcessHostImpl::set_render_process_host_factory_for_testing(
      rph_factory);
}

RenderViewHost* TestRenderViewHostFactory::CreateRenderViewHost(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int32_t routing_id,
    int32_t main_frame_routing_id,
    int32_t widget_routing_id,
    bool swapped_out) {
  return new TestRenderViewHost(
      instance,
      TestRenderWidgetHost::Create(widget_delegate, instance->GetProcess(),
                                   widget_routing_id, false),
      delegate, routing_id, main_frame_routing_id, swapped_out);
}

}  // namespace content
