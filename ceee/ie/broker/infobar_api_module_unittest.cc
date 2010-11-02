// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Window API implementation unit tests.

// MockWin32 can't be included after ChromeFrameHost because of an include
// incompatibility with atlwin.h.
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include "ceee/ie/broker/infobar_api_module.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/browser/extensions/extension_infobar_module_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ext = extension_infobar_module_constants;

namespace {

using infobar_api::InfobarApiResult;
using infobar_api::RegisterInvocations;
using infobar_api::ShowInfoBar;

using testing::_;
using testing::AddRef;
using testing::AtLeast;
using testing::MockApiDispatcher;
using testing::MockApiInvocation;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

const int kGoodTabWindowId = 99;
const int kGoodFrameWindowId = 88;
const int kRequestId = 43;
const char kHtmlPath[] = "/infobar/test.html";

const HWND kGoodTabWindow = (HWND)(kGoodTabWindowId + 1);
const HWND kGoodFrameWindow = (HWND)(kGoodFrameWindowId + 1);

TEST(InfobarApi, RegisterInvocations) {
  StrictMock<MockApiDispatcher> disp;
  EXPECT_CALL(disp, RegisterInvocation(NotNull(), NotNull())).Times(AtLeast(1));
  RegisterInvocations(&disp);
}

class MockInfobarApiResult : public InfobarApiResult {
 public:
  explicit MockInfobarApiResult(int request_id)
      : InfobarApiResult(request_id) {}
  MOCK_METHOD0(PostResult, void());
  MOCK_METHOD1(PostError, void(const std::string&));

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  StrictMock<MockApiDispatcher> mock_api_dispatcher_;
};

class InfobarApiTests: public testing::Test {
 public:
  virtual void SetUp() {
    EXPECT_HRESULT_SUCCEEDED(testing::MockInfobarExecutor::CreateInitialized(
        &mock_infobar_executor_, &mock_infobar_executor_keeper_));
  }

  virtual void TearDown() {
    // Everything should have been relinquished.
    mock_infobar_executor_keeper_.Release();
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }

 protected:
  void AlwaysMockGetInfobarExecutor(MockApiDispatcher* api_dispatcher,
                                    HWND window) {
    // We can't use CopyInterfaceToArgument here because GetExecutor takes a
    // void** argument instead of an interface.
    EXPECT_CALL(*api_dispatcher, GetExecutor(window, _, NotNull())).
        WillRepeatedly(DoAll(
            SetArgumentPointee<2>(mock_infobar_executor_keeper_.p),
            AddRef(mock_infobar_executor_keeper_.p)));
  }

  void MockGetInfobarExecutorOnce(MockApiDispatcher* api_dispatcher,
                                  HWND window) {
    // We can't use CopyInterfaceToArgument here because GetExecutor takes a
    // void** argument instead of an interface.
    EXPECT_CALL(*api_dispatcher, GetExecutor(window, _, NotNull())).
        WillOnce(DoAll(SetArgumentPointee<2>(mock_infobar_executor_keeper_.p),
                       AddRef(mock_infobar_executor_keeper_.p)));
  }

  StrictMock<testing::MockUser32> user32_;
  // The executor classes are already strict from their base class impl.
  testing::MockInfobarExecutor* mock_infobar_executor_;

 private:
  // To control the life span of the tab executor.
  CComPtr<ICeeeInfobarExecutor> mock_infobar_executor_keeper_;
};

TEST_F(InfobarApiTests, ShowInfoBarNoErrors) {
  // TODO(vadimb@google.com): Make the implementation work.
#if 0
  testing::LogDisabler no_dchecks;

  DictionaryValue dict;
  dict.Set(ext::kTabId, Value::CreateIntegerValue(kGoodTabWindowId));
  dict.Set(ext::kHtmlPath, Value::CreateStringValue(std::string(kHtmlPath)));
  ListValue good_args;
  ASSERT_TRUE(good_args.Set(0, dict.DeepCopy()));

  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillRepeatedly(Return(true));

  StrictMock<MockApiInvocation<InfobarApiResult, MockInfobarApiResult,
                               ShowInfoBar> > invocation;
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).
      WillRepeatedly(Return(kGoodTabWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kGoodFrameWindowId)).
      WillRepeatedly(Return(kGoodFrameWindow));
  MockGetInfobarExecutorOnce(&invocation.mock_api_dispatcher_, kGoodTabWindow);
  CComBSTR html_path(kHtmlPath);
  CeeeWindowHandle window_handle =
      reinterpret_cast<CeeeWindowHandle>(kGoodFrameWindow);
  EXPECT_CALL(*mock_infobar_executor_, ShowInfobar(StrEq(html_path.m_str), _)).
      WillOnce(DoAll(SetArgumentPointee<1>(window_handle), Return(S_OK)));

  invocation.AllocateNewResult(kRequestId);

  invocation.Execute(good_args, kRequestId);
#endif
}

}  // namespace
