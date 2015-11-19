// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/test/widget_test.h"

namespace exo {
namespace {

using SurfaceTest = test::ExoTestBase;

void ReleaseBuffer(int* release_buffer_call_count) {
  (*release_buffer_call_count)++;
}

TEST_F(SurfaceTest, Attach) {
  gfx::Size buffer_size(256, 256);
  scoped_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size).Pass(),
                 GL_TEXTURE_2D));

  // Set the release callback that will be run when buffer is no longer in use.
  int release_buffer_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&ReleaseBuffer, base::Unretained(&release_buffer_call_count)));

  scoped_ptr<Surface> surface1(new Surface);
  scoped_ptr<Surface> surface2(new Surface);

  // Attach the buffer to surface1.
  surface1->Attach(buffer.get());
  surface1->Commit();

  // Attaching buffer to surface2 when it is already attached to surface1
  // should fail and buffer should remain attached to surface1.
  surface2->Attach(buffer.get());
  surface2->Commit();

  // Attach a null buffer to surface1, this should release the previously
  // attached buffer.
  surface1->Attach(nullptr);
  surface1->Commit();
  ASSERT_EQ(release_buffer_call_count, 1);
}

TEST_F(SurfaceTest, Damage) {
  gfx::Size buffer_size(256, 256);
  scoped_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size).Pass(),
                 GL_TEXTURE_2D));
  scoped_ptr<Surface> surface(new Surface);

  // Attach the buffer to the surface. This will update the pending bounds of
  // the surface to the buffer size.
  surface->Attach(buffer.get());

  // Mark area inside the bounds of the surface as damaged. This should result
  // in pending damage.
  surface->Damage(gfx::Rect(0, 0, 10, 10));
  EXPECT_TRUE(surface->HasPendingDamageForTesting());
}

void SetFrameTime(base::TimeTicks* result, base::TimeTicks frame_time) {
  *result = frame_time;
}

TEST_F(SurfaceTest, RequestFrameCallback) {
  scoped_ptr<Surface> surface(new Surface);

  base::TimeTicks frame_time;
  surface->RequestFrameCallback(
      base::Bind(&SetFrameTime, base::Unretained(&frame_time)));
  surface->Commit();

  // Callback should not run synchronously.
  EXPECT_TRUE(frame_time.is_null());
}

TEST_F(SurfaceTest, SetOpaqueRegion) {
  scoped_ptr<Surface> surface(new Surface);

  // Setting a non-empty opaque region should succeed.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeWH(256, 256)));

  // Setting an empty opaque region should succeed.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeEmpty()));
}

TEST_F(SurfaceTest, Commit) {
  scoped_ptr<Surface> surface(new Surface);

  // Calling commit without a buffer should succeed.
  surface->Commit();
}

}  // namespace
}  // namespace exo
