// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gaming_seat.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/exo/buffer.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"

namespace exo {
namespace {

class MockGamingSeatDelegate : public GamingSeatDelegate {
 public:
  MOCK_CONST_METHOD1(CanAcceptGamepadEventsForSurface, bool(Surface*));
  MOCK_METHOD0(GamepadAdded, GamepadDelegate*());
  MOCK_METHOD0(Die, void());
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; };
  ~MockGamingSeatDelegate() { Die(); };
};

class MockGamepadDelegate : public GamepadDelegate {
 public:
  MockGamepadDelegate() {}

  // Overridden from GamepadDelegate:
  MOCK_METHOD0(OnRemoved, void());
  MOCK_METHOD2(OnAxis, void(int, double));
  MOCK_METHOD3(OnButton, void(int, bool, double));
  MOCK_METHOD0(OnFrame, void());
};

class GamingSeatTest : public test::ExoTestBase {
 public:
  GamingSeatTest() {}

  std::unique_ptr<device::GamepadDataFetcher> MockDataFetcherFactory() {
    blink::WebGamepads initial_data;
    std::unique_ptr<device::MockGamepadDataFetcher> fetcher(
        new device::MockGamepadDataFetcher(initial_data));
    mock_data_fetcher_ = fetcher.get();
    return std::move(fetcher);
  }

  void InitializeGamingSeat(MockGamingSeatDelegate* delegate) {
    polling_task_runner_ = new base::TestSimpleTaskRunner();
    gaming_seat_.reset(
        new GamingSeat(delegate, polling_task_runner_.get(),
                       base::Bind(&GamingSeatTest::MockDataFetcherFactory,
                                  base::Unretained(this))));
    // Run the polling task runner to have it create the data fetcher.
    polling_task_runner_->RunPendingTasks();
  }

  void DestroyGamingSeat(MockGamingSeatDelegate* delegate) {
    EXPECT_CALL(*delegate, Die()).Times(1);
    mock_data_fetcher_ = nullptr;
    gaming_seat_.reset();
    // Process tasks until polling is shut down.
    polling_task_runner_->RunPendingTasks();
    polling_task_runner_ = nullptr;
  }

  void SetDataAndPostToDelegate(const blink::WebGamepads& new_data) {
    ASSERT_TRUE(mock_data_fetcher_ != nullptr);
    mock_data_fetcher_->SetTestData(new_data);
    // Run one polling cycle, which will post a task to the origin task runner.
    polling_task_runner_->RunPendingTasks();
    // Run origin task runner to invoke delegate.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<GamingSeat> gaming_seat_;

  // Task runner to simulate the polling thread.
  scoped_refptr<base::TestSimpleTaskRunner> polling_task_runner_;

  // Weak reference to the mock data fetcher provided by MockDataFetcherFactory.
  // This instance is valid until both gamepad_ and polling_task_runner_ are
  // shut down.
  device::MockGamepadDataFetcher* mock_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamingSeatTest);
};

TEST_F(GamingSeatTest, ConnectionChange) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate0;
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate1;
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate2;

  {  // Test sequence
    testing::InSequence s;
    // connect gamepad 0
    // connect gamepad 2
    // connect gamepad 1
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded())
        .WillOnce(testing::Return(&gamepad_delegate0))
        .WillOnce(testing::Return(&gamepad_delegate2))
        .WillOnce(testing::Return(&gamepad_delegate1));
    // disconnect gamepad 1
    EXPECT_CALL(gamepad_delegate1, OnRemoved()).Times(1);
    // disconnect other gamepads
    EXPECT_CALL(gamepad_delegate0, OnRemoved()).Times(1);
    EXPECT_CALL(gamepad_delegate2, OnRemoved()).Times(1);
  }
  // Gamepad connected.
  blink::WebGamepads gamepad_connected;
  gamepad_connected.items[0].connected = true;
  gamepad_connected.items[0].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  gamepad_connected.items[2].connected = true;
  gamepad_connected.items[2].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  gamepad_connected.items[1].connected = true;
  gamepad_connected.items[1].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  // Gamepad 1 is dis connected
  gamepad_connected.items[1].connected = false;
  gamepad_connected.items[1].timestamp = 2;

  SetDataAndPostToDelegate(gamepad_connected);

  // Gamepad disconnected.
  blink::WebGamepads all_disconnected;
  SetDataAndPostToDelegate(all_disconnected);

  DestroyGamingSeat(gaming_seat_delegate);
}

TEST_F(GamingSeatTest, OnAxis) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate0;
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate2;

  // connect gamepad 0 and 2
  EXPECT_CALL(*gaming_seat_delegate, GamepadAdded())
      .WillOnce(testing::Return(&gamepad_delegate0))
      .WillOnce(testing::Return(&gamepad_delegate2));

  blink::WebGamepads gamepad_connected;
  gamepad_connected.items[0].connected = true;
  gamepad_connected.items[0].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  gamepad_connected.items[2].connected = true;
  gamepad_connected.items[2].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  // send axis event to 2 and then 0
  blink::WebGamepads axis_moved;
  axis_moved.items[0].connected = true;
  axis_moved.items[0].timestamp = 1;
  axis_moved.items[2].connected = true;
  axis_moved.items[2].timestamp = 2;
  axis_moved.items[2].axesLength = 1;
  axis_moved.items[2].axes[0] = 1.0;

  EXPECT_CALL(gamepad_delegate2, OnAxis(0, 1.0)).Times(1);
  EXPECT_CALL(gamepad_delegate2, OnFrame()).Times(1);
  SetDataAndPostToDelegate(axis_moved);

  axis_moved.items[0].timestamp = 2;
  axis_moved.items[0].axesLength = 1;
  axis_moved.items[0].axes[0] = 2.0;

  EXPECT_CALL(gamepad_delegate0, OnAxis(0, 2.0)).Times(1);
  EXPECT_CALL(gamepad_delegate0, OnFrame()).Times(1);
  SetDataAndPostToDelegate(axis_moved);

  EXPECT_CALL(gamepad_delegate0, OnRemoved()).Times(1);
  EXPECT_CALL(gamepad_delegate2, OnRemoved()).Times(1);
  DestroyGamingSeat(gaming_seat_delegate);
}

TEST_F(GamingSeatTest, OnButton) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate0;
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate2;

  // connect gamepad 0 and 2
  EXPECT_CALL(*gaming_seat_delegate, GamepadAdded())
      .WillOnce(testing::Return(&gamepad_delegate0))
      .WillOnce(testing::Return(&gamepad_delegate2));

  blink::WebGamepads gamepad_connected;
  gamepad_connected.items[0].connected = true;
  gamepad_connected.items[0].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  gamepad_connected.items[2].connected = true;
  gamepad_connected.items[2].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  // send axis event to 2 and then 0
  blink::WebGamepads axis_moved;
  axis_moved.items[0].connected = true;
  axis_moved.items[0].timestamp = 1;
  axis_moved.items[2].connected = true;
  axis_moved.items[2].timestamp = 2;

  axis_moved.items[2].buttonsLength = 1;
  axis_moved.items[2].buttons[0].pressed = true;
  axis_moved.items[2].buttons[0].value = 1.0;

  EXPECT_CALL(gamepad_delegate2, OnButton(0, true, 1.0)).Times(1);
  EXPECT_CALL(gamepad_delegate2, OnFrame()).Times(1);
  SetDataAndPostToDelegate(axis_moved);

  axis_moved.items[0].timestamp = 2;
  axis_moved.items[0].buttonsLength = 1;
  axis_moved.items[0].buttons[0].pressed = true;
  axis_moved.items[0].buttons[0].value = 2.0;

  EXPECT_CALL(gamepad_delegate0, OnButton(0, true, 2.0)).Times(1);
  EXPECT_CALL(gamepad_delegate0, OnFrame()).Times(1);
  SetDataAndPostToDelegate(axis_moved);

  EXPECT_CALL(gamepad_delegate0, OnRemoved()).Times(1);
  EXPECT_CALL(gamepad_delegate2, OnRemoved()).Times(1);

  DestroyGamingSeat(gaming_seat_delegate);
}

TEST_F(GamingSeatTest, OnWindowFocused) {
  // Create surface and move focus to it.
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);

  // In focus. Should be polling indefinitely, check a couple of time that the
  // poll task is re-posted.
  for (size_t i = 0; i < 5; ++i) {
    polling_task_runner_->RunPendingTasks();
    ASSERT_TRUE(polling_task_runner_->HasPendingTask());
  }

  // Remove focus from window.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  // Run EnablePolling and OnPoll task, no more polls should be scheduled.
  // In the first round of RunPendingTasks we will execute
  // EnablePollingOnPollingThread, which will cause the polling to stop being
  // scheduled in the next round.
  polling_task_runner_->RunPendingTasks();
  polling_task_runner_->RunPendingTasks();
  ASSERT_FALSE(polling_task_runner_->HasPendingTask());

  DestroyGamingSeat(gaming_seat_delegate);
}

}  // namespace
}  // namespace exo
