// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {
namespace image_writer {

using testing::_;
using testing::Lt;
using testing::AnyNumber;
using testing::AtLeast;

class ImageWriterOperationManagerTest
    : public ImageWriterUnitTestBase {
 public:
  void StartCallback(bool success, const std::string& error) {
    started_ = true;
    start_success_ = success;
    start_error_ = error;
  }

 protected:
  ImageWriterOperationManagerTest()
      : started_(false),
        start_success_(false),
        command_line_(CommandLine::NO_PROGRAM) {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&test_profile_));
    extension_service_ = test_extension_system->CreateExtensionService(
        &command_line_,
        base::FilePath(),
        false);
  }

  bool started_;
  bool start_success_;
  std::string start_error_;

  CommandLine command_line_;
  TestingProfile test_profile_;
  ExtensionService* extension_service_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ImageWriterOperationManagerTest, WriteFromFile) {
  OperationManager manager(&test_profile_);

  manager.StartWriteFromFile(
    kDummyExtensionId,
    test_image_,
    test_device_.AsUTF8Unsafe(),
    base::Bind(&ImageWriterOperationManagerTest::StartCallback,
               base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);
}

}  // namespace image_writer
}  // namespace extensions
