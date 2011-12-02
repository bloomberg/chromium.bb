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

namespace content {

namespace {

using WebKit::WebGamepads;

class MockDataFetcher : public GamepadDataFetcher {
 public:
  MockDataFetcher(WebGamepads& test_data) : read_data_(false, false) {
    test_data_ = test_data;
  }
  virtual void GetGamepadData(WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE {
    *pads = test_data_;
    read_data_.Signal();
  }

  void WaitForDataRead() { return read_data_.Wait(); }

  WebGamepads test_data_;
  base::WaitableEvent read_data_;
};

// Main test fixture
class GamepadProviderTest : public testing::Test {
 public:
  GamepadProvider* CreateProvider(WebGamepads& test_data) {
#if defined(OS_MACOSX)
    base::SystemMonitor::AllocateSystemIOPorts();
#endif
    system_monitor_.reset(new base::SystemMonitor);
    mock_data_fetcher_ = new MockDataFetcher(test_data);
    provider_ = new GamepadProvider(mock_data_fetcher_);
    return provider_.get();
  }

 protected:
  GamepadProviderTest() {
  }

  MessageLoop main_message_loop_;
  scoped_ptr<base::SystemMonitor> system_monitor_;
  MockDataFetcher* mock_data_fetcher_;
  scoped_refptr<GamepadProvider> provider_;
};

// Crashes. http://crbug.com/106163
TEST_F(GamepadProviderTest, DISABLED_PollingAccess) {
  WebGamepads test_data;
  test_data.length = 1;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].buttonsLength = 1;
  test_data.items[0].axesLength = 2;
  test_data.items[0].buttons[0] = 1.f;
  test_data.items[0].axes[0] = -1.f;
  test_data.items[0].axes[1] = .5f;

  GamepadProvider* provider = CreateProvider(test_data);

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
  WebGamepads output;

  base::subtle::Atomic32 version;
  do {
    version = hwbuf->sequence.ReadBegin();
    memcpy(&output, &hwbuf->buffer, sizeof(output));
  } while (hwbuf->sequence.ReadRetry(version));

  EXPECT_EQ(1u, output.length);
  EXPECT_EQ(1u, output.items[0].buttonsLength);
  EXPECT_EQ(1.f, output.items[0].buttons[0]);
  EXPECT_EQ(2u, output.items[0].axesLength);
  EXPECT_EQ(-1.f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);
}

}  // namespace

} // namespace content
