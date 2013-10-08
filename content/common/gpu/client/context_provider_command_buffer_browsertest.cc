// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/test/content_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class ContextProviderCommandBufferBrowserTest : public ContentBrowserTest {
 protected:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContext3d() {
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
        WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            BrowserGpuChannelHostFactory::instance(),
            WebKit::WebGraphicsContext3D::Attributes(),
            GURL("chrome://gpu/ContextProviderCommandBufferTest")));
    return context.Pass();
  }
};

IN_PROC_BROWSER_TEST_F(ContextProviderCommandBufferBrowserTest, LeakOnDestroy) {
  scoped_refptr<ContextProviderCommandBuffer> provider =
      ContextProviderCommandBuffer::Create(CreateContext3d(), "TestContext");
  provider->set_leak_on_destroy();
  EXPECT_TRUE(provider->BindToCurrentThread());
  // This should not crash.
  provider = NULL;
}

}  // namespace
}  // namespace content
