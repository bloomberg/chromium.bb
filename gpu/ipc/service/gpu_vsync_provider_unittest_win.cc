// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_vsync_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"

namespace gpu {

class GpuVSyncProviderTest : public testing::Test {
 public:
  GpuVSyncProviderTest()
      : vsync_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~GpuVSyncProviderTest() override {}

  void SetUp() override {}

  void TearDown() override {}

  void OnVSync(base::TimeTicks timestamp) {
    // This is called on VSync worker thread.
    base::AutoLock lock(lock_);
    if (++vsync_count_ == 3)
      vsync_event_.Signal();
  }

  int vsync_count() {
    base::AutoLock lock(lock_);
    return vsync_count_;
  }

  void reset_vsync_count() {
    base::AutoLock lock(lock_);
    vsync_count_ = 0;
  }

 protected:
  base::WaitableEvent vsync_event_;

 private:
  base::Lock lock_;
  int vsync_count_ = 0;
};

TEST_F(GpuVSyncProviderTest, VSyncSignalTest) {
  SurfaceHandle window = ui::GetHiddenWindow();

  std::unique_ptr<GpuVSyncProvider> provider = GpuVSyncProvider::Create(
      base::Bind(&GpuVSyncProviderTest::OnVSync, base::Unretained(this)),
      window);

  constexpr base::TimeDelta wait_timeout =
      base::TimeDelta::FromMilliseconds(300);

  // Verify that there are no VSync signals before provider is enabled
  bool wait_result = vsync_event_.TimedWait(wait_timeout);
  EXPECT_FALSE(wait_result);
  EXPECT_EQ(0, vsync_count());

  provider->EnableVSync(true);

  vsync_event_.Wait();

  provider->EnableVSync(false);

  // Verify that VSync callbacks stop coming after disabling.
  // Please note that it might still be possible for one
  // callback to be in flight on VSync worker thread, so |vsync_count_|
  // could still be incremented once, but not enough times to trigger
  // |vsync_event_|.
  reset_vsync_count();
  wait_result = vsync_event_.TimedWait(wait_timeout);
  EXPECT_FALSE(wait_result);
}

}  // namespace gpu
