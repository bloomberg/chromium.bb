// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

using ShellSurfaceTest = test::ExoTestBase;

TEST_F(ShellSurfaceTest, Show) {
  gfx::Size small_buffer_size(64, 64);
  scoped_ptr<Buffer> small_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(small_buffer_size),
                 GL_TEXTURE_2D));
  gfx::Size large_buffer_size(256, 256);
  scoped_ptr<Buffer> large_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(large_buffer_size),
                 GL_TEXTURE_2D));
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->Show();
  surface->Attach(small_buffer.get());
  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());
  EXPECT_EQ(
      small_buffer_size.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());

  surface->Attach(large_buffer.get());
  surface->Commit();
  EXPECT_EQ(
      large_buffer_size.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
}

TEST_F(ShellSurfaceTest, SetToplevel) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetToplevel();
  surface->Commit();
}

TEST_F(ShellSurfaceTest, SetFullscreen) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetFullscreen(true);
  surface->Commit();

  // Fullscreen mode can change after the initial Commit().
  shell_surface->SetFullscreen(false);
  surface->Commit();
}

TEST_F(ShellSurfaceTest, SetTitle) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetTitle(base::string16(base::ASCIIToUTF16("test")));
  surface->Commit();
}

}  // namespace
}  // namespace exo
