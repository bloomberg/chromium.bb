// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"

namespace extensions {
namespace image_writer {

using testing::_;
using testing::AnyNumber;

namespace {

class ImageWriterDestroyPartitionsOperationTest
    : public ImageWriterUnitTestBase {
};

// Tests that the DestroyPartitionsOperation can successfully zero the first
// kPartitionTableSize bytes of an image.
TEST_F(ImageWriterDestroyPartitionsOperationTest, DestroyPartitions) {
  MockOperationManager manager;
  base::RunLoop loop;

  scoped_refptr<DestroyPartitionsOperation> operation(
      new DestroyPartitionsOperation(manager.AsWeakPtr(),
                                     kDummyExtensionId,
                                     test_device_path_.AsUTF8Unsafe()));

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::STAGE_WRITE,
                                  _)).Times(AnyNumber());
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(1);
  EXPECT_CALL(manager, OnError(kDummyExtensionId, _, _, _)).Times(0);
#else
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager, OnError(kDummyExtensionId,
                               _,
                               _,
                               error::kUnsupportedOperation)).Times(1);
#endif

  operation->Start();

  loop.RunUntilIdle();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  scoped_ptr<char[]> image_data(new char[kPartitionTableSize]);
  scoped_ptr<char[]> zeroes(new char[kPartitionTableSize]);
  memset(zeroes.get(), 0, kPartitionTableSize);
  ASSERT_EQ(kPartitionTableSize, base::ReadFile(test_device_path_,
                                                image_data.get(),
                                                kPartitionTableSize));
  EXPECT_EQ(0, memcmp(image_data.get(), zeroes.get(), kPartitionTableSize));
#endif
}

} // namespace
} // namespace image_writer
} // namespace extensions
