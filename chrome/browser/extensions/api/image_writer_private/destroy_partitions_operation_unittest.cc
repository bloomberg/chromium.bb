// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"

namespace extensions {
namespace image_writer {
namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;

class ImageWriterDestroyPartitionsOperationTest
    : public ImageWriterUnitTestBase {};

TEST_F(ImageWriterDestroyPartitionsOperationTest, EndToEnd) {
  TestingProfile profile;
  MockOperationManager manager(&profile);
  scoped_refptr<FakeImageWriterClient> client = FakeImageWriterClient::Create();

  scoped_refptr<DestroyPartitionsOperation> operation(
      new DestroyPartitionsOperation(manager.AsWeakPtr(),
                                     kDummyExtensionId,
                                     test_device_path_.AsUTF8Unsafe()));

#if !defined(OS_CHROMEOS)
  operation->SetUtilityClientForTesting(client);
#endif

  EXPECT_CALL(
      manager,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::STAGE_WRITE,
                                  _)).Times(AnyNumber());
  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 100))
      .Times(AtLeast(1));
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(1);
  EXPECT_CALL(manager, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->Start();

  base::RunLoop().RunUntilIdle();

#if !defined(OS_CHROMEOS)
  client->Progress(0);
  client->Progress(50);
  client->Progress(100);
  client->Success();

  base::RunLoop().RunUntilIdle();
#endif
}

} // namespace
} // namespace image_writer
} // namespace extensions
