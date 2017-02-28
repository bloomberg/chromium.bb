// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/memory/ptr_util.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#endif

namespace content {

class OffscreenCanvasSurfaceManagerTest : public testing::Test {
 public:
  int getNumSurfaceImplInstances() {
    return OffscreenCanvasSurfaceManager::GetInstance()
        ->registered_surface_instances_.size();
  }

  void OnSurfaceCreated(const cc::SurfaceId& surface_id) {
    OffscreenCanvasSurfaceManager::GetInstance()->OnSurfaceCreated(
        cc::SurfaceInfo(surface_id, 1.0f, gfx::Size(10, 10)));
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<TestBrowserThread> ui_thread_;
  base::MessageLoopForUI message_loop_;
};

void OffscreenCanvasSurfaceManagerTest::SetUp() {
#if !defined(OS_ANDROID)
  ImageTransportFactory::InitializeForUnitTests(
      std::unique_ptr<ImageTransportFactory>(
          new NoTransportImageTransportFactory));
#endif
  ui_thread_.reset(new TestBrowserThread(BrowserThread::UI, &message_loop_));
}

void OffscreenCanvasSurfaceManagerTest::TearDown() {
#if !defined(OS_ANDROID)
  ImageTransportFactory::Terminate();
#endif
}

// This test mimics the workflow of OffscreenCanvas.commit() on renderer
// process.
TEST_F(OffscreenCanvasSurfaceManagerTest,
       SingleHTMLCanvasElementTransferToOffscreen) {
  cc::mojom::DisplayCompositorClientPtr client;
  cc::FrameSinkId frame_sink_id(3, 3);
  cc::LocalSurfaceIdAllocator local_surface_id_allocator;
  cc::LocalSurfaceId current_local_surface_id(
      local_surface_id_allocator.GenerateId());

  auto surface_impl = base::WrapUnique(new OffscreenCanvasSurfaceImpl(
      cc::FrameSinkId(), frame_sink_id, std::move(client)));
  EXPECT_EQ(1, this->getNumSurfaceImplInstances());
  EXPECT_EQ(surface_impl.get(),
            OffscreenCanvasSurfaceManager::GetInstance()->GetSurfaceInstance(
                frame_sink_id));

  this->OnSurfaceCreated(
      cc::SurfaceId(frame_sink_id, current_local_surface_id));
  EXPECT_EQ(current_local_surface_id, surface_impl->current_local_surface_id());

  surface_impl = nullptr;
  EXPECT_EQ(0, this->getNumSurfaceImplInstances());
}

TEST_F(OffscreenCanvasSurfaceManagerTest,
       MultiHTMLCanvasElementTransferToOffscreen) {
  cc::mojom::DisplayCompositorClientPtr client_a;
  cc::FrameSinkId dummy_parent_frame_sink_id(0, 0);
  cc::FrameSinkId frame_sink_id_a(3, 3);
  auto surface_impl_a = base::WrapUnique(new OffscreenCanvasSurfaceImpl(
      dummy_parent_frame_sink_id, frame_sink_id_a, std::move(client_a)));

  cc::mojom::DisplayCompositorClientPtr client_b;
  cc::FrameSinkId frame_sink_id_b(4, 4);

  auto surface_impl_b = base::WrapUnique(new OffscreenCanvasSurfaceImpl(
      dummy_parent_frame_sink_id, frame_sink_id_b, std::move(client_b)));

  EXPECT_EQ(2, this->getNumSurfaceImplInstances());
  EXPECT_EQ(surface_impl_a.get(),
            OffscreenCanvasSurfaceManager::GetInstance()->GetSurfaceInstance(
                frame_sink_id_a));
  EXPECT_EQ(surface_impl_b.get(),
            OffscreenCanvasSurfaceManager::GetInstance()->GetSurfaceInstance(
                frame_sink_id_b));

  surface_impl_a = nullptr;
  EXPECT_EQ(1, this->getNumSurfaceImplInstances());
  surface_impl_b = nullptr;
  EXPECT_EQ(0, this->getNumSurfaceImplInstances());
}

}  // namespace content
