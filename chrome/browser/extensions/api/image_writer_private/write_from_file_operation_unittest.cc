// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"

namespace extensions {
namespace image_writer {

using testing::_;
using testing::Lt;
using testing::AnyNumber;
using testing::AtLeast;

class ImageWriterFromFileTest : public ImageWriterUnitTestBase {
};

TEST_F(ImageWriterFromFileTest, InvalidFile) {
  MockOperationManager manager;

  scoped_refptr<WriteFromFileOperation> op = new WriteFromFileOperation(
    manager.AsWeakPtr(),
    kDummyExtensionId,
    test_image_,
    test_device_.AsUTF8Unsafe());

  base::DeleteFile(test_image_, false);

  EXPECT_CALL(manager, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager, OnError(kDummyExtensionId,
                               image_writer_api::STAGE_UNKNOWN,
                               0,
                               error::kImageInvalid)).Times(1);

  op->Start();

  base::RunLoop().RunUntilIdle();
}

}  // namespace image_writer
}  // namespace extensions
