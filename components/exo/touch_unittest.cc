// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_positioner.h"
#include "ash/common/wm/window_positioning_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/touch.h"
#include "components/exo/touch_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

using TouchTest = test::ExoTestBase;

class MockTouchDelegate : public TouchDelegate {
 public:
  MockTouchDelegate() {}

  // Overridden from TouchDelegate:
  MOCK_METHOD1(OnTouchDestroying, void(Touch*));
  MOCK_CONST_METHOD1(CanAcceptTouchEventsForSurface, bool(Surface*));
  MOCK_METHOD4(OnTouchDown,
               void(Surface*, base::TimeTicks, int, const gfx::Point&));
  MOCK_METHOD2(OnTouchUp, void(base::TimeTicks, int));
  MOCK_METHOD3(OnTouchMotion, void(base::TimeTicks, int, const gfx::Point&));
  MOCK_METHOD0(OnTouchCancel, void());
};

TEST_F(TouchTest, OnTouchDown) {
  ash::WindowPositioner::DisableAutoPositioning(true);

  std::unique_ptr<Surface> bottom_surface(new Surface);
  std::unique_ptr<ShellSurface> bottom_shell_surface(
      new ShellSurface(bottom_surface.get()));
  gfx::Size bottom_buffer_size(10, 10);
  std::unique_ptr<Buffer> bottom_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(bottom_buffer_size)));
  bottom_surface->Attach(bottom_buffer.get());
  bottom_surface->Commit();
  ash::wm::CenterWindow(ash::WmWindowAura::Get(
      bottom_shell_surface->GetWidget()->GetNativeWindow()));

  std::unique_ptr<Surface> top_surface(new Surface);
  std::unique_ptr<ShellSurface> top_shell_surface(
      new ShellSurface(top_surface.get()));
  gfx::Size top_buffer_size(8, 8);
  std::unique_ptr<Buffer> top_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(top_buffer_size)));
  top_surface->Attach(top_buffer.get());
  top_surface->Commit();
  ash::wm::CenterWindow(ash::WmWindowAura::Get(
      top_shell_surface->GetWidget()->GetNativeWindow()));

  MockTouchDelegate delegate;
  std::unique_ptr<Touch> touch(new Touch(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(top_surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(top_surface.get(), testing::_, 1, gfx::Point()));
  generator.set_current_location(
      top_surface->window()->GetBoundsInScreen().origin());
  generator.PressTouchId(1);

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(bottom_surface.get()))
      .WillRepeatedly(testing::Return(true));
  // Second touch point should be relative to the focus surface.
  EXPECT_CALL(delegate, OnTouchDown(top_surface.get(), testing::_, 2,
                                    gfx::Point(-1, -1)));
  generator.set_current_location(
      bottom_surface->window()->GetBoundsInScreen().origin());
  generator.PressTouchId(2);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchUp) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockTouchDelegate delegate;
  std::unique_ptr<Touch> touch(new Touch(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, testing::_, gfx::Point()))
      .Times(2);
  generator.set_current_location(
      surface->window()->GetBoundsInScreen().origin());
  generator.PressTouchId(1);
  generator.PressTouchId(2);

  EXPECT_CALL(delegate, OnTouchUp(testing::_, 1));
  generator.ReleaseTouchId(1);
  EXPECT_CALL(delegate, OnTouchUp(testing::_, 2));
  generator.ReleaseTouchId(2);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchMotion) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockTouchDelegate delegate;
  std::unique_ptr<Touch> touch(new Touch(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, testing::_, gfx::Point()));
  EXPECT_CALL(delegate,
              OnTouchMotion(testing::_, testing::_, gfx::Point(5, 5)));
  EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  generator.set_current_location(
      surface->window()->GetBoundsInScreen().origin());
  generator.PressMoveAndReleaseTouchBy(5, 5);

  // Check if touch point motion outside focus surface is reported properly to
  // the focus surface.
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, testing::_, gfx::Point()));
  EXPECT_CALL(delegate,
              OnTouchMotion(testing::_, testing::_, gfx::Point(100, 100)));
  EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  generator.set_current_location(
      surface->window()->GetBoundsInScreen().origin());
  generator.PressMoveAndReleaseTouchBy(100, 100);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, OnTouchCancel) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockTouchDelegate delegate;
  std::unique_ptr<Touch> touch(new Touch(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate,
              OnTouchDown(surface.get(), testing::_, testing::_, gfx::Point()))
      .Times(2);
  generator.set_current_location(
      surface->window()->GetBoundsInScreen().origin());
  generator.PressTouchId(1);
  generator.PressTouchId(2);

  // One touch point being canceled is enough for OnTouchCancel to be called.
  EXPECT_CALL(delegate, OnTouchCancel());
  ui::TouchEvent cancel_event(ui::ET_TOUCH_CANCELLED, gfx::Point(), 1,
                              ui::EventTimeForNow());
  generator.Dispatch(&cancel_event);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

TEST_F(TouchTest, IgnoreTouchEventDuringModal) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(10, 10))));
  surface->Attach(buffer.get());
  surface->Commit();
  gfx::Point location = surface->window()->GetBoundsInScreen().origin();

  MockTouchDelegate delegate;
  std::unique_ptr<Touch> touch(new Touch(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Create surface for modal window.
  std::unique_ptr<Surface> surface2(new Surface);
  std::unique_ptr<ShellSurface> shell_surface2(
      new ShellSurface(surface2.get(), nullptr, gfx::Rect(0, 0, 5, 5), true,
                       ash::kShellWindowId_SystemModalContainer));
  std::unique_ptr<Buffer> buffer2(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(5, 5))));
  surface2->Attach(buffer2.get());
  surface2->Commit();
  ash::wm::CenterWindow(ash::WmWindowAura::Get(surface2->window()));
  gfx::Point location2 = surface2->window()->GetBoundsInScreen().origin();

  // Make the window modal.
  shell_surface2->SetSystemModal(true);
  EXPECT_TRUE(ash::WmShell::Get()->IsSystemModalWindowOpen());

  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, CanAcceptTouchEventsForSurface(surface2.get()))
      .WillRepeatedly(testing::Return(true));

  // Check if touch events on modal window are registered.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(surface2.get(), testing::_, testing::_,
                                      gfx::Point()));
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::Point(1, 1)));
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  }
  generator.set_current_location(location2);
  generator.PressMoveAndReleaseTouchBy(1, 1);

  // Check if touch events on non-modal window are ignored.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(surface.get(), testing::_, testing::_,
                                      gfx::Point()))
        .Times(0);
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::Point(1, 1)))
        .Times(0);
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_)).Times(0);
  }
  generator.set_current_location(location);
  generator.PressMoveAndReleaseTouchBy(1, 1);

  // Make the window non-modal.
  shell_surface2->SetSystemModal(false);
  EXPECT_FALSE(ash::WmShell::Get()->IsSystemModalWindowOpen());

  // Check if touch events on non-modal window are registered.
  {
    testing::InSequence sequence;
    EXPECT_CALL(delegate, OnTouchDown(surface.get(), testing::_, testing::_,
                                      gfx::Point()));
    EXPECT_CALL(delegate,
                OnTouchMotion(testing::_, testing::_, gfx::Point(1, 1)));
    EXPECT_CALL(delegate, OnTouchUp(testing::_, testing::_));
  }
  generator.set_current_location(location);
  generator.PressMoveAndReleaseTouchBy(1, 1);

  EXPECT_CALL(delegate, OnTouchDestroying(touch.get()));
  touch.reset();
}

}  // namespace
}  // namespace exo
