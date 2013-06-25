// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "cc/resources/sync_point_helper.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/test/content_browser_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace content {
namespace {

enum ContextType {
  GPU_SERVICE_CONTEXT,
  IN_PROCESS_CONTEXT,
};

class SignalBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<ContextType> {
 public:
  virtual void SetUp() {
    switch (GetParam()) {
      case GPU_SERVICE_CONTEXT:
        context_.reset(
            WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
                BrowserGpuChannelHostFactory::instance(),
                WebKit::WebGraphicsContext3D::Attributes(),
                GURL()));
        break;
      case IN_PROCESS_CONTEXT:
        context_ =
            webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl::
            CreateOffscreenContext(WebKit::WebGraphicsContext3D::Attributes());
        break;
    }
  }

  // These tests should time out if the callback doesn't get called.
  void testSignalSyncPoint(unsigned sync_point) {
    base::RunLoop run_loop;
    cc::SyncPointHelper::SignalSyncPoint(
        context_.get(), sync_point, run_loop.QuitClosure());
    run_loop.Run();
  }

  // These tests should time out if the callback doesn't get called.
  void testSignalQuery(WebKit::WebGLId query) {
    base::RunLoop run_loop;
    cc::SyncPointHelper::SignalQuery(
        context_.get(), query, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  scoped_ptr<WebKit::WebGraphicsContext3D> context_;
};

IN_PROC_BROWSER_TEST_P(SignalBrowserTest, BasicSignalSyncPointTest) {
  testSignalSyncPoint(context_->insertSyncPoint());
};

IN_PROC_BROWSER_TEST_P(SignalBrowserTest, InvalidSignalSyncPointTest) {
  // Signalling something that doesn't exist should run the callback
  // immediately.
  testSignalSyncPoint(1297824234);
};

IN_PROC_BROWSER_TEST_P(SignalBrowserTest, BasicSignalQueryTest) {
  unsigned query = context_->createQueryEXT();
  context_->beginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query);
  context_->finish();
  context_->endQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  testSignalQuery(query);
  context_->deleteQueryEXT(query);
};

IN_PROC_BROWSER_TEST_P(SignalBrowserTest, SignalQueryUnboundTest) {
  WebKit::WebGLId query = context_->createQueryEXT();
  testSignalQuery(query);
  context_->deleteQueryEXT(query);
};

IN_PROC_BROWSER_TEST_P(SignalBrowserTest, InvalidSignalQueryUnboundTest) {
  // Signalling something that doesn't exist should run the callback
  // immediately.
  testSignalQuery(928729087);
};

INSTANTIATE_TEST_CASE_P(, SignalBrowserTest,
                        ::testing::Values(GPU_SERVICE_CONTEXT,
                                          IN_PROCESS_CONTEXT));

}  // namespace
}  // namespace content
