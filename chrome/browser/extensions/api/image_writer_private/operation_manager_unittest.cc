// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/event_router.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {
namespace image_writer {

// A fake for the EventRouter. If tests require monitoring of interaction with
// the event router put the logic here.
class FakeEventRouter : public extensions::EventRouter {
 public:
  explicit FakeEventRouter(Profile* profile) : EventRouter(profile, NULL) {}

  virtual void DispatchEventToExtension(
      const std::string& extension_id,
      scoped_ptr<extensions::Event> event) OVERRIDE {
    // Do nothing with the event as no tests currently care.
  }
};

// A fake ExtensionSystem that returns a FakeEventRouter for event_router().
class FakeExtensionSystem : public extensions::TestExtensionSystem {
 public:
  explicit FakeExtensionSystem(Profile* profile)
      : TestExtensionSystem(profile) {
    fake_event_router_.reset(new FakeEventRouter(profile));
  }

  virtual EventRouter* event_router() OVERRIDE {
    return fake_event_router_.get();
  }

 private:
  scoped_ptr<FakeEventRouter> fake_event_router_;
};

// Factory function to register for the ExtensionSystem.
KeyedService* BuildFakeExtensionSystem(content::BrowserContext* profile) {
  return new FakeExtensionSystem(static_cast<Profile*>(profile));
}

namespace {

class ImageWriterOperationManagerTest : public ImageWriterUnitTestBase {
 public:
  void StartCallback(bool success, const std::string& error) {
    started_ = true;
    start_success_ = success;
    start_error_ = error;
  }

  void CancelCallback(bool success, const std::string& error) {
    cancelled_ = true;
    cancel_success_ = true;
    cancel_error_ = error;
  }

 protected:
  ImageWriterOperationManagerTest()
      : started_(false),
        start_success_(false) {
  }

  virtual void SetUp() OVERRIDE {
    ImageWriterUnitTestBase::SetUp();
    extension_system_ = static_cast<FakeExtensionSystem*>(
        ExtensionSystemFactory::GetInstance()->
            SetTestingFactoryAndUse(&test_profile_, &BuildFakeExtensionSystem));
    event_router_ = static_cast<FakeEventRouter*>(
        extension_system_->event_router());
  }

  bool started_;
  bool start_success_;
  std::string start_error_;

  bool cancelled_;
  bool cancel_success_;
  std::string cancel_error_;

  TestingProfile test_profile_;
  FakeExtensionSystem* extension_system_;
  FakeEventRouter* event_router_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ImageWriterOperationManagerTest, WriteFromFile) {
  OperationManager manager(&test_profile_);

  manager.StartWriteFromFile(
      kDummyExtensionId,
      test_utils_.GetImagePath(),
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::Bind(&ImageWriterOperationManagerTest::StartCallback,
                 base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);

  manager.CancelWrite(
      kDummyExtensionId,
      base::Bind(&ImageWriterOperationManagerTest::CancelCallback,
                 base::Unretained(this)));

  EXPECT_TRUE(cancelled_);
  EXPECT_TRUE(cancel_success_);
  EXPECT_EQ("", cancel_error_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterOperationManagerTest, DestroyPartitions) {
  OperationManager manager(&test_profile_);

  manager.DestroyPartitions(
      kDummyExtensionId,
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::Bind(&ImageWriterOperationManagerTest::StartCallback,
                 base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);

  manager.CancelWrite(
      kDummyExtensionId,
      base::Bind(&ImageWriterOperationManagerTest::CancelCallback,
                 base::Unretained(this)));

  EXPECT_TRUE(cancelled_);
  EXPECT_TRUE(cancel_success_);
  EXPECT_EQ("", cancel_error_);

  base::RunLoop().RunUntilIdle();
}

} // namespace
} // namespace image_writer
} // namespace extensions
