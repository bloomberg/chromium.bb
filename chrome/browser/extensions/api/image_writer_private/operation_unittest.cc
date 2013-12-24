// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"

namespace extensions {
namespace image_writer {

class ImageWriterOperationTest : public ImageWriterUnitTestBase {
};

class DummyOperation : public Operation {
 public:
  DummyOperation(base::WeakPtr<OperationManager> manager,
                 const ExtensionId& extension_id,
                 const std::string& storage_unit_id)
      : Operation(manager, extension_id, storage_unit_id) {};
  virtual void Start() OVERRIDE {};
 private:
  virtual ~DummyOperation() {};
};

TEST_F(ImageWriterOperationTest, Create) {
  MockOperationManager manager;
  scoped_refptr<Operation> op(new DummyOperation(manager.AsWeakPtr(),
                                                 kDummyExtensionId,
                                                 test_device_.AsUTF8Unsafe()));

  EXPECT_EQ(0, op->GetProgress());
  EXPECT_EQ(image_writer_api::STAGE_UNKNOWN, op->GetStage());
}

}  // namespace image_writer
}  // namespace extensions
