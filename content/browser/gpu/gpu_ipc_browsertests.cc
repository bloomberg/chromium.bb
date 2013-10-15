// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/common/content_switches.h"
#include "content/test/content_browser_test.h"
#include "ui/gl/gl_switches.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace {

class ContextTestBase : public content::ContentBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    CHECK(content::BrowserGpuChannelHostFactory::instance());
    context_.reset(
        content::WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            content::BrowserGpuChannelHostFactory::instance(),
            WebKit::WebGraphicsContext3D::Attributes(),
            GURL()));
    CHECK(context_.get());
    context_->makeContextCurrent();
    ContentBrowserTest::SetUpOnMainThread();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    // Must delete the context first.
    context_.reset(NULL);
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  scoped_ptr<WebKit::WebGraphicsContext3D> context_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F IN_PROC_BROWSER_TEST_F
#include "content/common/gpu/client/gpu_context_tests.h"
