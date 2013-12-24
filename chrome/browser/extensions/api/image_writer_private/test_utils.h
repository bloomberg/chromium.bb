// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace image_writer {

const char kDummyExtensionId[] = "DummyExtension";
const char kTestData[] = "This is some test data, padded.  ";

class MockOperationManager : public OperationManager {
 public:
  MockOperationManager();
  virtual ~MockOperationManager();

  MOCK_METHOD3(OnProgress, void(const ExtensionId& extension_id,
                                image_writer_api::Stage stage,
                                int progress));
  // Callback for completion events.
  MOCK_METHOD1(OnComplete, void(const std::string& extension_id));

  // Callback for error events.
  MOCK_METHOD4(OnError, void(const ExtensionId& extension_id,
                             image_writer_api::Stage stage,
                             int progress,
                             const std::string& error_message));
};

class ImageWriterUnitTestBase : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  base::FilePath test_image_;
  base::FilePath test_device_;
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_
