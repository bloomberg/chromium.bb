// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::A;
using testing::AtLeast;
using testing::HasSubstr;
using testing::Return;
using testing::_;

static const char* const kPDFTestFile = "printing/cloud_print_unittest.pdf";
static const char* const kEmptyPDFTestFile =
    "printing/cloud_print_emptytest.pdf";

FilePath GetTestDataFileName() {
  FilePath test_data_directory;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
  FilePath test_file = test_data_directory.AppendASCII(kPDFTestFile);
  return test_file;
}

FilePath GetEmptyDataFileName() {
  FilePath test_data_directory;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
  FilePath test_file = test_data_directory.AppendASCII(kEmptyPDFTestFile);
  return test_file;
}

char* GetTestData() {
  static std::string sTestFileData;
  if (sTestFileData.empty()) {
    FilePath test_file = GetTestDataFileName();
    file_util::ReadFileToString(test_file, &sTestFileData);
  }
  return &sTestFileData[0];
}

namespace internal_cloud_print_helpers {

class MockCloudPrintFlowHandler : public CloudPrintFlowHandler {
 public:
  MOCK_METHOD0(RegisterMessages,
               void());
  MOCK_METHOD3(Observe,
               void(NotificationType type,
                    const NotificationSource& source,
                    const NotificationDetails& details));
  MOCK_METHOD0(CreateCloudPrintDataSender,
               scoped_refptr<CloudPrintDataSender>());
};

class MockCloudPrintHtmlDialogDelegate : public CloudPrintHtmlDialogDelegate {
 public:
  MOCK_CONST_METHOD0(IsDialogModal,
      bool());
  MOCK_CONST_METHOD0(GetDialogTitle,
      std::wstring());
  MOCK_CONST_METHOD0(GetDialogContentURL,
      GURL());
  MOCK_CONST_METHOD1(GetDOMMessageHandlers,
      void(std::vector<DOMMessageHandler*>* handlers));
  MOCK_CONST_METHOD1(GetDialogSize,
      void(gfx::Size* size));
  MOCK_CONST_METHOD0(GetDialogArgs,
      std::string());
  MOCK_METHOD1(OnDialogClosed,
      void(const std::string& json_retval));
  MOCK_METHOD2(OnCloseContents,
      void(TabContents* source, bool *out_close_dialog));
};

}  // namespace internal_cloud_print_helpers

using internal_cloud_print_helpers::CloudPrintDataSenderHelper;
using internal_cloud_print_helpers::CloudPrintDataSender;

class MockExternalHtmlDialogUI : public ExternalHtmlDialogUI {
 public:
  MOCK_METHOD1(RenderViewCreated,
      void(RenderViewHost* render_view_host));
};

class MockCloudPrintDataSenderHelper : public CloudPrintDataSenderHelper {
 public:
  // TODO(scottbyer): At some point this probably wants to use a
  // MockTabContents instead of NULL, and to pre-load it with a bunch
  // of expects/results.
  MockCloudPrintDataSenderHelper() : CloudPrintDataSenderHelper(NULL) {}
  MOCK_METHOD1(CallJavascriptFunction, void(const std::wstring&));
  MOCK_METHOD2(CallJavascriptFunction, void(const std::wstring&,
                                            const Value& arg1));
  MOCK_METHOD3(CallJavascriptFunction, void(const std::wstring&,
                                            const Value& arg1,
                                            const Value& arg2));
};

class CloudPrintURLTest : public testing::Test {
 public:
  CloudPrintURLTest() {}

 protected:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
  }

  scoped_ptr<Profile> profile_;
};

TEST_F(CloudPrintURLTest, CheckDefaultURLs) {
  std::string service_url =
      internal_cloud_print_helpers::CloudPrintService(profile_.get()).
      GetCloudPrintServiceURL().spec();
  EXPECT_THAT(service_url, HasSubstr("www.google.com"));
  EXPECT_THAT(service_url, HasSubstr("cloudprint"));

  std::string dialog_url =
      internal_cloud_print_helpers::CloudPrintService(profile_.get()).
      GetCloudPrintServiceDialogURL().spec();
  EXPECT_THAT(dialog_url, HasSubstr("www.google.com"));
  EXPECT_THAT(dialog_url, HasSubstr("/cloudprint/"));
  EXPECT_THAT(dialog_url, HasSubstr("/client/"));
  EXPECT_THAT(dialog_url, Not(HasSubstr("cloudprint/cloudprint")));
  EXPECT_THAT(dialog_url, HasSubstr("/dialog.html"));

  // Repeat to make sure there isn't a transient glitch.
  dialog_url =
      internal_cloud_print_helpers::CloudPrintService(profile_.get()).
      GetCloudPrintServiceDialogURL().spec();
  EXPECT_THAT(dialog_url, HasSubstr("www.google.com"));
  EXPECT_THAT(dialog_url, HasSubstr("/cloudprint/"));
  EXPECT_THAT(dialog_url, HasSubstr("/client/"));
  EXPECT_THAT(dialog_url, Not(HasSubstr("cloudprint/cloudprint")));
  EXPECT_THAT(dialog_url, HasSubstr("/dialog.html"));
}

// Testing for CloudPrintDataSender needs a mock DOMUI.
class CloudPrintDataSenderTest : public testing::Test {
 public:
  CloudPrintDataSenderTest()
      : file_thread_(ChromeThread::FILE, &message_loop_),
        io_thread_(ChromeThread::IO, &message_loop_) {}

 protected:
  virtual void SetUp() {
    mock_helper_.reset(new MockCloudPrintDataSenderHelper);
    print_data_sender_ =
        new CloudPrintDataSender(mock_helper_.get());
  }

  scoped_refptr<CloudPrintDataSender> print_data_sender_;
  scoped_ptr<MockCloudPrintDataSenderHelper> mock_helper_;

  MessageLoop message_loop_;
  ChromeThread file_thread_;
  ChromeThread io_thread_;
};

// TODO(scottbyer): DISABLED until the binary test file can get
// checked in separate from the patch.
TEST_F(CloudPrintDataSenderTest, DISABLED_CanSend) {
  EXPECT_CALL(*mock_helper_, CallJavascriptFunction(_, _, _)).
      WillOnce(Return());

  FilePath test_data_file_name = GetTestDataFileName();
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                         NewRunnableMethod(
                             print_data_sender_.get(),
                             &CloudPrintDataSender::ReadPrintDataFile,
                             test_data_file_name));
  MessageLoop::current()->RunAllPending();
}

TEST_F(CloudPrintDataSenderTest, BadFile) {
  EXPECT_CALL(*mock_helper_, CallJavascriptFunction(_, _, _)).Times(0);

#if defined(OS_WIN)
  FilePath bad_data_file_name(L"/some/file/that/isnot/there");
#else
  FilePath bad_data_file_name("/some/file/that/isnot/there");
#endif
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                         NewRunnableMethod(
                             print_data_sender_.get(),
                             &CloudPrintDataSender::ReadPrintDataFile,
                             bad_data_file_name));
  MessageLoop::current()->RunAllPending();
}

TEST_F(CloudPrintDataSenderTest, EmptyFile) {
  EXPECT_CALL(*mock_helper_, CallJavascriptFunction(_, _, _)).Times(0);

  FilePath empty_data_file_name = GetEmptyDataFileName();
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                         NewRunnableMethod(
                             print_data_sender_.get(),
                             &CloudPrintDataSender::ReadPrintDataFile,
                             empty_data_file_name));
  MessageLoop::current()->RunAllPending();
}

// Testing for CloudPrintFlowHandler needs a mock
// CloudPrintHtmlDialogDelegate, mock CloudPrintDataSender, and a mock
// DOMUI.

// Testing for CloudPrintHtmlDialogDelegate needs a mock
// CloudPrintFlowHandler.

// Testing for ExternalHtmlDialogUI needs a mock TabContents, mock
// CloudPrintHtmlDialogDelegate (provided through the mock
// tab_contents)

// Testing for PrintDialogCloud needs a mock Browser.
