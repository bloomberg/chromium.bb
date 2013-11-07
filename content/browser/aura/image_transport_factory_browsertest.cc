// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "content/browser/aura/image_transport_factory.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/test/content_browser_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

class ImageTransportFactoryBrowserTest : public ContentBrowserTest {
 public:
  ImageTransportFactoryBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    UseRealGLContexts();
    ContentBrowserTest::SetUp();
  }
};

class MockImageTransportFactoryObserver : public ImageTransportFactoryObserver {
 public:
  MOCK_METHOD0(OnLostResources, void());
};

// Checks that upon context loss, the observer is called and the created
// resources are reset.
IN_PROC_BROWSER_TEST_F(ImageTransportFactoryBrowserTest, TestLostContext) {
  // This test doesn't make sense in software compositing mode.
  if (!GpuDataManager::GetInstance()->CanUseGpuBrowserCompositor())
    return;

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  scoped_refptr<ui::Texture> texture = factory->CreateTransportClient(1.f);
  ASSERT_TRUE(texture.get());

  MockImageTransportFactoryObserver observer;
  factory->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLostResources())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  blink::WebGraphicsContext3D* context = texture->HostContext3D();
  context->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                               GL_INNOCENT_CONTEXT_RESET_ARB);

  run_loop.Run();
  EXPECT_FALSE(texture->HostContext3D());
  EXPECT_EQ(0u, texture->PrepareTexture());

  factory->RemoveObserver(&observer);
}

}  // anonymous namespace
}  // namespace content
