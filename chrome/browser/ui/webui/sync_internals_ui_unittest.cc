// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_ui.h"

#include <cstddef>
#include <string>

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_test_util.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/test/profile_mock.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using browser_sync::HasArgsAsList;
using browser_sync::JsArgList;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

// Subclass of SyncInternalsUI to mock out ExecuteJavascript.
class TestSyncInternalsUI : public SyncInternalsUI {
 public:
  explicit TestSyncInternalsUI(TabContents* contents)
      : SyncInternalsUI(contents) {}
  virtual ~TestSyncInternalsUI() {}

  MOCK_METHOD1(ExecuteJavascript, void(const std::wstring&));
};

class SyncInternalsUITest : public RenderViewHostTestHarness {
 protected:
  // We allocate memory for |sync_internals_ui_| but we don't
  // construct it.  This is because we want to set mock expectations
  // with its address before we construct it, and its constructor
  // calls into our mocks.
  SyncInternalsUITest()
      // The message loop is provided by RenderViewHostTestHarness.
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()),
        test_sync_internals_ui_buf_(NULL),
        test_sync_internals_ui_constructor_called_(false) {}

  virtual void SetUp() {
    test_sync_internals_ui_buf_ = operator new(sizeof(TestSyncInternalsUI));
    test_sync_internals_ui_constructor_called_ = false;
    profile_.reset(new NiceMock<ProfileMock>());
    RenderViewHostTestHarness::SetUp();
  }

  virtual void TearDown() {
    if (test_sync_internals_ui_constructor_called_) {
      GetTestSyncInternalsUI()->~TestSyncInternalsUI();
    }
    operator delete(test_sync_internals_ui_buf_);
    RenderViewHostTestHarness::TearDown();
  }

  NiceMock<ProfileMock>* GetProfileMock() {
    return static_cast<NiceMock<ProfileMock>*>(profile());
  }

  // Set up boilerplate expectations for calls done during
  // SyncInternalUI's construction/destruction.
  void ExpectSetupTeardownCalls() {
    EXPECT_CALL(*GetProfileMock(), GetProfileSyncService())
        .WillRepeatedly(Return(&profile_sync_service_mock_));

    EXPECT_CALL(profile_sync_service_mock_, GetJsFrontend())
        .WillRepeatedly(Return(&mock_js_backend_));

    // Called by sync_ui_util::ConstructAboutInformation().
    EXPECT_CALL(profile_sync_service_mock_, HasSyncSetupCompleted())
        .WillRepeatedly(Return(false));

    // Called by SyncInternalsUI's constructor.
    EXPECT_CALL(mock_js_backend_,
                AddHandler(GetTestSyncInternalsUIAddress()));

    // Called by SyncInternalUI's destructor.
    EXPECT_CALL(mock_js_backend_,
                RemoveHandler(GetTestSyncInternalsUIAddress()));
  }

  // Like ExpectSetupTeardownCalls() but with a NULL
  // ProfileSyncService.
  void ExpectSetupTeardownCallsNullService() {
    EXPECT_CALL(*GetProfileMock(), GetProfileSyncService())
        .WillRepeatedly(Return(static_cast<ProfileSyncService*>(NULL)));
  }

  void ConstructTestSyncInternalsUI() {
    if (test_sync_internals_ui_constructor_called_) {
      ADD_FAILURE() << "ConstructTestSyncInternalsUI() should be called "
                    << "at most once per test";
      return;
    }
    new(test_sync_internals_ui_buf_) TestSyncInternalsUI(contents());
    test_sync_internals_ui_constructor_called_ = true;
  }

  TestSyncInternalsUI* GetTestSyncInternalsUI() {
    if (!test_sync_internals_ui_constructor_called_) {
      ADD_FAILURE() << "ConstructTestSyncInternalsUI() should be called "
                    << "before GetTestSyncInternalsUI()";
      return NULL;
    }
    return GetTestSyncInternalsUIAddress();
  }

  // Used for passing into EXPECT_CALL().
  TestSyncInternalsUI* GetTestSyncInternalsUIAddress() {
    EXPECT_TRUE(test_sync_internals_ui_buf_);
    return static_cast<TestSyncInternalsUI*>(test_sync_internals_ui_buf_);
  }

  StrictMock<ProfileSyncServiceMock> profile_sync_service_mock_;
  StrictMock<browser_sync::MockJsFrontend> mock_js_backend_;

 private:
  // Needed by |contents()|.
  BrowserThread ui_thread_;
  void* test_sync_internals_ui_buf_;
  bool test_sync_internals_ui_constructor_called_;
};

TEST_F(SyncInternalsUITest, HandleJsEvent) {
  ExpectSetupTeardownCalls();

  ConstructTestSyncInternalsUI();

  EXPECT_CALL(*GetTestSyncInternalsUI(),
              ExecuteJavascript(std::wstring(L"testMessage(5,true);")));

  ListValue args;
  args.Append(Value::CreateIntegerValue(5));
  args.Append(Value::CreateBooleanValue(true));
  GetTestSyncInternalsUI()->HandleJsEvent("testMessage", JsArgList(args));
}

TEST_F(SyncInternalsUITest, HandleJsEventNullService) {
  ExpectSetupTeardownCallsNullService();

  ConstructTestSyncInternalsUI();

  EXPECT_CALL(*GetTestSyncInternalsUI(),
              ExecuteJavascript(std::wstring(L"testMessage(5,true);")));

  ListValue args;
  args.Append(Value::CreateIntegerValue(5));
  args.Append(Value::CreateBooleanValue(true));
  GetTestSyncInternalsUI()->HandleJsEvent("testMessage", JsArgList(args));
}

TEST_F(SyncInternalsUITest, ProcessWebUIMessageBasic) {
  ExpectSetupTeardownCalls();

  ViewHostMsg_DomMessage_Params params;
  params.name = "testName";
  params.arguments.Append(Value::CreateIntegerValue(10));

  EXPECT_CALL(mock_js_backend_,
              ProcessMessage(params.name, HasArgsAsList(params.arguments),
                             GetTestSyncInternalsUIAddress()));

  ConstructTestSyncInternalsUI();

  GetTestSyncInternalsUI()->ProcessWebUIMessage(params);
}

TEST_F(SyncInternalsUITest, ProcessWebUIMessageBasicNullService) {
  ExpectSetupTeardownCallsNullService();

  ConstructTestSyncInternalsUI();

  ViewHostMsg_DomMessage_Params params;
  params.name = "testName";
  params.arguments.Append(Value::CreateIntegerValue(5));

  // Should drop the message.
  GetTestSyncInternalsUI()->ProcessWebUIMessage(params);
}

namespace {
const wchar_t kAboutInfoCall[] =
    L"onGetAboutInfoFinished({\"summary\":\"SYNC DISABLED\"});";
}  // namespace

TEST_F(SyncInternalsUITest, ProcessWebUIMessageGetAboutInfo) {
  ExpectSetupTeardownCalls();

  ViewHostMsg_DomMessage_Params params;
  params.name = "getAboutInfo";

  ConstructTestSyncInternalsUI();

  EXPECT_CALL(*GetTestSyncInternalsUI(),
              ExecuteJavascript(std::wstring(kAboutInfoCall)));

  GetTestSyncInternalsUI()->ProcessWebUIMessage(params);
}

TEST_F(SyncInternalsUITest, ProcessWebUIMessageGetAboutInfoNullService) {
  ExpectSetupTeardownCallsNullService();

  ViewHostMsg_DomMessage_Params params;
  params.name = "getAboutInfo";

  ConstructTestSyncInternalsUI();

  EXPECT_CALL(*GetTestSyncInternalsUI(),
              ExecuteJavascript(std::wstring(kAboutInfoCall)));

  GetTestSyncInternalsUI()->ProcessWebUIMessage(params);
}

}  // namespace
