// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {
namespace {

const int kRootHeight = 600;
const int kRootWidth = 800;

}  // namespace

class MagnificationControllerTest: public test::AshTestBase {
 public:
  MagnificationControllerTest() {}
  virtual ~MagnificationControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay(StringPrintf("%dx%d", kRootWidth, kRootHeight));

    aura::RootWindow* root = GetRootWindow();
    gfx::Rect root_bounds(root->bounds());

#if defined(OS_WIN)
    // RootWindow and Display can't resize on Windows Ash.
    // http://crbug.com/165962
    EXPECT_EQ(kRootHeight, root_bounds.height());
    EXPECT_EQ(kRootWidth, root_bounds.width());
#endif
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
  }

 protected:
  aura::RootWindow* GetRootWindow() {
    return Shell::GetPrimaryRootWindow();
  }

  ash::MagnificationController* GetMagnificationController() {
    return ash::Shell::GetInstance()->magnification_controller();
  }

  gfx::Rect GetViewport() {
    gfx::RectF bounds(0, 0, kRootWidth, kRootHeight);
    GetRootWindow()->layer()->transform().TransformRectReverse(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MagnificationControllerTest);
};

TEST_F(MagnificationControllerTest, EnableAndDisable) {
  // Confirms the magnifier is disabled.
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  // Enables magnifier.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Disables magnifier.
  GetMagnificationController()->SetEnabled(false);
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  // Confirms the the scale can't be changed.
  GetMagnificationController()->SetScale(4.0f, false);
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, MagnifyAndUnmagnify) {
  // Enables magnifier and confirms the default scale is 2.0x.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Changes the scale.
  GetMagnificationController()->SetScale(4.0f, false);
  EXPECT_EQ(4.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("300,225 200x150", GetViewport().ToString());

  GetMagnificationController()->SetScale(1.0f, false);
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  GetMagnificationController()->SetScale(3.0f, false);
  EXPECT_EQ(3.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("266,200 268x200", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, MoveWindow) {
  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Move the viewport.
  GetMagnificationController()->MoveWindow(0, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(200, 300, false);
  EXPECT_EQ("200,300 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(400, 0, false);
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(400, 300, false);
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());

  // Confirms that the viewport can't across the top-left border.
  GetMagnificationController()->MoveWindow(-100, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(0, -100, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(-100, -100, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  // Confirms that the viewport can't across the bittom-right border.
  GetMagnificationController()->MoveWindow(800, 0, false);
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(0, 400, false);
  EXPECT_EQ("0,300 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(200, 400, false);
  EXPECT_EQ("200,300 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(1000, 1000, false);
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());
}

}  // namespace internal
}  // namespace ash
