// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;
using testing::A;
using testing::AtLeast;
using testing::Eq;
using testing::HasSubstr;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using testing::StrEq;
using testing::_;
using ui::ExternalWebDialogUI;

const char kPDFTestFile[] = "printing/cloud_print_unittest.pdf";
const char kMockJobTitle[] = "Mock Job Title";
const char kMockPrintTicket[] = "Resolution=300";


base::FilePath GetTestDataFileName() {
  base::FilePath test_data_directory;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
  base::FilePath test_file = test_data_directory.AppendASCII(kPDFTestFile);
  return test_file;
}

char* GetTestData() {
  static std::string sTestFileData;
  if (sTestFileData.empty()) {
    base::FilePath test_file = GetTestDataFileName();
    base::ReadFileToString(test_file, &sTestFileData);
  }
  return &sTestFileData[0];
}

MATCHER_P(StringValueEq, expected, "StringValue") {
  if (expected->Equals(&arg))
    return true;
  std::string expected_string, arg_string;
  expected->GetAsString(&expected_string);
  arg.GetAsString(&arg_string);
  *result_listener << "'" << arg_string
                   << "' (expected '" << expected_string << "')";
  return false;
}

namespace internal_cloud_print_helpers {

class MockCloudPrintFlowHandler
    : public CloudPrintFlowHandler,
      public base::SupportsWeakPtr<MockCloudPrintFlowHandler> {
 public:
  MockCloudPrintFlowHandler(const base::string16& title,
                            const base::string16& print_ticket,
                            const std::string& file_type)
      : CloudPrintFlowHandler(NULL, title, print_ticket, file_type) {}
  MOCK_METHOD0(DestructorCalled, void());
  MOCK_METHOD0(RegisterMessages, void());
  MOCK_METHOD3(Observe,
               void(int type,
                    const content::NotificationSource& source,
                    const content::NotificationDetails& details));
  MOCK_METHOD1(SetDialogDelegate,
               void(CloudPrintWebDialogDelegate* delegate));
  MOCK_METHOD0(CreateCloudPrintDataSender,
               scoped_refptr<CloudPrintDataSender>());
};

class MockCloudPrintWebDialogDelegate : public CloudPrintWebDialogDelegate {
 public:
  MOCK_CONST_METHOD0(GetDialogModalType,
      ui::ModalType());
  MOCK_CONST_METHOD0(GetDialogTitle,
      base::string16());
  MOCK_CONST_METHOD0(GetDialogContentURL,
      GURL());
  MOCK_CONST_METHOD1(GetWebUIMessageHandlers,
      void(std::vector<WebUIMessageHandler*>* handlers));
  MOCK_CONST_METHOD1(GetDialogSize,
      void(gfx::Size* size));
  MOCK_CONST_METHOD0(GetDialogArgs,
      std::string());
  MOCK_METHOD1(OnDialogClosed,
      void(const std::string& json_retval));
  MOCK_METHOD2(OnCloseContents,
      void(WebContents* source, bool *out_close_dialog));
};

}  // namespace internal_cloud_print_helpers

using internal_cloud_print_helpers::CloudPrintDataSenderHelper;
using internal_cloud_print_helpers::CloudPrintDataSender;

class MockExternalWebDialogUI : public ExternalWebDialogUI {
 public:
  MOCK_METHOD1(RenderViewCreated,
               void(content::RenderViewHost* render_view_host));
};

class MockCloudPrintDataSenderHelper : public CloudPrintDataSenderHelper {
 public:
  // TODO(scottbyer): At some point this probably wants to use a
  // MockTabContents instead of NULL, and to pre-load it with a bunch
  // of expects/results.
  MockCloudPrintDataSenderHelper() : CloudPrintDataSenderHelper(NULL) {}
  MOCK_METHOD3(CallJavascriptFunction, void(const std::string&,
                                            const base::Value& arg1,
                                            const base::Value& arg2));
};

// Testing for CloudPrintDataSender needs a mock WebUI.
class CloudPrintDataSenderTest : public testing::Test {
 public:
  CloudPrintDataSenderTest()
      : file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

 protected:
  virtual void SetUp() {
    mock_helper_.reset(new MockCloudPrintDataSenderHelper);
  }

  scoped_refptr<CloudPrintDataSender> CreateSender(
      const base::RefCountedString* data) {
    return new CloudPrintDataSender(mock_helper_.get(),
                                    base::ASCIIToUTF16(kMockJobTitle),
                                    base::ASCIIToUTF16(kMockPrintTicket),
                                    std::string("application/pdf"),
                                    data);
  }

  scoped_refptr<CloudPrintDataSender> print_data_sender_;
  scoped_ptr<MockCloudPrintDataSenderHelper> mock_helper_;

  base::MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_F(CloudPrintDataSenderTest, CanSend) {
  base::StringValue mock_job_title(kMockJobTitle);
  EXPECT_CALL(*mock_helper_,
              CallJavascriptFunction(_, _, StringValueEq(&mock_job_title))).
      WillOnce(Return());

  std::string data("test_data");
  scoped_refptr<CloudPrintDataSender> print_data_sender(
      CreateSender(base::RefCountedString::TakeString(&data)));
  base::FilePath test_data_file_name = GetTestDataFileName();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CloudPrintDataSender::SendPrintData, print_data_sender));
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(CloudPrintDataSenderTest, NoData) {
  EXPECT_CALL(*mock_helper_, CallJavascriptFunction(_, _, _)).Times(0);

  scoped_refptr<CloudPrintDataSender> print_data_sender(CreateSender(NULL));
  base::FilePath test_data_file_name = GetTestDataFileName();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CloudPrintDataSender::SendPrintData, print_data_sender));
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(CloudPrintDataSenderTest, EmptyData) {
  EXPECT_CALL(*mock_helper_, CallJavascriptFunction(_, _, _)).Times(0);

  std::string data;
  scoped_refptr<CloudPrintDataSender> print_data_sender(
      CreateSender(base::RefCountedString::TakeString(&data)));
  base::FilePath test_data_file_name = GetTestDataFileName();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CloudPrintDataSender::SendPrintData, print_data_sender));
  base::MessageLoop::current()->RunUntilIdle();
}

// Testing for CloudPrintFlowHandler needs a mock
// CloudPrintWebDialogDelegate, mock CloudPrintDataSender, and a mock
// WebUI.

// Testing for CloudPrintWebDialogDelegate needs a mock
// CloudPrintFlowHandler.

using internal_cloud_print_helpers::MockCloudPrintFlowHandler;
using internal_cloud_print_helpers::CloudPrintWebDialogDelegate;

class CloudPrintWebDialogDelegateTest : public testing::Test {
 public:
  CloudPrintWebDialogDelegateTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

 protected:
  virtual void SetUp() {
    base::string16 mock_title;
    base::string16 mock_print_ticket;
    std::string mock_file_type;
    MockCloudPrintFlowHandler* handler =
        new MockCloudPrintFlowHandler(mock_print_ticket, mock_title,
                                      mock_file_type);
    mock_flow_handler_ = handler->AsWeakPtr();
    EXPECT_CALL(*mock_flow_handler_.get(), SetDialogDelegate(_));
    EXPECT_CALL(*mock_flow_handler_.get(), SetDialogDelegate(NULL));
    delegate_.reset(new CloudPrintWebDialogDelegate(mock_flow_handler_.get(),
                                                    std::string()));
  }

  virtual void TearDown() {
    delegate_.reset();
    if (mock_flow_handler_.get())
      delete mock_flow_handler_.get();
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::WeakPtr<MockCloudPrintFlowHandler> mock_flow_handler_;
  scoped_ptr<CloudPrintWebDialogDelegate> delegate_;
};

TEST_F(CloudPrintWebDialogDelegateTest, BasicChecks) {
  EXPECT_THAT(delegate_->GetDialogContentURL().spec(),
              StrEq(chrome::kChromeUICloudPrintResourcesURL));
  EXPECT_TRUE(delegate_->GetDialogTitle().empty());

  bool close_dialog = false;
  delegate_->OnCloseContents(NULL, &close_dialog);
  EXPECT_TRUE(close_dialog);
}

TEST_F(CloudPrintWebDialogDelegateTest, OwnedFlowDestroyed) {
  delegate_.reset();
  EXPECT_THAT(mock_flow_handler_.get(), IsNull());
}

TEST_F(CloudPrintWebDialogDelegateTest, UnownedFlowLetGo) {
  std::vector<WebUIMessageHandler*> handlers;
  delegate_->GetWebUIMessageHandlers(&handlers);
  delegate_.reset();
  EXPECT_THAT(mock_flow_handler_.get(), NotNull());
}

// Testing for ExternalWebDialogUI needs a mock WebContents and mock
// CloudPrintWebDialogDelegate (attached to the mock web_contents).

// Testing for PrintDialogCloud needs a mock Browser.
