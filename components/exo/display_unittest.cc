// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using DisplayTest = test::ExoTestBase;

TEST_F(DisplayTest, CreateSurface) {
  scoped_ptr<Display> display(new Display);

  // Creating a surface should succeed.
  scoped_ptr<Surface> surface = display->CreateSurface();
  EXPECT_TRUE(surface);
}

TEST_F(DisplayTest, CreateSharedMemory) {
  scoped_ptr<Display> display(new Display);

  int shm_size = 8192;
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  bool rv = shared_memory->CreateAnonymous(shm_size);
  ASSERT_TRUE(rv);

  base::SharedMemoryHandle handle =
      base::SharedMemory::DuplicateHandle(shared_memory->handle());
  ASSERT_TRUE(base::SharedMemory::IsHandleValid(handle));

  // Creating a shared memory instance from a valid handle should succeed.
  scoped_ptr<SharedMemory> shm1 = display->CreateSharedMemory(handle, shm_size);
  EXPECT_TRUE(shm1);

  // Creating a shared memory instance from a invalid handle should fail.
  scoped_ptr<SharedMemory> shm2 =
      display->CreateSharedMemory(base::SharedMemoryHandle(), shm_size);
  EXPECT_FALSE(shm2);
}

TEST_F(DisplayTest, CreateShellSurface) {
  scoped_ptr<Display> display(new Display);

  // Create two surfaces.
  scoped_ptr<Surface> surface1 = display->CreateSurface();
  EXPECT_TRUE(surface1);
  scoped_ptr<Surface> surface2 = display->CreateSurface();
  EXPECT_TRUE(surface2);

  // Create a shell surface for surface1.
  scoped_ptr<ShellSurface> shell_surface1 =
      display->CreateShellSurface(surface1.get());

  // Create a shell surface for surface2.
  scoped_ptr<ShellSurface> shell_surface2 =
      display->CreateShellSurface(surface2.get());
}

}  // namespace
}  // namespace exo
