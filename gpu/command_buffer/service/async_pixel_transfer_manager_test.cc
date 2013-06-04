// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager_test.h"

#include "gpu/command_buffer/service/async_pixel_transfer_delegate_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

AsyncPixelTransferManagerTest::AsyncPixelTransferManagerTest()
    : delegate_(
          new ::testing::StrictMock<gpu::MockAsyncPixelTransferDelegate>) {}

AsyncPixelTransferManagerTest::~AsyncPixelTransferManagerTest() {}

AsyncPixelTransferDelegate*
AsyncPixelTransferManagerTest::GetAsyncPixelTransferDelegate() {
  return delegate_.get();
}

}  // namespace gpu
