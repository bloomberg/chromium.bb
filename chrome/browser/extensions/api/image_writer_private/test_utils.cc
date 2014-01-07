// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"

namespace extensions {
namespace image_writer {

MockOperationManager::MockOperationManager()
    : OperationManager(NULL) {
}

MockOperationManager::~MockOperationManager() {
}

void ImageWriterUnitTestBase::SetUp() {
  ASSERT_TRUE(base::CreateTemporaryFile(&test_image_));
  ASSERT_TRUE(base::CreateTemporaryFile(&test_device_));

  ASSERT_EQ(32, file_util::WriteFile(test_image_, kTestData, 32));
}

void ImageWriterUnitTestBase::TearDown() {
  base::DeleteFile(test_image_, false);
  base::DeleteFile(test_device_, false);
}

}  // namespace image_writer
}  // namespace extensions
