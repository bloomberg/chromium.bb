// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace image_writer {

const char kDummyExtensionId[] = "DummyExtension";

// Default file size to use in tests.  Currently 32kB.
const int kTestFileSize = 32 * 1024;
// Pattern to use in the image file.
const int kImagePattern = 0x55555555; // 01010101
// Pattern to use in the device file.
const int kDevicePattern = 0xAAAAAAAA; // 10101010

// A mock around the operation manager for tracking callbacks.  Note that there
// are non-virtual methods on this class that should not be called in tests.
class MockOperationManager : public OperationManager {
 public:
  MockOperationManager();
  explicit MockOperationManager(Profile* profile);
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

// Base class for unit tests that manages creating image and device files.
class ImageWriterUnitTestBase : public testing::Test {
 public:
  ImageWriterUnitTestBase();
  virtual ~ImageWriterUnitTestBase();

 protected:
  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  // Fills |file| with |length| bytes of |pattern|, overwriting any existing
  // data.
  bool FillFile(const base::FilePath& file,
                const int pattern,
                const int length);

  base::ScopedTempDir temp_dir_;
  base::FilePath test_image_path_;
  base::FilePath test_device_path_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TEST_UTILS_H_
