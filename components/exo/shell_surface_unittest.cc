// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

using ShellSurfaceTest = test::ExoTestBase;

TEST_F(ShellSurfaceTest, Maximize) {
  gfx::Size buffer_size(256, 256);
  scoped_ptr<Buffer> buffer(new Buffer(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D));
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  shell_surface->Maximize();
  surface->Commit();
  EXPECT_EQ(CurrentContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
}

TEST_F(ShellSurfaceTest, Restore) {
  gfx::Size buffer_size(256, 256);
  scoped_ptr<Buffer> buffer(new Buffer(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D));
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  shell_surface->Maximize();
  shell_surface->Restore();
  surface->Commit();
  EXPECT_EQ(
      buffer_size.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
}

TEST_F(ShellSurfaceTest, SetFullscreen) {
  gfx::Size buffer_size(256, 256);
  scoped_ptr<Buffer> buffer(new Buffer(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D));
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetFullscreen(true);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(CurrentContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
}

TEST_F(ShellSurfaceTest, SetTitle) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetTitle(base::string16(base::ASCIIToUTF16("test")));
  surface->Commit();
}

TEST_F(ShellSurfaceTest, SetApplicationId) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Commit();
  EXPECT_EQ("", ShellSurface::GetApplicationId(
                    shell_surface->GetWidget()->GetNativeWindow()));
  shell_surface->SetApplicationId("test");
  EXPECT_EQ("test", ShellSurface::GetApplicationId(
                        shell_surface->GetWidget()->GetNativeWindow()));
}

void DestroyShellSurface(scoped_ptr<ShellSurface>* shell_surface) {
  shell_surface->reset();
}

TEST_F(ShellSurfaceTest, Move) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  // Map shell surface.
  surface->Commit();

  // Post a task that will destroy the shell surface and then start an
  // interactive move. The interactive move should end when surface is
  // destroyed.
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&DestroyShellSurface, base::Unretained(&shell_surface)));
  shell_surface->Move();
}

TEST_F(ShellSurfaceTest, SetGeometry) {
  gfx::Size buffer_size(64, 64);
  scoped_ptr<Buffer> buffer(new Buffer(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D));
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  gfx::Rect geometry(16, 16, 32, 32);
  shell_surface->SetGeometry(geometry);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(
      geometry.size().ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
  EXPECT_EQ(gfx::Rect(gfx::Point() - geometry.OffsetFromOrigin(), buffer_size)
                .ToString(),
            surface->bounds().ToString());
}

void Close(int* close_call_count) {
  (*close_call_count)++;
}

TEST_F(ShellSurfaceTest, CloseCallback) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  int close_call_count = 0;
  shell_surface->set_close_callback(
      base::Bind(&Close, base::Unretained(&close_call_count)));

  surface->Commit();

  EXPECT_EQ(0, close_call_count);
  shell_surface->GetWidget()->Close();
  EXPECT_EQ(1, close_call_count);
}

TEST_F(ShellSurfaceTest, SurfaceDestroyedCallback) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_surface_destroyed_callback(
      base::Bind(&DestroyShellSurface, base::Unretained(&shell_surface)));

  surface->Commit();

  EXPECT_TRUE(shell_surface.get());
  surface.reset();
  EXPECT_FALSE(shell_surface.get());
}

void Configure(gfx::Size* suggested_size, const gfx::Size& size) {
  *suggested_size = size;
}

TEST_F(ShellSurfaceTest, ConfigureCallback) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  gfx::Size suggested_size;
  shell_surface->set_configure_callback(
      base::Bind(&Configure, base::Unretained(&suggested_size)));
  shell_surface->Maximize();
  EXPECT_EQ(CurrentContext()->bounds().width(), suggested_size.width());

  shell_surface->SetFullscreen(true);
  EXPECT_EQ(CurrentContext()->bounds().size().ToString(),
            suggested_size.ToString());
}

}  // namespace
}  // namespace exo
