// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_ui.h"

#include <cstddef>
#include <string>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_test_util.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/profile_mock.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using browser_sync::HasArgsAsList;
using browser_sync::JsArgList;
using browser_sync::JsEventDetails;
using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

// Subclass of SyncInternalsUI to mock out ExecuteJavascript.
class TestSyncInternalsUI : public SyncInternalsUI {
 public:
  explicit TestSyncInternalsUI(TabContents* contents)
      : SyncInternalsUI(contents) {}
  virtual ~TestSyncInternalsUI() {}

  MOCK_METHOD1(ExecuteJavascript, void(const string16&));
};

// Tests with non-NULL ProfileSyncService.
class SyncInternalsUITestWithService : public ChromeRenderViewHostTestHarness {
 protected:
  SyncInternalsUITestWithService() {}

  virtual ~SyncInternalsUITestWithService() {}

  virtual void SetUp() {
    NiceMock<ProfileMock>* profile_mock = new NiceMock<ProfileMock>();
    StrictMock<ProfileSyncServiceMock> profile_sync_service_mock;
    EXPECT_CALL(*profile_mock, GetProfileSyncService())
        .WillOnce(Return(&profile_sync_service_mock));
    browser_context_.reset(profile_mock);

    ChromeRenderViewHostTestHarness::SetUp();

    EXPECT_CALL(profile_sync_service_mock, GetJsController())
        .WillOnce(Return(mock_js_controller_.AsWeakPtr()));

    EXPECT_CALL(mock_js_controller_, AddJsEventHandler(_));

    {
      // Needed by |test_sync_internals_ui_|'s constructor.  The
      // message loop is provided by ChromeRenderViewHostTestHarness.
      BrowserThread ui_thread_(BrowserThread::UI,
                               MessageLoopForUI::current());
      // |test_sync_internals_ui_|'s constructor triggers all the
      // expectations above.
      test_sync_internals_ui_.reset(new TestSyncInternalsUI(contents()));
    }

    Mock::VerifyAndClearExpectations(profile_mock);
    Mock::VerifyAndClearExpectations(&mock_js_controller_);
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(&mock_js_controller_);

    // Called by |test_sync_internals_ui_|'s destructor.
    EXPECT_CALL(mock_js_controller_,
                RemoveJsEventHandler(test_sync_internals_ui_.get()));
    test_sync_internals_ui_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  StrictMock<browser_sync::MockJsController> mock_js_controller_;
  scoped_ptr<TestSyncInternalsUI> test_sync_internals_ui_;
};

TEST_F(SyncInternalsUITestWithService, HandleJsEvent) {
  EXPECT_CALL(*test_sync_internals_ui_,
              ExecuteJavascript(
                  ASCIIToUTF16("chrome.sync.testMessage.fire({});")));

  test_sync_internals_ui_->HandleJsEvent("testMessage", JsEventDetails());
}

TEST_F(SyncInternalsUITestWithService, HandleJsReply) {
  EXPECT_CALL(
      *test_sync_internals_ui_,
      ExecuteJavascript(
          ASCIIToUTF16("chrome.sync.testMessage.handleReply(5,true);")));

  ListValue args;
  args.Append(Value::CreateIntegerValue(5));
  args.Append(Value::CreateBooleanValue(true));
  test_sync_internals_ui_->HandleJsReply("testMessage", JsArgList(&args));
}

TEST_F(SyncInternalsUITestWithService, OnWebUISendBasic) {
  const std::string& name = "testName";
  ListValue args;
  args.Append(Value::CreateIntegerValue(10));

  EXPECT_CALL(mock_js_controller_,
              ProcessJsMessage(name, HasArgsAsList(args), _));

  test_sync_internals_ui_->OnWebUISend(GURL(), name, args);
}

// Tests with NULL ProfileSyncService.
class SyncInternalsUITestWithoutService
    : public ChromeRenderViewHostTestHarness {
 protected:
  SyncInternalsUITestWithoutService() {}

  virtual ~SyncInternalsUITestWithoutService() {}

  virtual void SetUp() {
    NiceMock<ProfileMock>* profile_mock = new NiceMock<ProfileMock>();
    EXPECT_CALL(*profile_mock, GetProfileSyncService())
        .WillOnce(Return(static_cast<ProfileSyncService*>(NULL)));
    browser_context_.reset(profile_mock);

    ChromeRenderViewHostTestHarness::SetUp();

    {
      // Needed by |test_sync_internals_ui_|'s constructor.  The
      // message loop is provided by ChromeRenderViewHostTestHarness.
      BrowserThread ui_thread_(BrowserThread::UI,
                               MessageLoopForUI::current());
      // |test_sync_internals_ui_|'s constructor triggers all the
      // expectations above.
      test_sync_internals_ui_.reset(new TestSyncInternalsUI(contents()));
    }

    Mock::VerifyAndClearExpectations(profile_mock);
  }

  scoped_ptr<TestSyncInternalsUI> test_sync_internals_ui_;
};

TEST_F(SyncInternalsUITestWithoutService, HandleJsEvent) {
  EXPECT_CALL(*test_sync_internals_ui_,
              ExecuteJavascript(
                  ASCIIToUTF16("chrome.sync.testMessage.fire({});")));

  test_sync_internals_ui_->HandleJsEvent("testMessage", JsEventDetails());
}

TEST_F(SyncInternalsUITestWithoutService, HandleJsReply) {
  EXPECT_CALL(
      *test_sync_internals_ui_,
      ExecuteJavascript(
          ASCIIToUTF16("chrome.sync.testMessage.handleReply(5,true);")));

  ListValue args;
  args.Append(Value::CreateIntegerValue(5));
  args.Append(Value::CreateBooleanValue(true));
  test_sync_internals_ui_->HandleJsReply(
      "testMessage", JsArgList(&args));
}

TEST_F(SyncInternalsUITestWithoutService, OnWebUISendBasic) {
  const std::string& name = "testName";
  ListValue args;
  args.Append(Value::CreateIntegerValue(5));

  // Should drop the message.
  test_sync_internals_ui_->OnWebUISend(GURL(), name, args);
}

// TODO(lipalani) - add a test case to test about:sync with a non null
// service.
TEST_F(SyncInternalsUITestWithoutService, OnWebUISendGetAboutInfo) {
  const char kAboutInfoCall[] =
      "chrome.sync.getAboutInfo.handleReply({\"summary\":\"SYNC DISABLED\"});";
  EXPECT_CALL(*test_sync_internals_ui_,
              ExecuteJavascript(ASCIIToUTF16(kAboutInfoCall)));

  ListValue args;
  test_sync_internals_ui_->OnWebUISend(GURL(), "getAboutInfo", args);
}

}  // namespace
