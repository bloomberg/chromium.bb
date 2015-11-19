// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using ShellSurfaceTest = test::ExoTestBase;

TEST_F(ShellSurfaceTest, Show) {
  scoped_ptr<Surface> surface(new Surface);
  scoped_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->Show();
  surface->Commit();
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
