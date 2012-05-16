// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_VIEW_HOST_FACTORY_H_
#define CONTENT_TEST_TEST_RENDER_VIEW_HOST_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/browser/renderer_host/render_view_host_factory.h"

namespace content {

class SiteInstance;
class RenderViewHostDelegate;
class RenderProcessHostFactory;
class SessionStorageNamespace;

// Manages creation of the RenderViewHosts using our special subclass. This
// automatically registers itself when it goes in scope, and unregisters itself
// when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  explicit TestRenderViewHostFactory(
      content::RenderProcessHostFactory* rph_factory);
  virtual ~TestRenderViewHostFactory();

  virtual void set_render_process_host_factory(
      content::RenderProcessHostFactory* rph_factory);
  virtual content::RenderViewHost* CreateRenderViewHost(
      content::SiteInstance* instance,
      content::RenderViewHostDelegate* delegate,
      content::RenderWidgetHostDelegate* widget_delegate,
      int routing_id,
      bool swapped_out,
      content::SessionStorageNamespace* session_storage) OVERRIDE;

 private:
  // This is a bit of a hack. With the current design of the site instances /
  // browsing instances, it's difficult to pass a RenderProcessHostFactory
  // around properly.
  //
  // Instead, we set it right before we create a new RenderViewHost, which
  // happens before the RenderProcessHost is created. This way, the instance
  // has the correct factory and creates our special RenderProcessHosts.
  content::RenderProcessHostFactory* render_process_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHostFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_VIEW_HOST_FACTORY_H_
