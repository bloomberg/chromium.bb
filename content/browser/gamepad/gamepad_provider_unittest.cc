// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/system_monitor/system_monitor.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/common/gamepad_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDataFetcher : public gamepad::DataFetcher {
 public:
  MockDataFetcher() : read_data_(false, false) {
    memset(&test_data, 0, sizeof(test_data));
  }
  virtual void GetGamepadData(WebKit::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE {
    *pads = test_data;
    read_data_.Signal();
  }

  void SetData(WebKit::WebGamepads& data) {
    test_data = data;
  }

  void WaitForDataRead() { return read_data_.Wait(); }

  WebKit::WebGamepads test_data;
  base::WaitableEvent read_data_;
};

// Main test fixture
class GamepadProviderTest : public testing::Test {
 public:
  gamepad::Provider* CreateProvider() {
#if defined(OS_MACOSX)
    base::SystemMonitor::AllocateSystemIOPorts();
#endif
    system_monitor_.reset(new base::SystemMonitor);
    mock_data_fetcher_ = new MockDataFetcher;
    provider_ = new gamepad::Provider(mock_data_fetcher_);
    return provider_.get();
  }

 protected:
  GamepadProviderTest() {
  }

  MessageLoop main_message_loop_;
  scoped_ptr<base::SystemMonitor> system_monitor_;
  MockDataFetcher* mock_data_fetcher_;
  scoped_refptr<gamepad::Provider> provider_;
};

TEST_F(GamepadProviderTest, BasicStartStop) {
  gamepad::Provider* provider = CreateProvider();
  provider->Start();
  provider->Stop();
  // Just ensure that there's no asserts on startup, shutdown, or destroy.
}

// http://crbug.com/105348
TEST_F(GamepadProviderTest, FLAKY_PollingAccess) {
  using namespace gamepad;

  Provider* provider = CreateProvider();
  provider->Start();

  WebKit::WebGamepads test_data;
  test_data.length = 1;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].buttonsLength = 1;
  test_data.items[0].axesLength = 2;
  test_data.items[0].buttons[0] = 1.f;
  test_data.items[0].axes[0] = -1.f;
  test_data.items[0].axes[1] = .5f;
  mock_data_fetcher_->SetData(test_data);

  main_message_loop_.RunAllPending();

  mock_data_fetcher_->WaitForDataRead();

  // Renderer-side, pull data out of poll buffer.
  base::SharedMemoryHandle handle =
      provider->GetRendererSharedMemoryHandle(base::GetCurrentProcessHandle());
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, true));
  EXPECT_TRUE(shared_memory->Map(sizeof(GamepadHardwareBuffer)));
  void* mem = shared_memory->memory();

  GamepadHardwareBuffer* hwbuf = static_cast<GamepadHardwareBuffer*>(mem);
  // See gamepad_hardware_buffer.h for details on the read discipline.
  base::subtle::Atomic32 start, end;
  WebKit::WebGamepads output;
  int contention_count;

  // Here we're attempting to test the read discipline during contention. If
  // we fail to read this many times, then the read thread is starving, and we
  // should fail the test.
  for (contention_count = 0; contention_count < 1000; ++contention_count) {
    end = base::subtle::Acquire_Load(&hwbuf->end_marker);
    memcpy(&output, &hwbuf->buffer, sizeof(output));
    start = base::subtle::Acquire_Load(&hwbuf->start_marker);
    if (start == end)
        break;
    base::PlatformThread::YieldCurrentThread();
  }
  EXPECT_GT(1000, contention_count);
  EXPECT_EQ(1u, output.length);
  EXPECT_EQ(1u, output.items[0].buttonsLength);
  EXPECT_EQ(1.f, output.items[0].buttons[0]);
  EXPECT_EQ(2u, output.items[0].axesLength);
  EXPECT_EQ(-1.f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);

  provider->Stop();
}

}  // namespace
