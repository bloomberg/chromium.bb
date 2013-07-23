// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test UsbServiceTest;

#if defined(OS_LINUX)
// Linux trybot does not support usb.
#define MAYBE_GracefulShutdown DISABLED_GracefulShutdown
#elif defined(OS_ANDROID)
// Android build does not include usb support.
#define MAYBE_GracefulShutdown DISABLED_GracefulShutdown
#else
#define MAYBE_GracefulShutdown GracefulShutdown
#endif

TEST_F(UsbServiceTest, MAYBE_GracefulShutdown) {
  base::TimeTicks start = base::TimeTicks::Now();
  scoped_ptr<UsbService> service(new UsbService());
  service->Shutdown();
  base::TimeDelta elapse = base::TimeTicks::Now() - start;
  if (elapse > base::TimeDelta::FromSeconds(2)) {
    FAIL();
  }
}

}  // namespace
