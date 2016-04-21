// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

using SurfaceTest = test::ExoTestBase;

void ReleaseBuffer(int* release_buffer_call_count) {
  (*release_buffer_call_count)++;
}

TEST_F(SurfaceTest, Attach) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  // Set the release callback that will be run when buffer is no longer in use.
  int release_buffer_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&ReleaseBuffer, base::Unretained(&release_buffer_call_count)));

  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to surface1.
  surface->Attach(buffer.get());
  surface->Commit();

  // Commit without calling Attach() should have no effect.
  surface->Commit();
  EXPECT_EQ(0, release_buffer_call_count);

  // Attach a null buffer to surface, this should release the previously
  // attached buffer.
  surface->Attach(nullptr);
  surface->Commit();
  ASSERT_EQ(1, release_buffer_call_count);
}

TEST_F(SurfaceTest, Damage) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to the surface. This will update the pending bounds of
  // the surface to the buffer size.
  surface->Attach(buffer.get());

  // Mark areas inside the bounds of the surface as damaged. This should result
  // in pending damage.
  surface->Damage(gfx::Rect(0, 0, 10, 10));
  surface->Damage(gfx::Rect(10, 10, 10, 10));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(0, 0, 10, 10)));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(10, 10, 10, 10)));
  EXPECT_FALSE(surface->HasPendingDamageForTesting(gfx::Rect(5, 5, 10, 10)));
}

void SetFrameTime(base::TimeTicks* result, base::TimeTicks frame_time) {
  *result = frame_time;
}

TEST_F(SurfaceTest, RequestFrameCallback) {
  std::unique_ptr<Surface> surface(new Surface);

  base::TimeTicks frame_time;
  surface->RequestFrameCallback(
      base::Bind(&SetFrameTime, base::Unretained(&frame_time)));
  surface->Commit();

  // Callback should not run synchronously.
  EXPECT_TRUE(frame_time.is_null());
}

TEST_F(SurfaceTest, SetOpaqueRegion) {
  std::unique_ptr<Surface> surface(new Surface);

  // Setting a non-empty opaque region should succeed.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeWH(256, 256)));

  // Setting an empty opaque region should succeed.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeEmpty()));
}

TEST_F(SurfaceTest, SetInputRegion) {
  std::unique_ptr<Surface> surface(new Surface);

  // Setting a non-empty input region should succeed.
  surface->SetInputRegion(SkRegion(SkIRect::MakeWH(256, 256)));

  // Setting an empty input region should succeed.
  surface->SetInputRegion(SkRegion(SkIRect::MakeEmpty()));
}

TEST_F(SurfaceTest, SetBufferScale) {
  gfx::Size buffer_size(512, 512);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // This will update the bounds of the surface and take the buffer scale into
  // account.
  const float kBufferScale = 2.0f;
  surface->Attach(buffer.get());
  surface->SetBufferScale(kBufferScale);
  surface->Commit();
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->bounds().size().ToString());
}

TEST_F(SurfaceTest, SetViewport) {
  gfx::Size buffer_size(1, 1);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // This will update the bounds of the surface and take the viewport into
  // account.
  surface->Attach(buffer.get());
  gfx::Size viewport(256, 256);
  surface->SetViewport(viewport);
  surface->Commit();
  EXPECT_EQ(viewport.ToString(), surface->bounds().size().ToString());
}

TEST_F(SurfaceTest, SetOnlyVisibleOnSecureOutput) {
  gfx::Size buffer_size(1, 1);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  surface->Attach(buffer.get());
  surface->SetOnlyVisibleOnSecureOutput(true);
  surface->Commit();

  cc::TextureMailbox mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> release_callback;
  bool rv = surface->layer()->PrepareTextureMailbox(&mailbox, &release_callback,
                                                    false);
  ASSERT_TRUE(rv);

  EXPECT_TRUE(mailbox.secure_output_only());
  release_callback->Run(gpu::SyncToken(), false);
}

TEST_F(SurfaceTest, Commit) {
  std::unique_ptr<Surface> surface(new Surface);

  // Calling commit without a buffer should succeed.
  surface->Commit();
}

}  // namespace
}  // namespace exo
