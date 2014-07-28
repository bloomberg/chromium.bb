// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"

namespace ash {
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
    UpdateDisplay(base::StringPrintf("%dx%d", kRootWidth, kRootHeight));

    aura::Window* root = GetRootWindow();
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
  aura::Window* GetRootWindow() const {
    return Shell::GetPrimaryRootWindow();
  }

  std::string GetHostMouseLocation() {
    const gfx::Point& location =
        aura::test::QueryLatestMousePositionRequestInHost(
            GetRootWindow()->GetHost());
    return location.ToString();
  }

  ash::MagnificationController* GetMagnificationController() const {
    return ash::Shell::GetInstance()->magnification_controller();
  }

  gfx::Rect GetViewport() const {
    gfx::RectF bounds(0, 0, kRootWidth, kRootHeight);
    GetRootWindow()->layer()->transform().TransformRectReverse(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  std::string CurrentPointOfInterest() const {
    return GetMagnificationController()->
        GetPointOfInterestForTesting().ToString();
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
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  // Changes the scale.
  GetMagnificationController()->SetScale(4.0f, false);
  EXPECT_EQ(4.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("300,225 200x150", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetMagnificationController()->SetScale(1.0f, false);
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetMagnificationController()->SetScale(3.0f, false);
  EXPECT_EQ(3.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("266,200 267x200", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());
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

TEST_F(MagnificationControllerTest, PointOfInterest) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ("0,0", CurrentPointOfInterest());

  generator.MoveMouseToInHost(gfx::Point(799, 599));
  EXPECT_EQ("799,599", CurrentPointOfInterest());

  generator.MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  generator.MoveMouseToInHost(gfx::Point(500, 400));
  EXPECT_EQ("450,350", CurrentPointOfInterest());
}

TEST_F(MagnificationControllerTest, PanWindow2xLeftToRight) {
  const aura::Env* env = aura::Env::GetInstance();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("0,0", env->last_mouse_location().ToString());

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->MoveWindow(0, 0, false);
  generator.MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ("0,0", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(300, 150));
  EXPECT_EQ("150,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(700, 150));
  EXPECT_EQ("350,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(701, 150));
  EXPECT_EQ("350,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(702, 150));
  EXPECT_EQ("351,75", env->last_mouse_location().ToString());
  EXPECT_EQ("1,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(703, 150));
  EXPECT_EQ("352,75", env->last_mouse_location().ToString());
  EXPECT_EQ("2,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(704, 150));
  EXPECT_EQ("354,75", env->last_mouse_location().ToString());
  EXPECT_EQ("4,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(712, 150));
  EXPECT_EQ("360,75", env->last_mouse_location().ToString());
  EXPECT_EQ("10,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(600, 150));
  EXPECT_EQ("310,75", env->last_mouse_location().ToString());
  EXPECT_EQ("10,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(720, 150));
  EXPECT_EQ("370,75", env->last_mouse_location().ToString());
  EXPECT_EQ("20,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("410,75", env->last_mouse_location().ToString());
  EXPECT_EQ("410,75", CurrentPointOfInterest());
  EXPECT_EQ("60,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(799, 150));
  EXPECT_EQ("459,75", env->last_mouse_location().ToString());
  EXPECT_EQ("109,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(702, 150));
  EXPECT_EQ("460,75", env->last_mouse_location().ToString());
  EXPECT_EQ("110,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("500,75", env->last_mouse_location().ToString());
  EXPECT_EQ("150,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("540,75", env->last_mouse_location().ToString());
  EXPECT_EQ("190,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("580,75", env->last_mouse_location().ToString());
  EXPECT_EQ("230,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("620,75", env->last_mouse_location().ToString());
  EXPECT_EQ("270,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("660,75", env->last_mouse_location().ToString());
  EXPECT_EQ("310,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("700,75", env->last_mouse_location().ToString());
  EXPECT_EQ("350,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("740,75", env->last_mouse_location().ToString());
  EXPECT_EQ("390,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("780,75", env->last_mouse_location().ToString());
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(799, 150));
  EXPECT_EQ("799,75", env->last_mouse_location().ToString());
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, PanWindow2xRightToLeft) {
  const aura::Env* env = aura::Env::GetInstance();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("799,300", env->last_mouse_location().ToString());

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);

  generator.MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("798,300", env->last_mouse_location().ToString());
  EXPECT_EQ("400,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  EXPECT_EQ("350,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("350,300", env->last_mouse_location().ToString());
  EXPECT_EQ("300,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("300,300", env->last_mouse_location().ToString());
  EXPECT_EQ("250,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("250,300", env->last_mouse_location().ToString());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("200,300", env->last_mouse_location().ToString());
  EXPECT_EQ("150,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("150,300", env->last_mouse_location().ToString());
  EXPECT_EQ("100,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("100,300", env->last_mouse_location().ToString());
  EXPECT_EQ("50,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("50,300", env->last_mouse_location().ToString());
  EXPECT_EQ("0,150 400x300", GetViewport().ToString());

  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("0,300", env->last_mouse_location().ToString());
  EXPECT_EQ("0,150 400x300", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, PanWindowToRight) {
  const aura::Env* env = aura::Env::GetInstance();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());

  float scale = 2.f;

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.f, GetMagnificationController()->GetScale());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.3784142, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  generator.MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("566,299", env->last_mouse_location().ToString());
  EXPECT_EQ("705,300", GetHostMouseLocation());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.8284268, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("599,299", env->last_mouse_location().ToString());
  EXPECT_EQ("702,300", GetHostMouseLocation());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(3.3635852, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("627,298", env->last_mouse_location().ToString());
  EXPECT_EQ("707,300", GetHostMouseLocation());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(4.f, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("649,298", env->last_mouse_location().ToString());
  EXPECT_EQ("704,300", GetHostMouseLocation());
}

TEST_F(MagnificationControllerTest, PanWindowToLeft) {
  const aura::Env* env = aura::Env::GetInstance();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());

  float scale = 2.f;

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.f, GetMagnificationController()->GetScale());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.3784142, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("231,299", env->last_mouse_location().ToString());
  EXPECT_EQ("100,300", GetHostMouseLocation());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.8284268, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("194,299", env->last_mouse_location().ToString());
  EXPECT_EQ("99,300", GetHostMouseLocation());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(3.3635852, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("164,298", env->last_mouse_location().ToString());
  EXPECT_EQ("98,300", GetHostMouseLocation());

  scale *= kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(4.f, GetMagnificationController()->GetScale());
  generator.MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("139,298", env->last_mouse_location().ToString());
  EXPECT_EQ("100,300", GetHostMouseLocation());
}

}  // namespace ash
