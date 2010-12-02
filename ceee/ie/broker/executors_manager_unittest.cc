// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for executors manager.

#include <vector>

#include "base/logging.h"
#include "ceee/ie/broker/common_api_module.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/nt_internals.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

const int kTabWindowId = 99;
const int kTabWindowId2 = 88;
const HWND kTabWindow = reinterpret_cast<HWND>(kTabWindowId + 1);
const HWND kTabWindow2 = reinterpret_cast<HWND>(kTabWindowId2 + 1);
const HWND kFrameWindow = reinterpret_cast<HWND>(kTabWindowId2 + 42);

// We mock the IUnknown interface to make sure we properly AddRef and
// Release the executors in the manager's map.
// TODO(mad@chromium.org): replace this with the MockExecutorIUnknown
// in mock_broker_and_friends.h.
class MockExecutor : public IUnknown {
 public:
  MockExecutor() : ref_count_(0) {}
  STDMETHOD_(ULONG, AddRef)() { return ++ref_count_; }
  STDMETHOD_(ULONG, Release)() {
    EXPECT_GT(ref_count_, 0UL);
    return --ref_count_;
  }
  ULONG ref_count() const { return ref_count_; }
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, QueryInterface,
                             HRESULT(REFIID, void**));
 private:
  ULONG ref_count_;
};

// We also need to mock the creator or executors for 1) making sure it is
// called properly, and 2) for it to return our mock executor.
class MockExecutorCreator: public ICeeeExecutorCreator {
 public:
  // No need to mock AddRef, it is not called.
  STDMETHOD_(ULONG, AddRef)() { return 1; }
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Release, ULONG());
  STDMETHOD (QueryInterface)(REFIID, LPVOID*) { return S_OK; }

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, CreateWindowExecutor,
                             HRESULT(long, CeeeWindowHandle));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Teardown, HRESULT(long));
};

// We need to override some virtual functions of the executors manager and
// also provide public access to some protected methods and data.
class TestingExecutorsManager : public ExecutorsManager {
 public:
  TestingExecutorsManager()
      : ExecutorsManager(true),  // no thread.
        current_executor_(NULL),
        current_handle_(NULL),
        current_thread_id_(kThreadId) {
  }

  ~TestingExecutorsManager() {
    nt_internals::PUBLIC_OBJECT_BASIC_INFORMATION pobi;
    ULONG length = 0;
    for (size_t index = 0; index < opened_handles_.size(); ++index) {
      nt_internals::NtQueryObject(opened_handles_[index].first,
          nt_internals::ObjectBasicInformation, &pobi, sizeof(pobi), &length);
      EXPECT_EQ(length, sizeof(pobi));
      EXPECT_EQ(1UL, pobi.HandleCount);
      if (1UL != pobi.HandleCount) {
        printf("Leaked handle for: %s\n", opened_handles_[index].second);
      }
      EXPECT_TRUE(::CloseHandle(opened_handles_[index].first));
    }
  }

  MOCK_METHOD2(WaitForSingleObject, DWORD(HANDLE, DWORD));
  MOCK_METHOD4(WaitForMultipleObjects,
               DWORD(DWORD, const HANDLE*, BOOL, DWORD));

  HANDLE GetNewHandle(const char* info) {
    // Some tests depend on the handle to be manual reset.
    opened_handles_.push_back(std::pair<HANDLE, const char*>(
        ::CreateEvent(NULL, TRUE, FALSE, NULL), info));
    HANDLE returned_handle = NULL;
    EXPECT_TRUE(::DuplicateHandle(::GetCurrentProcess(),
        opened_handles_.back().first, ::GetCurrentProcess(), &returned_handle,
        0, FALSE, DUPLICATE_SAME_ACCESS));
    return returned_handle;
  }

  // This is a seem in the base class allowing us ot use our own creator.
  HRESULT GetExecutorCreator(ICeeeExecutorCreator** executor_creator) {
    *executor_creator = &executor_creator_;
    return S_OK;
  }
  // provide public access so we can test the method.
  using ExecutorsManager::GetThreadHandles;
  // Wrap the call to ThreadPRoc by first creating the necessary info.
  DWORD CallThreadProc() {
    // This struct is declared in the unnamed namespace of the implementation.
    struct ThreadStartData {
      ExecutorsManager* me;
      CHandle thread_started_gate;
    } thread_start_data;
    thread_start_data.me = this;
    thread_start_data.thread_started_gate.Attach(GetNewHandle("TSD"));
    DCHECK(thread_start_data.thread_started_gate != NULL);
    return ExecutorsManager::ThreadProc(&thread_start_data);
  }

  // Access to protected handles.
  HANDLE GetUpdateHandle() {
    return update_threads_list_gate_.m_h;
  }
  HANDLE GetTerminateHandle() {
    return termination_gate_.m_h;
  }

  // Fake a pending registration, to properly test that case.
  void AddPendingRegistration(HANDLE thread_handle,
                              ThreadId thread_id = kThreadId) {
    pending_registrations_[thread_id].Attach(thread_handle);
  }
  void RemovePendingRegistration(ThreadId thread_id = kThreadId) {
    pending_registrations_.erase(thread_id);
  }

  // This method is to be Invoked by the Mock of WaitForXXXObject[s].
  void RegisterExecutorOnWait() {
    executors_[current_thread_id_] = ExecutorInfo(current_executor_,
                                                  current_handle_);
  }

  // Public access to the executors_ map.
  void FakeRegisterExecutor(HANDLE handle,
                            IUnknown* executor = NULL,
                            ThreadId thread_id = kThreadId) {
    executors_[thread_id] = ExecutorInfo(executor, handle);
  }
  size_t GetNumExecutors() {
    return executors_.size();
  }
  bool IsExecutorRegistered(ThreadId thread_id = kThreadId) {
    return (executors_.find(thread_id) != executors_.end());
  }

  virtual WindowEventsFunnel& windows_events_funnel() {
    return mock_window_events_funnel_;
  }

  IUnknown* current_executor_;
  HANDLE current_handle_;
  ThreadId current_thread_id_;
  StrictMock<MockExecutorCreator> executor_creator_;
  StrictMock<testing::MockWindowEventsFunnel> mock_window_events_funnel_;

  static const ThreadId kThreadId;
  static const HWND kWindowHwnd;
  static const CeeeWindowHandle kWindowHandle;

  // Publicize these protected static const values.
  using ExecutorsManager::kTerminationHandleIndexOffset;
  using ExecutorsManager::kUpdateHandleIndexOffset;

  std::vector<std::pair<HANDLE, const char*>> opened_handles_;
};

const TestingExecutorsManager::ThreadId TestingExecutorsManager::kThreadId = 42;
const HWND TestingExecutorsManager::kWindowHwnd = reinterpret_cast<HWND>(93);
const CeeeWindowHandle TestingExecutorsManager::kWindowHandle
    = reinterpret_cast<CeeeWindowHandle>(kWindowHwnd);

class ExecutorsManagerTests: public testing::Test {
 public:
  ExecutorsManagerTests() : initial_handle_count_(0) {
  }
  // This is not reliable enough, but we keep it here so that we can enable
  // it from time to time to make sure everything is OK.
  /*
  virtual void SetUp() {
    // This is called from the threadproc and changes the handles count if we
    // don't call it first.
    ASSERT_HRESULT_SUCCEEDED(::CoInitializeEx(NULL, COINIT_MULTITHREADED));

    // Acquire the number of handles in the process.
    ::GetProcessHandleCount(::GetCurrentProcess(), &initial_handle_count_);
    printf("Initial Count: %d\n", initial_handle_count_);
  }

  virtual void TearDown() {
    // Make sure the number of handles in the process didn't change.
    DWORD new_handle_count = 0;
    ::GetProcessHandleCount(::GetCurrentProcess(), &new_handle_count);
    EXPECT_EQ(initial_handle_count_, new_handle_count);
    printf("Final Count: %d\n", new_handle_count);

    ::CoUninitialize();
  }
  */
 private:
  DWORD initial_handle_count_;
};

TEST_F(ExecutorsManagerTests, RegisterExecutor) {
  testing::LogDisabler no_dchecks;
  TestingExecutorsManager executors_manager;

  // Invalid arguments.
  EXPECT_EQ(E_INVALIDARG, executors_manager.RegisterWindowExecutor(
      TestingExecutorsManager::kThreadId, NULL));

  MockExecutor executor;
  EXPECT_EQ(E_INVALIDARG, executors_manager.RegisterWindowExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(0, executor.ref_count());

  executors_manager.AddPendingRegistration(
      executors_manager.GetNewHandle("PendingRegistration1"));
  EXPECT_EQ(E_INVALIDARG, executors_manager.RegisterWindowExecutor(
      TestingExecutorsManager::kThreadId, NULL));

  // Dead thread...
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, OpenThread(SYNCHRONIZE, FALSE,
      TestingExecutorsManager::kThreadId)).WillOnce(Return((HANDLE)NULL));
  EXPECT_EQ(S_FALSE, executors_manager.RegisterWindowExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(0, executor.ref_count());

  // Failed, already registered.
  executors_manager.FakeRegisterExecutor(
      executors_manager.GetNewHandle("FakeExec1"), &executor);
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_TRUE(executors_manager.IsExecutorRegistered());
  EXPECT_GT(executor.ref_count(), 0UL);
  ULONG previous_ref_count = executor.ref_count();

  EXPECT_EQ(E_UNEXPECTED, executors_manager.RegisterWindowExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(previous_ref_count, executor.ref_count());

  // Cleanup.
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  EXPECT_EQ(S_FALSE, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
  EXPECT_FALSE(executors_manager.IsExecutorRegistered());
  EXPECT_EQ(0, executor.ref_count());

  // Success!!!
  EXPECT_CALL(kernel32, OpenThread(SYNCHRONIZE, FALSE,
      TestingExecutorsManager::kThreadId)).WillOnce(
          Return(executors_manager.GetNewHandle("OpenThread")));
  EXPECT_EQ(S_OK, executors_manager.RegisterWindowExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_TRUE(executors_manager.IsExecutorRegistered());
  EXPECT_GT(executor.ref_count(), 0UL);

  // Make sure we properly cleanup.
  executors_manager.RemovePendingRegistration();
  EXPECT_EQ(S_OK, executors_manager.Terminate());
  EXPECT_EQ(0, executor.ref_count());
}

TEST_F(ExecutorsManagerTests, RegisterTabExecutor) {
  testing::LogDisabler no_dchecks;
  TestingExecutorsManager executors_manager;

  // Already registered.
  MockExecutor executor;
  HANDLE new_handle = executors_manager.GetNewHandle("FakeExec1");
  executors_manager.FakeRegisterExecutor(new_handle, &executor);
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_TRUE(executors_manager.IsExecutorRegistered());
  EXPECT_GT(executor.ref_count(), 0UL);
  ULONG previous_ref_count = executor.ref_count();

  EXPECT_EQ(S_OK, executors_manager.RegisterTabExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(previous_ref_count, executor.ref_count());

  // Cleanup.
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  EXPECT_EQ(S_FALSE, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
  EXPECT_FALSE(executors_manager.IsExecutorRegistered());
  EXPECT_EQ(0, executor.ref_count());

  // Dead thread...
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, OpenThread(SYNCHRONIZE, FALSE,
      TestingExecutorsManager::kThreadId)).WillOnce(Return((HANDLE)NULL));
  EXPECT_EQ(E_UNEXPECTED, executors_manager.RegisterTabExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(0, executor.ref_count());

  // Success!!!
  new_handle = executors_manager.GetNewHandle("OpenThread");
  EXPECT_CALL(kernel32, OpenThread(SYNCHRONIZE, FALSE,
      TestingExecutorsManager::kThreadId)).WillOnce(Return(new_handle));
  EXPECT_EQ(S_OK, executors_manager.RegisterTabExecutor(
      TestingExecutorsManager::kThreadId, &executor));
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_TRUE(executors_manager.IsExecutorRegistered());
  EXPECT_GT(executor.ref_count(), 0UL);

  // Make sure we properly cleanup.
  EXPECT_EQ(S_OK, executors_manager.Terminate());
  EXPECT_EQ(0, executor.ref_count());
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
}

// Mock static functions defined in CommonApiResult.
MOCK_STATIC_CLASS_BEGIN(MockCommonApiResultStatics)
MOCK_STATIC_INIT_BEGIN(MockCommonApiResultStatics)
MOCK_STATIC_INIT2(common_api::CommonApiResult::IsIeFrameClass,
                  IsIeFrameClass);
MOCK_STATIC_INIT_END()
MOCK_STATIC1(bool, , IsIeFrameClass, HWND);
MOCK_STATIC_CLASS_END(MockCommonApiResultStatics)


TEST_F(ExecutorsManagerTests, GetExecutor) {
  testing::LogDisabler no_dchecks;
  TestingExecutorsManager executors_manager;

  // We now test that the window is a valid frame before attempting to
  // inject an executor in the thread.
  StrictMock<MockCommonApiResultStatics> mock_common;
  EXPECT_CALL(
      mock_common, IsIeFrameClass(TestingExecutorsManager::kWindowHwnd)).
          WillRepeatedly(Return(true));

  // Already in the map.
  MockExecutor executor;
  executors_manager.FakeRegisterExecutor(
      executors_manager.GetNewHandle("FakeExec1"), &executor);
  EXPECT_GT(executor.ref_count(), 0UL);
  ULONG previous_ref_count = executor.ref_count();

  IUnknown* result_executor = NULL;
  EXPECT_CALL(executor, QueryInterface(_, _)).WillOnce(
      DoAll(SetArgumentPointee<1>(&executor), Return(S_OK)));
  EXPECT_EQ(S_OK, executors_manager.GetExecutor(
      TestingExecutorsManager::kThreadId, TestingExecutorsManager::kWindowHwnd,
      IID_IUnknown, reinterpret_cast<void**>(&result_executor)));
  EXPECT_EQ(&executor, result_executor);
  // Since our mocked QI doesn't addref, ref count shouldn't have changed.
  EXPECT_EQ(previous_ref_count, executor.ref_count());

  // Cleanup.
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  // Since our mocked QI doesn't addref, we should be back to 0 now.
  EXPECT_EQ(0, executor.ref_count());

  // Fail Executor creation.
  result_executor = NULL;
  EXPECT_CALL(executors_manager.executor_creator_,
      CreateWindowExecutor(TestingExecutorsManager::kThreadId,
                           TestingExecutorsManager::kWindowHandle)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(executors_manager.executor_creator_, Release()).
      WillOnce(Return(1));
  EXPECT_EQ(E_FAIL, executors_manager.GetExecutor(
      TestingExecutorsManager::kThreadId, TestingExecutorsManager::kWindowHwnd,
      IID_IUnknown, reinterpret_cast<void**>(&result_executor)));
  EXPECT_EQ(NULL, result_executor);

  // Fail registration.
  EXPECT_CALL(executors_manager, WaitForSingleObject(_, _)).
      WillOnce(Return(WAIT_OBJECT_0));
  EXPECT_CALL(executors_manager.executor_creator_,
      CreateWindowExecutor(TestingExecutorsManager::kThreadId,
                           TestingExecutorsManager::kWindowHandle)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(executors_manager.executor_creator_,
      Teardown(TestingExecutorsManager::kThreadId)).WillOnce(Return(S_OK));
  EXPECT_CALL(executors_manager.executor_creator_, Release()).
      WillOnce(Return(1));
  EXPECT_EQ(E_UNEXPECTED, executors_manager.GetExecutor(
      TestingExecutorsManager::kThreadId, TestingExecutorsManager::kWindowHwnd,
      IID_IUnknown, reinterpret_cast<void**>(&result_executor)));
  EXPECT_EQ(NULL, result_executor);

  // Pending registration fail.
  EXPECT_CALL(executors_manager, WaitForSingleObject(_, _)).
      WillOnce(Return(WAIT_OBJECT_0));
  executors_manager.AddPendingRegistration(
      executors_manager.GetNewHandle("PendingReg1"));

  EXPECT_EQ(E_UNEXPECTED, executors_manager.GetExecutor(
      TestingExecutorsManager::kThreadId, TestingExecutorsManager::kWindowHwnd,
      IID_IUnknown, reinterpret_cast<void**>(&result_executor)));
  EXPECT_EQ(NULL, result_executor);

  // Success Creating new.
  executors_manager.current_executor_ = &executor;
  EXPECT_CALL(executors_manager, WaitForSingleObject(_, _)).
      WillOnce(DoAll(InvokeWithoutArgs(&executors_manager,
          &TestingExecutorsManager::RegisterExecutorOnWait),
          Return(WAIT_OBJECT_0)));
  EXPECT_CALL(executors_manager.executor_creator_,
      CreateWindowExecutor(TestingExecutorsManager::kThreadId,
                           TestingExecutorsManager::kWindowHandle)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(executors_manager.executor_creator_,
      Teardown(TestingExecutorsManager::kThreadId)).WillOnce(Return(S_OK));
  EXPECT_CALL(executors_manager.executor_creator_, Release()).
      WillOnce(Return(1));
  EXPECT_CALL(executor, QueryInterface(_, _)).WillOnce(
      DoAll(SetArgumentPointee<1>(&executor), Return(S_OK)));
  EXPECT_EQ(S_OK, executors_manager.GetExecutor(
      TestingExecutorsManager::kThreadId, TestingExecutorsManager::kWindowHwnd,
      IID_IUnknown, reinterpret_cast<void**>(&result_executor)));
  EXPECT_EQ(&executor, result_executor);
  EXPECT_GT(executor.ref_count(), 0UL);

  // Cleanup.
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  // Since our mocked QI doesn't addref, we should be back to 0 now.
  EXPECT_EQ(0, executor.ref_count());

  // Success with pending registration.
  EXPECT_CALL(executors_manager, WaitForSingleObject(_, _)).
      WillOnce(DoAll(InvokeWithoutArgs(&executors_manager,
          &TestingExecutorsManager::RegisterExecutorOnWait),
          Return(WAIT_OBJECT_0)));
  executors_manager.AddPendingRegistration(
      executors_manager.GetNewHandle("PendingReg2"));
  EXPECT_CALL(executor, QueryInterface(_, _)).WillOnce(
      DoAll(SetArgumentPointee<1>(&executor), Return(S_OK)));
  EXPECT_EQ(S_OK, executors_manager.GetExecutor(
      TestingExecutorsManager::kThreadId, TestingExecutorsManager::kWindowHwnd,
      IID_IUnknown, reinterpret_cast<void**>(&result_executor)));
  EXPECT_EQ(&executor, result_executor);
  EXPECT_GT(executor.ref_count(), 0UL);

  // Make sure we properly cleanup.
  EXPECT_EQ(S_OK, executors_manager.Terminate());
  // Since our mocked QI doesn't addref, we should be back to 0 now.
  EXPECT_EQ(0, executor.ref_count());
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
}

TEST_F(ExecutorsManagerTests, RemoveExecutor) {
  TestingExecutorsManager executors_manager;

  // Nothing to remove...
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
  EXPECT_EQ(S_FALSE, executors_manager.RemoveExecutor(1));
  EXPECT_EQ(0, executors_manager.GetNumExecutors());

  // Success.
  MockExecutor executor1;
  executors_manager.FakeRegisterExecutor(
      executors_manager.GetNewHandle("Fake0"), &executor1);
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_GT(executor1.ref_count(), 0UL);
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
  EXPECT_EQ(0, executor1.ref_count());
  EXPECT_FALSE(executors_manager.IsExecutorRegistered());
  EXPECT_EQ(S_FALSE, executors_manager.RemoveExecutor(
      TestingExecutorsManager::kThreadId));

  // Multiple values, removed one at a time...
  MockExecutor executor2;
  MockExecutor executor3;
  executors_manager.FakeRegisterExecutor(
      executors_manager.GetNewHandle("Fake1"), &executor1, 1);
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_GT(executor1.ref_count(), 0UL);
  executors_manager.FakeRegisterExecutor(
      executors_manager.GetNewHandle("Fake2"), &executor2, 2);
  EXPECT_EQ(2, executors_manager.GetNumExecutors());
  EXPECT_GT(executor2.ref_count(), 0UL);
  executors_manager.FakeRegisterExecutor(
      executors_manager.GetNewHandle("Fake3"), &executor3, 3);
  EXPECT_EQ(3, executors_manager.GetNumExecutors());
  EXPECT_GT(executor3.ref_count(), 0UL);
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(2));
  EXPECT_EQ(2, executors_manager.GetNumExecutors());
  EXPECT_FALSE(executors_manager.IsExecutorRegistered(2));
  EXPECT_TRUE(executors_manager.IsExecutorRegistered(1));
  EXPECT_TRUE(executors_manager.IsExecutorRegistered(3));
  EXPECT_GT(executor1.ref_count(), 0UL);
  EXPECT_EQ(0, executor2.ref_count());
  EXPECT_GT(executor3.ref_count(), 0UL);

  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(3));
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_FALSE(executors_manager.IsExecutorRegistered(3));
  EXPECT_TRUE(executors_manager.IsExecutorRegistered(1));
  EXPECT_GT(executor1.ref_count(), 0UL);
  EXPECT_EQ(0, executor3.ref_count());

  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(1));
  EXPECT_EQ(0, executors_manager.GetNumExecutors());
  EXPECT_FALSE(executors_manager.IsExecutorRegistered(1));
  EXPECT_EQ(0, executor1.ref_count());
}

// Since the returned handle is a duplicate, making sure we have the
// appropriate handle is a little tricky.
void VerifyIsSameHandle(HANDLE handle1, HANDLE handle2) {
  // First make sure neither is set.
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(handle1, 0));
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(handle2, 0));

  // Now check that setting one also sets the other.
  ::SetEvent(handle1);
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(handle1, 0));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(handle2, 0));

  // Manual reset.
  ::ResetEvent(handle1);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(handle1, 0));
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(handle2, 0));
}

TEST_F(ExecutorsManagerTests, GetThreadHandles) {
  TestingExecutorsManager executors_manager;

  // We need to work with invalid, and one other valid value for thread info.
  static const TestingExecutorsManager::ThreadId kInvalidThreadId = 0xF0F0F0F0;
  static const TestingExecutorsManager::ThreadId kThreadIdA = 0xAAAAAAAA;
  static const TestingExecutorsManager::ThreadId kThreadIdB = 0xBBBBBBBB;

  // As for the thread proc code, we work with arrays of handles and thread ids
  // on the stack.
  CHandle thread_handles[MAXIMUM_WAIT_OBJECTS];
  TestingExecutorsManager::ThreadId thread_ids[MAXIMUM_WAIT_OBJECTS];

  // Make sure the arrays are not touched.
  thread_ids[0] = kInvalidThreadId;
  size_t num_threads = executors_manager.GetThreadHandles(
      thread_handles, thread_ids, MAXIMUM_WAIT_OBJECTS);
  EXPECT_EQ(0, num_threads);
  EXPECT_EQ(static_cast<HANDLE>(NULL), thread_handles[0]);
  EXPECT_EQ(kInvalidThreadId, thread_ids[0]);

  // Make sure only the first index will get affected.
  thread_ids[1] = kInvalidThreadId;
  HANDLE thread_handle_a = executors_manager.GetNewHandle("A");
  executors_manager.FakeRegisterExecutor(thread_handle_a, NULL, kThreadIdA);
  num_threads = executors_manager.GetThreadHandles(thread_handles, thread_ids,
                                                   MAXIMUM_WAIT_OBJECTS);
  EXPECT_EQ(1, num_threads);
  VerifyIsSameHandle(thread_handle_a, thread_handles[0]);
  EXPECT_EQ(kThreadIdA, thread_ids[0]);
  EXPECT_EQ(static_cast<HANDLE>(NULL), thread_handles[1]);
  EXPECT_EQ(kInvalidThreadId, thread_ids[1]);
  // Need to close active handles since GetThreadHandles expect NULL m_h.
  thread_handles[0].Close();

  // Now expect 2 values and make sure the third one isn't affected.
  thread_ids[0] = kInvalidThreadId;
  thread_ids[2] = kInvalidThreadId;  // We asserted above that index 1 is OK.
  HANDLE thread_handle_b = executors_manager.GetNewHandle("B");
  executors_manager.FakeRegisterExecutor(thread_handle_b, NULL, kThreadIdB);
  num_threads = executors_manager.GetThreadHandles(thread_handles, thread_ids,
                                                   MAXIMUM_WAIT_OBJECTS);
  EXPECT_EQ(2, num_threads);
  // We can't be sure of the order in which the map returns the elements.
  if (thread_ids[0] == kThreadIdA) {
    VerifyIsSameHandle(thread_handle_a, thread_handles[0]);
    EXPECT_EQ(kThreadIdB, thread_ids[1]);
    VerifyIsSameHandle(thread_handle_b, thread_handles[1]);
  } else {
    VerifyIsSameHandle(thread_handle_b, thread_handles[0]);
    EXPECT_EQ(kThreadIdB, thread_ids[0]);
    VerifyIsSameHandle(thread_handle_a, thread_handles[1]);
    EXPECT_EQ(kThreadIdA, thread_ids[1]);
  }
  EXPECT_EQ(static_cast<HANDLE>(NULL), thread_handles[2]);
  EXPECT_EQ(kInvalidThreadId, thread_ids[2]);
  thread_handles[0].Close();
  thread_handles[1].Close();

  // Now remove threads and make sure they won't be returned again.
  thread_ids[0] = kInvalidThreadId;
  thread_ids[1] = kInvalidThreadId;  // Asserted index 2 invalid already.

  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(kThreadIdA));
  num_threads = executors_manager.GetThreadHandles(thread_handles, thread_ids,
                                                   MAXIMUM_WAIT_OBJECTS);
  EXPECT_EQ(1, num_threads);
  VerifyIsSameHandle(thread_handle_b, thread_handles[0]);
  EXPECT_EQ(kThreadIdB, thread_ids[0]);
  EXPECT_EQ(static_cast<HANDLE>(NULL), thread_handles[1]);
  EXPECT_EQ(kInvalidThreadId, thread_ids[1]);
  thread_handles[0].Close();

  // Now remove the last one...
  thread_ids[0] = kInvalidThreadId;
  EXPECT_EQ(S_OK, executors_manager.RemoveExecutor(kThreadIdB));
  num_threads = executors_manager.GetThreadHandles(thread_handles, thread_ids,
                                                   MAXIMUM_WAIT_OBJECTS);
  EXPECT_EQ(0, num_threads);
  EXPECT_EQ(static_cast<HANDLE>(NULL), thread_handles[0]);
  EXPECT_EQ(kInvalidThreadId, thread_ids[0]);
}

TEST_F(ExecutorsManagerTests, ThreadProc) {
  testing::LogDisabler no_logs;
  TestingExecutorsManager executors_manager;

  // Register an object and make sure we wake up to update our list.
  MockExecutor executor1;
  executors_manager.current_executor_ = &executor1;
  executors_manager.current_handle_ = executors_manager.GetNewHandle("1");
  EXPECT_CALL(executors_manager, WaitForMultipleObjects(2, _, FALSE, INFINITE)).
      WillOnce(DoAll(InvokeWithoutArgs(&executors_manager,
          &TestingExecutorsManager::RegisterExecutorOnWait),
          Return(TestingExecutorsManager::kUpdateHandleIndexOffset)));
  EXPECT_CALL(executors_manager, WaitForMultipleObjects(3, _, FALSE, INFINITE)).
      WillOnce(Return(
          TestingExecutorsManager::kTerminationHandleIndexOffset + 1));
  EXPECT_EQ(1, executors_manager.CallThreadProc());

  EXPECT_TRUE(executors_manager.IsExecutorRegistered());
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  EXPECT_GT(executor1.ref_count(), 0UL);

  // Add another destination thread.
  static const TestingExecutorsManager::ThreadId kOtherThreadId = 14;
  MockExecutor executor2;
  executors_manager.current_executor_ = &executor2;
  executors_manager.current_handle_ = executors_manager.GetNewHandle("2");
  executors_manager.current_thread_id_ = kOtherThreadId;
  // Number of handles and return index offset needs to be increased by 1
  // because we already have a destination thread in our map.
  EXPECT_CALL(executors_manager, WaitForMultipleObjects(3, _, FALSE, INFINITE)).
      WillOnce(DoAll(InvokeWithoutArgs(&executors_manager,
          &TestingExecutorsManager::RegisterExecutorOnWait), Return(
          TestingExecutorsManager::kUpdateHandleIndexOffset + 1))).WillOnce(
          Return(TestingExecutorsManager::kTerminationHandleIndexOffset + 1));
  // Now that we have another destination thread in the map,
  // we wait for 4 handles. We return 1 to remove one of them, and wait will
  // be called with three handles again, the mock above will be called for the
  // second time with 3 handles and will return the proper termination offset.
  EXPECT_CALL(executors_manager, WaitForMultipleObjects(4, _, FALSE, INFINITE)).
      WillOnce(Return(1));
  EXPECT_EQ(1, executors_manager.CallThreadProc());
  EXPECT_EQ(1, executors_manager.GetNumExecutors());
  // We can't tell which one is at position 1 in the array returned by
  // GetThreadHandles since they are fetched from an unsorted map.
  EXPECT_TRUE(executors_manager.IsExecutorRegistered(kOtherThreadId) ||
      executors_manager.IsExecutorRegistered(
          TestingExecutorsManager::kThreadId));
  if (executors_manager.IsExecutorRegistered(kOtherThreadId)) {
    // We kept the other executor in the map.
    EXPECT_EQ(0, executor1.ref_count());
    EXPECT_GT(executor2.ref_count(), 0UL);
  } else {
    EXPECT_GT(executor1.ref_count(), 0UL);
    EXPECT_EQ(0, executor2.ref_count());
  }

  // Cleanup.
  executors_manager.Terminate();
  EXPECT_EQ(0, executor1.ref_count());
  EXPECT_EQ(0, executor2.ref_count());
  EXPECT_EQ(0, executors_manager.GetNumExecutors());

  // Test the thread failure path.
  EXPECT_CALL(executors_manager, WaitForMultipleObjects(2, _, FALSE, INFINITE)).
      WillOnce(Return(WAIT_FAILED));
  EXPECT_EQ(1, executors_manager.CallThreadProc());
}

TEST_F(ExecutorsManagerTests, SetTabIdForHandle) {
  testing::LogDisabler no_logs;
  TestingExecutorsManager executors_manager;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, GetTopLevelParent(_)).
      WillRepeatedly(Return(kFrameWindow));
  StrictMock<MockCommonApiResultStatics> mock_common;
  EXPECT_CALL(mock_common, IsIeFrameClass(kFrameWindow)).
      WillRepeatedly(Return(true));

  EXPECT_FALSE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kInvalidChromeSessionId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_FALSE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromId(kTabWindowId));

  // The first time we get a child of the frame window set, we notify.
  EXPECT_CALL(executors_manager.mock_window_events_funnel_,
              OnCreated(kFrameWindow)).Times(1);
  executors_manager.SetTabIdForHandle(kTabWindowId, kTabWindow);
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kTabWindowId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  // Tab IDs or handles can only be mapped once.
  executors_manager.SetTabIdForHandle(kTabWindowId, kTabWindow2);
  EXPECT_FALSE(executors_manager.IsTabHandleValid(kTabWindow2));
  EXPECT_EQ(kInvalidChromeSessionId,
            executors_manager.GetTabIdFromHandle(kTabWindow2));
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kTabWindowId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  executors_manager.SetTabIdForHandle(kTabWindowId2, kTabWindow);
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kTabWindowId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  EXPECT_FALSE(executors_manager.IsTabIdValid(kTabWindowId2));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromId(kTabWindowId2));
  executors_manager.SetTabIdForHandle(kTabWindowId2, kTabWindow2);
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow2));
  EXPECT_EQ(kTabWindowId2,
            executors_manager.GetTabIdFromHandle(kTabWindow2));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId2));
  EXPECT_EQ(kTabWindow2,
            executors_manager.GetTabHandleFromId(kTabWindowId2));
}

TEST_F(ExecutorsManagerTests, SetTabToolBandIdForHandle) {
  testing::LogDisabler no_logs;
  TestingExecutorsManager executors_manager;

  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId));
  executors_manager.SetTabToolBandIdForHandle(kTabWindowId, kTabWindow);
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId));
  // Tool band tab IDs or handles can only be mapped once.
  executors_manager.SetTabToolBandIdForHandle(kTabWindowId, kTabWindow2);
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId));
  executors_manager.SetTabToolBandIdForHandle(kTabWindowId2, kTabWindow);
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId2));
  executors_manager.SetTabToolBandIdForHandle(kTabWindowId2, kTabWindow2);
  EXPECT_EQ(kTabWindow2,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId2));
}

TEST_F(ExecutorsManagerTests, DeleteTabHandle) {
  testing::LogDisabler no_logs;
  TestingExecutorsManager executors_manager;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, GetTopLevelParent(_)).
      WillRepeatedly(Return(kFrameWindow));
  StrictMock<MockCommonApiResultStatics> mock_common;
  // We return FALSE to force a lookup for the parent.
  EXPECT_CALL(mock_common, IsIeFrameClass(kFrameWindow)).
      WillRepeatedly(Return(false));

  // Test deletion of a nonexistent window.
  executors_manager.DeleteTabHandle(kTabWindow);
  EXPECT_FALSE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kInvalidChromeSessionId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_FALSE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  EXPECT_CALL(executors_manager.mock_window_events_funnel_,
              OnCreated(kFrameWindow)).Times(1);
  executors_manager.SetTabIdForHandle(kTabWindowId, kTabWindow);
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kTabWindowId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromId(kTabWindowId));

  // The last time we remove a child of the frame window set, we notify.
  EXPECT_CALL(executors_manager.mock_window_events_funnel_,
              OnRemoved(kFrameWindow)).Times(1);
  executors_manager.DeleteTabHandle(kTabWindow);
  EXPECT_FALSE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kInvalidChromeSessionId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_FALSE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  // Test deletion of a nonexistent window when another one exists.
  EXPECT_CALL(executors_manager.mock_window_events_funnel_,
              OnCreated(kFrameWindow)).Times(1);
  executors_manager.SetTabIdForHandle(kTabWindowId2, kTabWindow2);
  executors_manager.DeleteTabHandle(kTabWindow);
  EXPECT_FALSE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kInvalidChromeSessionId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_FALSE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow2));
  EXPECT_EQ(kTabWindowId2,
            executors_manager.GetTabIdFromHandle(kTabWindow2));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId2));
  EXPECT_EQ(kTabWindow2,
            executors_manager.GetTabHandleFromId(kTabWindowId2));
}

TEST_F(ExecutorsManagerTests, DeleteTabHandleWithToolBandId) {
  testing::LogDisabler no_logs;
  TestingExecutorsManager executors_manager;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, GetTopLevelParent(_)).
      WillRepeatedly(Return(kFrameWindow));
  StrictMock<MockCommonApiResultStatics> mock_common;
  EXPECT_CALL(mock_common, IsIeFrameClass(kFrameWindow)).
      WillRepeatedly(Return(true));

  EXPECT_CALL(executors_manager.mock_window_events_funnel_,
              OnCreated(kFrameWindow)).Times(1);
  executors_manager.SetTabIdForHandle(kTabWindowId, kTabWindow);
  executors_manager.SetTabToolBandIdForHandle(kTabWindowId2, kTabWindow);
  EXPECT_TRUE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kTabWindowId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_TRUE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  EXPECT_EQ(kTabWindow,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId2));
  // The last time we remove a child of the frame window set, we notify.
  EXPECT_CALL(executors_manager.mock_window_events_funnel_,
              OnRemoved(kFrameWindow)).Times(1);
  executors_manager.DeleteTabHandle(kTabWindow);
  EXPECT_FALSE(executors_manager.IsTabHandleValid(kTabWindow));
  EXPECT_EQ(kInvalidChromeSessionId,
            executors_manager.GetTabIdFromHandle(kTabWindow));
  EXPECT_FALSE(executors_manager.IsTabIdValid(kTabWindowId));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromId(kTabWindowId));
  EXPECT_EQ(INVALID_HANDLE_VALUE,
            executors_manager.GetTabHandleFromToolBandId(kTabWindowId2));
}

}  // namespace
