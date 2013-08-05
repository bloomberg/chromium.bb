// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_context.h"

#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class UsbContextTest : public testing::Test {
 protected:
  class UsbContextForTest : public UsbContext {
   public:
    UsbContextForTest() : UsbContext() {}
   private:
    virtual ~UsbContextForTest() {}
    DISALLOW_COPY_AND_ASSIGN(UsbContextForTest);
  };
};

}  // namespace

#if defined(OS_LINUX)
// Linux trybot does not support usb.
#define MAYBE_GracefulShutdown DISABLED_GracefulShutdown
#elif defined(OS_ANDROID)
// Android build does not include usb support.
#define MAYBE_GracefulShutdown DISABLED_GracefulShutdown
#else
#define MAYBE_GracefulShutdown GracefulShutdown
#endif

TEST_F(UsbContextTest, MAYBE_GracefulShutdown) {
  base::TimeTicks start = base::TimeTicks::Now();
  {
    scoped_refptr<UsbContextForTest> context(new UsbContextForTest());
  }
  base::TimeDelta elapse = base::TimeTicks::Now() - start;
  if (elapse > base::TimeDelta::FromSeconds(2)) {
    FAIL();
  }
}
