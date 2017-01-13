// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "cc/output/context_provider.h"
#include "cc/resources/single_release_callback.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

using BufferTest = test::ExoTestBase;

void Release(int* release_call_count) {
  (*release_call_count)++;
}

TEST_F(BufferTest, ReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  const cc::FrameSinkId arbitrary_frame_sink_id(1, 1);
  scoped_refptr<CompositorFrameSinkHolder> compositor_frame_sink_holder =
      new CompositorFrameSinkHolder(surface.get(), arbitrary_frame_sink_id,
                                    aura::Env::GetInstance()
                                        ->context_factory_private()
                                        ->GetSurfaceManager());

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&Release, base::Unretained(&release_call_count)));

  buffer->OnAttach();
  cc::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  bool rv = buffer->ProduceTransferableResource(
      compositor_frame_sink_holder.get(), 0, false, true, &resource);
  ASSERT_TRUE(rv);

  // Release buffer.
  cc::ReturnedResource returned_resource;
  returned_resource.id = resource.id;
  returned_resource.sync_token = resource.mailbox_holder.sync_token;
  returned_resource.lost = false;
  cc::ReturnedResourceArray resources = {returned_resource};
  compositor_frame_sink_holder->ReclaimResources(resources);

  RunAllPendingInMessageLoop();
  ASSERT_EQ(release_call_count, 0);

  buffer->OnDetach();

  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, IsLost) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  const cc::FrameSinkId arbitrary_frame_sink_id(1, 1);
  std::unique_ptr<Surface> surface(new Surface);
  scoped_refptr<CompositorFrameSinkHolder> compositor_frame_sink_holder =
      new CompositorFrameSinkHolder(surface.get(), arbitrary_frame_sink_id,
                                    aura::Env::GetInstance()
                                        ->context_factory_private()
                                        ->GetSurfaceManager());
  cc::ResourceId resource_id = 0;

  buffer->OnAttach();
  // Acquire a texture transferable resource for the contents of the buffer.
  cc::TransferableResource resource;
  bool rv = buffer->ProduceTransferableResource(
      compositor_frame_sink_holder.get(), resource_id, false, true, &resource);
  ASSERT_TRUE(rv);

  scoped_refptr<cc::ContextProvider> context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadContextProvider();
  if (context_provider) {
    gpu::gles2::GLES2Interface* gles2 = context_provider->ContextGL();
    gles2->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                               GL_INNOCENT_CONTEXT_RESET_ARB);
  }

  // Release buffer.
  bool is_lost = true;
  cc::ReturnedResource returned_resource;
  returned_resource.id = resource_id;
  returned_resource.sync_token = gpu::SyncToken();
  returned_resource.lost = is_lost;
  cc::ReturnedResourceArray resources = {returned_resource};
  compositor_frame_sink_holder->ReclaimResources(resources);
  RunAllPendingInMessageLoop();

  // Producing a new texture transferable resource for the contents of the
  // buffer.
  ++resource_id;
  cc::TransferableResource new_resource;
  rv = buffer->ProduceTransferableResource(compositor_frame_sink_holder.get(),
                                           resource_id, false, false,
                                           &new_resource);
  ASSERT_TRUE(rv);
  buffer->OnDetach();

  cc::ReturnedResource returned_resource2;
  returned_resource2.id = resource_id;
  returned_resource2.sync_token = gpu::SyncToken();
  returned_resource2.lost = false;
  cc::ReturnedResourceArray resources2 = {returned_resource2};
  compositor_frame_sink_holder->ReclaimResources(resources2);
  RunAllPendingInMessageLoop();
}

}  // namespace
}  // namespace exo
