// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password_exporter_for_testing.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/password_test_util.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakePasswordSerialzerBridge : NSObject<PasswordSerializerBridge>

// Allows for on demand execution of the block that handles the serialized
// passwords.
- (void)executeHandler;

@end

@implementation FakePasswordSerialzerBridge {
  // Handler processing the serialized passwords.
  void (^_serializedPasswordsHandler)(std::string);
}

- (void)serializePasswords:
            (std::vector<std::unique_ptr<autofill::PasswordForm>>)passwords
                   handler:(void (^)(std::string))serializedPasswordsHandler {
  _serializedPasswordsHandler = serializedPasswordsHandler;
}

- (void)executeHandler {
  _serializedPasswordsHandler("test serialized passwords");
}
@end

@interface FakePasswordFileWriter : NSObject<FileWriterProtocol>

// Indicates if the writing of the file was finished successfully or with an
// error.
@property(nonatomic, assign) WriteToURLStatus writingStatus;

@end

@implementation FakePasswordFileWriter

@synthesize writingStatus = _writingStatus;

- (void)writeData:(NSString*)data
            toURL:(NSURL*)fileURL
          handler:(void (^)(WriteToURLStatus))handler {
  handler(self.writingStatus);
}

@end

namespace {
class PasswordExporterTest : public PlatformTest {
 public:
  PasswordExporterTest() = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    password_exporter_delegate_ =
        OCMProtocolMock(@protocol(PasswordExporterDelegate));
    password_exporter_ = [[PasswordExporter alloc]
        initWithReauthenticationModule:mock_reauthentication_module_
                              delegate:password_exporter_delegate_];
  }

  std::vector<std::unique_ptr<autofill::PasswordForm>> CreatePasswordList() {
    auto password_form = std::make_unique<autofill::PasswordForm>();
    password_form->origin = GURL("http://accounts.google.com/a/LoginAuth");
    password_form->username_value = base::ASCIIToUTF16("test@testmail.com");
    password_form->password_value = base::ASCIIToUTF16("test1");

    std::vector<std::unique_ptr<autofill::PasswordForm>> password_forms;
    password_forms.push_back(std::move(password_form));
    return password_forms;
  }

  void TearDown() override {
    NSURL* passwords_file_url = GetPasswordsFileURL();
    if ([[NSFileManager defaultManager]
            fileExistsAtPath:[passwords_file_url path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:passwords_file_url
                                                error:nil];
    }
    PlatformTest::TearDown();
  }

  NSURL* GetPasswordsFileURL() {
    NSString* passwords_file_name =
        [l10n_util::GetNSString(IDS_PASSWORD_MANAGER_DEFAULT_EXPORT_FILENAME)
            stringByAppendingString:@".csv"];
    return [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES]
        URLByAppendingPathComponent:passwords_file_name
                        isDirectory:NO];
  }

  BOOL PasswordFileExists() {
    return [[NSFileManager defaultManager]
        fileExistsAtPath:[GetPasswordsFileURL() path]];
  }

  id password_exporter_delegate_;
  PasswordExporter* password_exporter_;
  MockReauthenticationModule* mock_reauthentication_module_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(PasswordExporterTest, PasswordFileWriteAuthSuccessful) {
  mock_reauthentication_module_.shouldSucceed = YES;
  NSURL* passwords_file_url = GetPasswordsFileURL();

  OCMExpect([password_exporter_delegate_
      showActivityViewWithActivityItems:[OCMArg isEqual:@[ passwords_file_url ]]
                      completionHandler:[OCMArg any]]);

  [password_exporter_ startExportFlow:CreatePasswordList()];

  // Wait for all asynchronous tasks to complete.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  EXPECT_TRUE(PasswordFileExists());
}

TEST_F(PasswordExporterTest, PasswordFileWriteAuthFailed) {
  mock_reauthentication_module_.shouldSucceed = NO;
  FakePasswordSerialzerBridge* fake_password_serializer_bridge =
      [[FakePasswordSerialzerBridge alloc] init];
  [password_exporter_
      setPasswordSerializerBridge:fake_password_serializer_bridge];
  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];

  // Use @try/@catch as -reject raises an exception.
  @try {
    [password_exporter_ startExportFlow:CreatePasswordList()];

    // Wait for all asynchronous tasks to complete.
    scoped_task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
  // Serializing passwords hasn't finished.
  EXPECT_FALSE(PasswordFileExists());
  EXPECT_TRUE(password_exporter_.isExporting);

  [fake_password_serializer_bridge executeHandler];

  // Serializing password has finished, but reauthentication was not successful.
  EXPECT_FALSE(PasswordFileExists());
  EXPECT_FALSE(password_exporter_.isExporting);
}

TEST_F(PasswordExporterTest,
       PasswordFileNotWrittenBeforeSerializationFinished) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordSerialzerBridge* fake_password_serializer_bridge =
      [[FakePasswordSerialzerBridge alloc] init];
  [password_exporter_
      setPasswordSerializerBridge:fake_password_serializer_bridge];

  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];

  // Use @try/@catch as -reject raises an exception.
  @try {
    [password_exporter_ startExportFlow:CreatePasswordList()];

    // Wait for all asynchronous tasks to complete.
    scoped_task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
  EXPECT_FALSE(PasswordFileExists());
}

TEST_F(PasswordExporterTest, WritingFailedOutOfDiskSpace) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus =
      WriteToURLStatus::OUT_OF_DISK_SPACE_ERROR;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  OCMExpect([password_exporter_delegate_
      showExportErrorAlertWithLocalizedReason:
          l10n_util::GetNSString(
              IDS_IOS_EXPORT_PASSWORDS_OUT_OF_SPACE_ALERT_MESSAGE)]);

  [password_exporter_ startExportFlow:CreatePasswordList()];

  // Wait for all asynchronous tasks to complete.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  EXPECT_FALSE(password_exporter_.isExporting);
}

TEST_F(PasswordExporterTest, WritingFailedUnknownError) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus = WriteToURLStatus::UNKNOWN_ERROR;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  OCMExpect([password_exporter_delegate_
      showExportErrorAlertWithLocalizedReason:
          l10n_util::GetNSString(
              IDS_IOS_EXPORT_PASSWORDS_UNKNOWN_ERROR_ALERT_MESSAGE)]);

  [password_exporter_ startExportFlow:CreatePasswordList()];

  // Wait for all asynchronous tasks to complete.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  EXPECT_FALSE(password_exporter_.isExporting);
}

}  // namespace
