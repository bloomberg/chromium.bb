// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

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

#if defined(USE_OZONE)
TEST_F(DisplayTest, CreatePrimeBuffer) {
  const gfx::Size buffer_size(256, 256);

  scoped_ptr<Display> display(new Display);

  // Creating a prime buffer from a native pixmap handle should succeed.
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gfx::kNullAcceleratedWidget, buffer_size,
                               gfx::BufferFormat::RGBA_8888,
                               gfx::BufferUsage::GPU_READ);
  gfx::NativePixmapHandle native_pixmap_handle = pixmap->ExportHandle();
  scoped_ptr<Buffer> buffer1 = display->CreatePrimeBuffer(
      base::ScopedFD(native_pixmap_handle.fd.fd), buffer_size,
      gfx::BufferFormat::RGBA_8888, native_pixmap_handle.stride);
  EXPECT_TRUE(buffer1);

  // Creating a prime buffer using an invalid fd should fail.
  scoped_ptr<Buffer> buffer2 = display->CreatePrimeBuffer(
      base::ScopedFD(), buffer_size, gfx::BufferFormat::RGBA_8888,
      buffer_size.width() * 4);
  EXPECT_FALSE(buffer2);
}
#endif

TEST_F(DisplayTest, CreateShellSurface) {
  scoped_ptr<Display> display(new Display);

  // Create two surfaces.
  scoped_ptr<Surface> surface1 = display->CreateSurface();
  ASSERT_TRUE(surface1);
  scoped_ptr<Surface> surface2 = display->CreateSurface();
  ASSERT_TRUE(surface2);

  // Create a shell surface for surface1.
  scoped_ptr<ShellSurface> shell_surface1 =
      display->CreateShellSurface(surface1.get());
  EXPECT_TRUE(shell_surface1);

  // Create a shell surface for surface2.
  scoped_ptr<ShellSurface> shell_surface2 =
      display->CreateShellSurface(surface2.get());
  EXPECT_TRUE(shell_surface2);
}

TEST_F(DisplayTest, CreateSubSurface) {
  scoped_ptr<Display> display(new Display);

  // Create child, parent and toplevel surfaces.
  scoped_ptr<Surface> child = display->CreateSurface();
  ASSERT_TRUE(child);
  scoped_ptr<Surface> parent = display->CreateSurface();
  ASSERT_TRUE(parent);
  scoped_ptr<Surface> toplevel = display->CreateSurface();
  ASSERT_TRUE(toplevel);

  // Attempting to create a sub surface for child with child as its parent
  // should fail.
  EXPECT_FALSE(display->CreateSubSurface(child.get(), child.get()));

  // Create a sub surface for child.
  scoped_ptr<SubSurface> child_sub_surface =
      display->CreateSubSurface(child.get(), toplevel.get());
  EXPECT_TRUE(child_sub_surface);

  // Attempting to create another sub surface when already assigned the role of
  // sub surface should fail.
  EXPECT_FALSE(display->CreateSubSurface(child.get(), parent.get()));

  // Deleting the sub surface should allow a new sub surface to be created.
  child_sub_surface.reset();
  child_sub_surface = display->CreateSubSurface(child.get(), parent.get());
  EXPECT_TRUE(child_sub_surface);

  scoped_ptr<Surface> sibling = display->CreateSurface();
  ASSERT_TRUE(sibling);

  // Create a sub surface for sibiling.
  scoped_ptr<SubSurface> sibling_sub_surface =
      display->CreateSubSurface(sibling.get(), parent.get());
  EXPECT_TRUE(sibling_sub_surface);

  // Create a shell surface for toplevel surface.
  scoped_ptr<ShellSurface> shell_surface =
      display->CreateShellSurface(toplevel.get());
  EXPECT_TRUE(shell_surface);

  // Attempting to create a sub surface when already assigned the role of
  // shell surface should fail.
  EXPECT_FALSE(display->CreateSubSurface(toplevel.get(), parent.get()));

  scoped_ptr<Surface> grandchild = display->CreateSurface();
  ASSERT_TRUE(grandchild);
  // Create a sub surface for grandchild.
  scoped_ptr<SubSurface> grandchild_sub_surface =
      display->CreateSubSurface(grandchild.get(), child.get());
  EXPECT_TRUE(grandchild_sub_surface);

  // Attempting to create a sub surface for parent with child as its parent
  // should fail.
  EXPECT_FALSE(display->CreateSubSurface(parent.get(), child.get()));

  // Attempting to create a sub surface for parent with grandchild as its parent
  // should fail.
  EXPECT_FALSE(display->CreateSubSurface(parent.get(), grandchild.get()));

  // Create a sub surface for parent.
  EXPECT_TRUE(display->CreateSubSurface(parent.get(), toplevel.get()));
}

}  // namespace
}  // namespace exo
