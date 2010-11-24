// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for user broker.

#include "base/logging.h"
#include "ceee/ie/broker/broker.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Return;

// Lets us test protected members and mock certain calls.
class MockExecutorsManager : public ExecutorsManager {
 public:
  // true for no thread.
  MockExecutorsManager() : ExecutorsManager(true) {
  }
  MOCK_METHOD2(RegisterWindowExecutor, HRESULT(ThreadId thread_id,
                                               IUnknown* executor));
  MOCK_METHOD2(RegisterTabExecutor, HRESULT(ThreadId thread_id,
                                            IUnknown* executor));
  MOCK_METHOD1(RemoveExecutor, HRESULT(ThreadId thread_id));

  MOCK_METHOD2(SetTabIdForHandle, void(int tab_id, HWND tab_handle));
  MOCK_METHOD2(SetTabToolBandIdForHandle, void(int tool_band_id,
                                               HWND tab_handle));
};

class TestBroker: public CeeeBroker {
 public:
  // Prevents the instantiation of the ExecutorsManager, ApiDispatcher
  // singletons.
  HRESULT FinalConstruct() {
    return S_OK;
  }
  // Set our own ExecutorsManager mock.
  void SetExecutorsManager(ExecutorsManager* executors_manager) {
    executors_manager_ = executors_manager;
  }

  // Set our own ApiDispatcher mock.
  void SetApiDisptacher(ApiDispatcher* api_dispatcher) {
    api_dispatcher_ = api_dispatcher;
  }
};

TEST(CeeeBroker, All) {
  CComObject<TestBroker>* broker = NULL;
  CComObject<TestBroker>::CreateInstance(&broker);
  CComPtr<TestBroker> broker_keeper(broker);

  MockExecutorsManager executors_manager;
  broker->SetExecutorsManager(&executors_manager);

  testing::MockApiDispatcher api_dispatcher;
  broker->SetApiDisptacher(&api_dispatcher);

  EXPECT_CALL(api_dispatcher, HandleApiRequest(NULL, NULL)).Times(1);
  EXPECT_EQ(S_OK, broker->Execute(NULL, NULL));

  EXPECT_CALL(executors_manager, RegisterWindowExecutor(0, NULL)).
      WillOnce(Return(S_OK));
  EXPECT_EQ(S_OK, broker->RegisterWindowExecutor(0, NULL));

  EXPECT_CALL(executors_manager, RegisterTabExecutor(0, NULL)).
      WillOnce(Return(S_OK));
  EXPECT_EQ(S_OK, broker->RegisterTabExecutor(0, NULL));

  EXPECT_CALL(executors_manager, RemoveExecutor(0)).
      WillOnce(Return(S_OK));
  EXPECT_EQ(S_OK, broker->UnregisterExecutor(0));

  HWND test_handle = reinterpret_cast<HWND>(100);
  EXPECT_CALL(executors_manager, SetTabIdForHandle(99, test_handle));
  EXPECT_EQ(S_OK, broker->SetTabIdForHandle(99, 100));

  EXPECT_CALL(executors_manager, SetTabToolBandIdForHandle(99, test_handle));
  EXPECT_EQ(S_OK, broker->SetTabToolBandIdForHandle(99, 100));
}

}  // namespace
