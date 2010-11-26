// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Content script native API implementation.
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "ceee/ie/plugin/scripting/content_script_manager.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/dispex_mocks.h"
#include "ceee/testing/utils/instance_count_mixin.h"

namespace {

using testing::IDispatchExMockImpl;
using testing::InstanceCountMixin;
using testing::InstanceCountMixinBase;
using testing::_;
using testing::AllOf;
using testing::DispParamArgEq;
using testing::Field;
using testing::Eq;
using testing::Return;
using testing::StrictMock;
using testing::MockDispatchEx;

class IExtensionPortMessagingProviderMockImpl
    : public IExtensionPortMessagingProvider {
 public:
  MOCK_METHOD1(CloseAll, void(IContentScriptNativeApi* instance));
  MOCK_METHOD4(OpenChannelToExtension, HRESULT(
      IContentScriptNativeApi* instance,
      const std::string& extension,
      const std::string& channel_name,
      int cookie));
  MOCK_METHOD2(PostMessage, HRESULT(int port_id, const std::string& message));
};

class MockIExtensionPortMessagingProvider
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockIExtensionPortMessagingProvider>,
      public InstanceCountMixin<MockIExtensionPortMessagingProvider>,
      public StrictMock<IExtensionPortMessagingProviderMockImpl> {
  BEGIN_COM_MAP(MockIExtensionPortMessagingProvider)
  END_COM_MAP()

  HRESULT Initialize() { return S_OK; }
};

class TestingContentScriptNativeApi
    : public ContentScriptNativeApi,
      public InstanceCountMixin<TestingContentScriptNativeApi>,
      public InitializingCoClass<TestingContentScriptNativeApi> {
 public:
  // Disambiguate.
  using InitializingCoClass<TestingContentScriptNativeApi>::CreateInitialized;
  using ContentScriptNativeApi::Initialize;

  HRESULT Initialize(TestingContentScriptNativeApi** self) {
    *self = this;
    return S_OK;
  }
};

class ContentScriptNativeApiTest: public testing::Test {
 public:
  ContentScriptNativeApiTest() : api_(NULL), function_(NULL) {
  }

  void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(
        TestingContentScriptNativeApi::CreateInitialized(&api_, &api_keeper_));
    ASSERT_HRESULT_SUCCEEDED(
        MockDispatchEx::CreateInitialized(&function_, &function_keeper_));
  }

  void TearDown() {
    if (api_ != NULL)
      api_->TearDown();

    api_ = NULL;
    api_keeper_.Release();

    function_ = NULL;
    function_keeper_.Release();

    ASSERT_EQ(0, InstanceCountMixinBase::all_instance_count());
  }

 protected:
  TestingContentScriptNativeApi* api_;
  CComPtr<ICeeeContentScriptNativeApi> api_keeper_;

  MockDispatchEx* function_;
  CComPtr<IDispatch> function_keeper_;
};

TEST_F(ContentScriptNativeApiTest, ImplementsInterfaces) {
  CComPtr<IDispatch> disp;
  ASSERT_HRESULT_SUCCEEDED(
      api_keeper_->QueryInterface(&disp));
}

int kPortId = 42;
const wchar_t* kChannelName = L"Q92FM";
const wchar_t* kTab = NULL;
const wchar_t* kSourceExtensionId = L"fepbkochiplomghbdfgekenppangbiap";
const wchar_t* kTargetExtensionId = L"kgeddobpkdopccblmihponcjlbdpmbod";
const wchar_t* kWideMsg = L"\"JSONified string\"";
const char* kMsg = "\"JSONified string\"";

const wchar_t* kWideMsg2 = L"\"Other JSONified string\"";
const char* kMsg2 = "\"Other JSONified string\"";

TEST_F(ContentScriptNativeApiTest, CallUnsetCallbacks) {
  ASSERT_HRESULT_FAILED(api_->CallOnLoad(kSourceExtensionId));
  ASSERT_HRESULT_FAILED(api_->CallOnUnload());
  ASSERT_HRESULT_FAILED(api_->CallOnPortConnect(kPortId,
                                                kChannelName,
                                                kTab,
                                                kSourceExtensionId,
                                                kTargetExtensionId));
  ASSERT_HRESULT_FAILED(api_->CallOnPortDisconnect(kPortId));
  ASSERT_HRESULT_FAILED(api_->CallOnPortMessage(kWideMsg, kPortId));
}

TEST_F(ContentScriptNativeApiTest, CallOnLoad) {
  ASSERT_HRESULT_FAILED(api_->CallOnLoad(kSourceExtensionId));
  ASSERT_HRESULT_SUCCEEDED(api_->put_onLoad(function_keeper_));
  function_->ExpectInvoke(DISPID_VALUE, kSourceExtensionId);
  ASSERT_HRESULT_SUCCEEDED(api_->CallOnLoad(kSourceExtensionId));
}

TEST_F(ContentScriptNativeApiTest, CallOnUnload) {
  ASSERT_HRESULT_FAILED(api_->CallOnUnload());
  ASSERT_HRESULT_SUCCEEDED(api_->put_onUnload(function_keeper_));
  function_->ExpectInvoke(DISPID_VALUE);
  ASSERT_HRESULT_SUCCEEDED(api_->CallOnUnload());
}

TEST_F(ContentScriptNativeApiTest, CallOnPortConnect) {
  ASSERT_HRESULT_FAILED(api_->CallOnPortConnect(kPortId,
                                                kChannelName,
                                                kTab,
                                                kSourceExtensionId,
                                                kTargetExtensionId));
  ASSERT_HRESULT_SUCCEEDED(api_->put_onPortConnect(function_keeper_));
  function_->ExpectInvoke(DISPID_VALUE,
                          kPortId,
                          kChannelName,
                          kTab,
                          kSourceExtensionId,
                          kTargetExtensionId);
  ASSERT_HRESULT_SUCCEEDED(api_->CallOnPortConnect(kPortId,
                                                   kChannelName,
                                                   kTab,
                                                   kSourceExtensionId,
                                                   kTargetExtensionId));
}

TEST_F(ContentScriptNativeApiTest, CallOnPortDisconnect) {
  ASSERT_HRESULT_FAILED(api_->CallOnPortDisconnect(kPortId));
  ASSERT_HRESULT_SUCCEEDED(api_->put_onPortDisconnect(function_keeper_));
  function_->ExpectInvoke(DISPID_VALUE, kPortId);
  ASSERT_HRESULT_SUCCEEDED(api_->CallOnPortDisconnect(kPortId));
}

TEST_F(ContentScriptNativeApiTest, CallOnPortMessage) {
  ASSERT_HRESULT_FAILED(api_->CallOnPortMessage(kWideMsg, kPortId));
  ASSERT_HRESULT_SUCCEEDED(api_->put_onPortMessage(function_keeper_));
  function_->ExpectInvoke(DISPID_VALUE, kWideMsg, kPortId);
  ASSERT_HRESULT_SUCCEEDED(api_->CallOnPortMessage(kWideMsg, kPortId));
}

TEST_F(ContentScriptNativeApiTest, OnPostMessage) {
  MockIExtensionPortMessagingProvider* mock_provider;
  ASSERT_HRESULT_SUCCEEDED(MockIExtensionPortMessagingProvider::
      CreateInstance(&mock_provider));
  CComPtr<IExtensionPortMessagingProvider> mock_provider_holder(mock_provider);
  EXPECT_HRESULT_SUCCEEDED(api_->Initialize(mock_provider_holder.p));
  // TODO(siggi@chromium.org): Expect the appropriate argument values.
  EXPECT_CALL(*mock_provider, OpenChannelToExtension(api_, _, _, _))
      .WillRepeatedly(Return(S_OK));
  CComBSTR source_id(L"SourceId");
  CComBSTR name(L"name");
  long local_port_id1 = 0;
  EXPECT_HRESULT_SUCCEEDED(api_->OpenChannelToExtension(
      source_id, source_id, name, &local_port_id1));
  long local_port_id2 = 0;
  EXPECT_HRESULT_SUCCEEDED(api_->OpenChannelToExtension(
      source_id, source_id, name, &local_port_id2));

  // TODO(siggi@chromium.org): Test pending messages code.
  static const int kRemotePortId1 = 42;
  static const int kRemotePortId2 = 84;
  api_->OnChannelOpened((int)local_port_id2, kRemotePortId2);
  api_->OnChannelOpened((int)local_port_id1, kRemotePortId1);

  EXPECT_HRESULT_SUCCEEDED(api_->put_onPortMessage(function_keeper_));
  function_->ExpectInvoke(DISPID_VALUE, kWideMsg, local_port_id1);
  api_->OnPostMessage(kRemotePortId1, kMsg);

  function_->ExpectInvoke(DISPID_VALUE, kWideMsg2, local_port_id2);
  api_->OnPostMessage(kRemotePortId2, kMsg2);

  EXPECT_CALL(*mock_provider, CloseAll(api_));
  api_->TearDown();
}

}  // namespace
