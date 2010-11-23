// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Executor COM implementation unit tests.

#include "ceee/ie/plugin/bho/executor.h"

#include <shlobj.h>
#include <atlbase.h>
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/plugin/toolband/toolband_proxy.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InSequence;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using testing::StrEq;
using testing::StrictMock;
using testing::InstanceCountMixin;
using testing::InstanceCountMixinBase;

class ISynchronizeMock: public ISynchronize {
 public:
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, Wait,
      HRESULT(DWORD flags, DWORD milliseconds));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Signal, HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Reset, HRESULT());
};

class TestISynchronize
    : public CComObjectRootEx<CComMultiThreadModel>,
      public InitializingCoClass<TestISynchronize>,
      public InstanceCountMixin<TestISynchronize>,
      public StrictMock<ISynchronizeMock> {
 public:
  DECLARE_GET_CONTROLLING_UNKNOWN()
  BEGIN_COM_MAP(TestISynchronize)
    COM_INTERFACE_ENTRY(ISynchronize)
    COM_INTERFACE_ENTRY_AGGREGATE_BLIND(async_call_.p)
  END_COM_MAP()

  TestISynchronize() {
  }

  ~TestISynchronize() {
  }

  HRESULT Initialize(TestISynchronize** self) {
    *self = this;
    return S_OK;
  }

  void set_async_call(IUnknown* async_call) { async_call_ = async_call; }

 private:
  // This is the async call instance we aggregate.
  CComPtr<IUnknown> async_call_;
};

class MockCeeeExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockCeeeExecutor>,
      public InstanceCountMixin<MockCeeeExecutor>,
      public StrictMock<testing::MockCeeeWindowExecutorImpl>,
      public StrictMock<testing::MockCeeeTabExecutorImpl>,
      public StrictMock<testing::MockCeeeCookieExecutorImpl>,
      public StrictMock<testing::MockCeeeInfobarExecutorImpl> {
 public:
  BEGIN_COM_MAP(MockCeeeExecutor)
    COM_INTERFACE_ENTRY(ICeeeWindowExecutor)
    COM_INTERFACE_ENTRY(ICeeeTabExecutor)
    COM_INTERFACE_ENTRY(ICeeeCookieExecutor)
    COM_INTERFACE_ENTRY(ICeeeInfobarExecutor)
  END_COM_MAP()

  HRESULT Initialize(MockCeeeExecutor** self) {
    *self = this;
    return S_OK;
  }
};

class TestAsyncTabCall
    : public AsyncTabCall,
      public InitializingCoClass<TestAsyncTabCall>,
      public InstanceCountMixin<TestAsyncTabCall> {
 public:
};

// A test subclass of CeeeExecutor to allow testing
// the async behavior of ICeeeTabExecutor.
class TestCeeeExecutor
    : public CeeeExecutor,
      public InitializingCoClass<TestCeeeExecutor>,
      public InstanceCountMixin<TestCeeeExecutor> {
 public:
  HRESULT Initialize(TestCeeeExecutor** self) {
    *self = this;
    return S_OK;
  }

  // Mock out the ICeeeTabExecutor interface implementation.
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Initialize,
      HRESULT(CeeeWindowHandle hwnd));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetTabInfo,
      HRESULT(CeeeTabInfo* tab_info));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, Navigate,
      HRESULT(BSTR url, long flags, BSTR target));
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, InsertCode,
      HRESULT(BSTR code, BSTR file, BOOL all_frames, CeeeTabCodeType type));

  MOCK_METHOD4(CreateTabCall, HRESULT(ICeeeTabExecutor* executor,
                                      IUnknown *outer,
                                      REFIID riid2,
                                      IUnknown **out));
};

class RemoteObjectHost;
DISABLE_RUNNABLE_METHOD_REFCOUNT(RemoteObjectHost);

class RemoteObjectHost {
 public:
  RemoteObjectHost() : remote_thread_("RemoteObjectHost") {
  }

  ~RemoteObjectHost() {
    remote_git_ptr_.Revoke();
    remote_object_.Release();
  }

  void HostObject(IUnknown* object) {
    // Spin up our thread and initialize it.
    ASSERT_TRUE(remote_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_UI, 0)));

    // Marshal the object in the thread.
    RunSync(
        NewRunnableMethod(this, &RemoteObjectHost::InitRemoteThread, object));

    // Marshal the contained object back as an IUnknown.
    ASSERT_HRESULT_SUCCEEDED(remote_git_ptr_.CopyTo(&remote_object_));
  }

  void RunSync(Task* task) {
    ScopedHandle event(::CreateEvent(NULL, TRUE, FALSE, NULL));

    remote_thread_.message_loop()->PostTask(FROM_HERE, task);
    remote_thread_.message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(::SetEvent, event.Get()));

    ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(event.Get(), INFINITE));
  }

  void InitRemoteThread(IUnknown* object) {
    EXPECT_HRESULT_SUCCEEDED(::CoInitialize(NULL));
    EXPECT_HRESULT_SUCCEEDED(remote_git_ptr_.Attach(object));
  }

 public:
  // The git pointer is created in the remote_thread_.
  CComGITPtr<IUnknown> remote_git_ptr_;
  // And this is a local proxy to the remote object.
  CComPtr<IUnknown> remote_object_;
  base::Thread remote_thread_;
};

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

static const HRESULT E_CAFEBABE = 0xCAFEBABE;

class ExecutorComTest: public testing::Test {
 public:
  ExecutorComTest()
      : executor_(NULL),
        synchronize_(NULL),
        loop_(MessageLoop::TYPE_UI) {
  }

  static bool ImplementsThreadSafeReferenceCounting() {
    return true;
  }

  virtual void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(::CoInitialize(NULL));

    ASSERT_HRESULT_SUCCEEDED(TestCeeeExecutor::CreateInitialized(
        &executor_, &executor_keeper_));
    ASSERT_HRESULT_SUCCEEDED(TestISynchronize::CreateInitialized(
        &synchronize_, &synchronize_keeper_));
  }

  virtual void TearDown() {
    executor_ = NULL;
    executor_keeper_.Release();

    synchronize_->set_async_call(NULL);
    synchronize_ = NULL;
    synchronize_keeper_.Release();

    UnregisterProxies();

    // We want everything unregistered before we CoUninitialize.
    ::CoUninitialize();

    EXPECT_EQ(0, InstanceCountMixinBase::all_instance_count());
  }

  void RegisterProxies() {
    ASSERT_TRUE(RegisterProxyStubs(&proxy_cookies_));
  }

  void UnregisterProxies() {
    if (!proxy_cookies_.empty()) {
      UnregisterProxyStubs(proxy_cookies_);
      proxy_cookies_.clear();
    }
  }

  static void CreateTestTabCallImpl(ICeeeTabExecutor* executor,
                                    IUnknown *outer,
                                    REFIID riid2,
                                    IUnknown **out) {
    CComPolyObject<TestAsyncTabCall>* async_tab_call = NULL;
    ASSERT_HRESULT_SUCCEEDED(
        CComPolyObject<TestAsyncTabCall>::CreateInstance(
            outer, &async_tab_call));
    ASSERT_TRUE(async_tab_call != NULL);
    ASSERT_HRESULT_SUCCEEDED(async_tab_call->m_contained.Initialize(executor));
    ASSERT_HRESULT_SUCCEEDED(
        async_tab_call->QueryInterface(riid2, reinterpret_cast<void**>(out)));
  }

  void ExpectCreateTabCall(TestCeeeExecutor* executor) {
    EXPECT_CALL(*executor, CreateTabCall(executor, _, _, _))
        .WillOnce(
            DoAll(
                Invoke(&ExecutorComTest::CreateTestTabCallImpl),
                Return(S_OK)));
  }

  void CreateTabExecutor(AsyncICeeeTabExecutor** executor_call) {
    // Retrieve the call factory.
    CComPtr<ICallFactory> call_factory;
    ASSERT_HRESULT_SUCCEEDED(executor_keeper_->QueryInterface(&call_factory));
    ASSERT_TRUE(call_factory != NULL);

    // Create the call object.
    ASSERT_NO_FATAL_FAILURE(ExpectCreateTabCall(executor_));
    CComPtr<IUnknown> async_unknown;
    ASSERT_HRESULT_SUCCEEDED(
        call_factory->CreateCall(IID_AsyncICeeeTabExecutor,
                                 synchronize_->GetControllingUnknown(),
                                 IID_IUnknown,
                                 &async_unknown));
    synchronize_->set_async_call(async_unknown);
    async_unknown.Release();

    ASSERT_HRESULT_SUCCEEDED(
        synchronize_keeper_->QueryInterface(executor_call));
  }

  void RunShortMessageLoop() {
    // Set up the expectations we have for invocations
    // during the brief message loop.
    EXPECT_CALL(*synchronize_, Signal())
        .WillOnce(
            DoAll(
                QuitMessageLoop(&loop_),
                Return(S_OK)));

    // And run the loop.
    loop_.Run();
  }

 protected:
  // This test executor is created on setup.
  TestCeeeExecutor* executor_;
  CComPtr<ICeeeTabExecutor> executor_keeper_;

  TestISynchronize* synchronize_;
  CComPtr<ISynchronize> synchronize_keeper_;

  MessageLoop loop_;

  std::vector<DWORD> proxy_cookies_;
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(ExecutorComTest);

static const wchar_t* kUrl = L"http://www.google.com/";
static long kFlags = 74565;
static const wchar_t* kTarget = L"_blank";

TEST_F(ExecutorComTest, MarshalingFailsWithNoProxy) {
  RemoteObjectHost host;
  ASSERT_NO_FATAL_FAILURE(host.HostObject(executor_keeper_));

  // We expect failure, unmarshaling shouldn't be possible
  // without the proxies registered in our (source) thread.
  CComPtr<ICeeeTabExecutor> executor;
  ASSERT_HRESULT_FAILED(host.remote_object_.QueryInterface(&executor));
}

TEST_F(ExecutorComTest, MarshalingFailsWithNoLocalProxy) {
  RemoteObjectHost host;
  ASSERT_NO_FATAL_FAILURE(host.HostObject(executor_keeper_));

  std::vector<DWORD> invoker_cookies;
  host.RunSync(NewRunnableFunction(RegisterProxyStubs, &invoker_cookies));

  // We expect failure, unmarshaling shouldn't be possible
  // without the proxies registered in our (source) thread.
  CComPtr<ICeeeTabExecutor> executor;
  ASSERT_HRESULT_FAILED(host.remote_object_.QueryInterface(&executor));
}

TEST_F(ExecutorComTest, MarshalingSucceedsWithProxiesRegistered) {
  RemoteObjectHost host;
  ASSERT_NO_FATAL_FAILURE(host.HostObject(executor_keeper_));

  std::vector<DWORD> invoker_cookies;
  host.RunSync(NewRunnableFunction(RegisterProxyStubs, &invoker_cookies));

  // Register proxies in our thread.
  RegisterProxies();

  // We expect success as both home and away proxies
  // are now registered.
  CComPtr<ICeeeTabExecutor> executor;
  ASSERT_HRESULT_SUCCEEDED(host.remote_object_.QueryInterface(&executor));
}

TEST_F(ExecutorComTest, AllProxiesAreRegistered) {
  // Create the mock executor.
  CComPtr<IUnknown> mock_keeper;
  MockCeeeExecutor* mock_executor = NULL;
  ASSERT_HRESULT_SUCCEEDED(
      MockCeeeExecutor::CreateInitialized(&mock_executor, &mock_keeper));

  RemoteObjectHost host;
  ASSERT_NO_FATAL_FAILURE(host.HostObject(mock_keeper));

  // Register proxies on both sides.
  std::vector<DWORD> invoker_cookies;
  host.RunSync(NewRunnableFunction(RegisterProxyStubs, &invoker_cookies));
  RegisterProxies();

  CComPtr<ICeeeWindowExecutor> window_executor;
  EXPECT_HRESULT_SUCCEEDED(
      host.remote_object_.QueryInterface(&window_executor));

  CComPtr<ICeeeTabExecutor> tab_executor;
  EXPECT_HRESULT_SUCCEEDED(
      host.remote_object_.QueryInterface(&tab_executor));

  CComPtr<ICeeeCookieExecutor> cookie_executor;
  EXPECT_HRESULT_SUCCEEDED(
      host.remote_object_.QueryInterface(&cookie_executor));

  CComPtr<ICeeeInfobarExecutor> infobar_executor;
  EXPECT_HRESULT_SUCCEEDED(
      host.remote_object_.QueryInterface(&infobar_executor));
}

TEST_F(ExecutorComTest, UnregisterProxiesWorks) {
  RemoteObjectHost host;
  ASSERT_NO_FATAL_FAILURE(host.HostObject(executor_keeper_));

  std::vector<DWORD> invoker_cookies;
  host.RunSync(NewRunnableFunction(RegisterProxyStubs, &invoker_cookies));

  // Register proxies in our thread.
  RegisterProxies();

  // We expect success as both home and away proxies
  // are now registered.
  CComPtr<ICeeeTabExecutor> executor;
  ASSERT_HRESULT_SUCCEEDED(host.remote_object_.QueryInterface(&executor));
  executor.Release();

  // Now unregister the proxies on our side and try again.
  // We need to release the local proxy, as it'll cache any interfaces
  // we've already queried.
  UnregisterProxies();
  host.remote_object_.Release();
  ASSERT_HRESULT_SUCCEEDED(host.remote_git_ptr_.CopyTo(&host.remote_object_));
  ASSERT_HRESULT_FAILED(host.remote_object_.QueryInterface(&executor));
}

TEST_F(ExecutorComTest, ImplementsCallFactory) {
  CComPtr<ICallFactory> call_factory;
  ASSERT_HRESULT_SUCCEEDED(executor_keeper_->QueryInterface(&call_factory));
  ASSERT_TRUE(call_factory != NULL);
}

TEST_F(ExecutorComTest, SyncInitialize) {
  CComPtr<AsyncICeeeTabExecutor> executor_call;
  ASSERT_NO_FATAL_FAILURE(CreateTabExecutor(&executor_call));

  static const CeeeWindowHandle kWindow = 0xF1F2F3F4;
  // We expect this to run synchronously at begin time.
  EXPECT_CALL(*synchronize_, Signal());
  EXPECT_CALL(*executor_, Initialize(kWindow))
      .WillOnce(Return(E_CAFEBABE));

  ASSERT_HRESULT_SUCCEEDED(executor_call->Begin_Initialize(kWindow));

  // Verify that we return the exepected HRESULT.
  ASSERT_EQ(E_CAFEBABE, executor_call->Finish_Initialize());
}

TEST_F(ExecutorComTest, AsyncGetTabInfo) {
  CComPtr<AsyncICeeeTabExecutor> executor_call;
  ASSERT_NO_FATAL_FAILURE(CreateTabExecutor(&executor_call));
  ASSERT_HRESULT_SUCCEEDED(executor_call->Begin_GetTabInfo());

  ASSERT_NO_FATAL_FAILURE(RunShortMessageLoop());

  // We expect the call after the message loop, as there's an out
  // param we have to cater to.
  CeeeTabInfo tab_info = {};
  EXPECT_CALL(*executor_, GetTabInfo(&tab_info))
      .WillOnce(Return(E_CAFEBABE));

  // Verify that we return the expected HRESULT.
  ASSERT_EQ(E_CAFEBABE, executor_call->Finish_GetTabInfo(&tab_info));
}

TEST_F(ExecutorComTest, AsyncNavigate) {
  CComPtr<AsyncICeeeTabExecutor> executor_call;
  ASSERT_NO_FATAL_FAILURE(CreateTabExecutor(&executor_call));

  ASSERT_HRESULT_SUCCEEDED(executor_call->Begin_Navigate(
      CComBSTR(kUrl), kFlags, CComBSTR(kTarget)));

  // We expect the call to from within the message loop.
  EXPECT_CALL(*executor_, Navigate(StrEq(kUrl), kFlags, StrEq(kTarget)))
      .WillOnce(Return(E_CAFEBABE));

  ASSERT_NO_FATAL_FAILURE(RunShortMessageLoop());

  // Verify that we return the exepected HRESULT.
  ASSERT_EQ(E_CAFEBABE, executor_call->Finish_Navigate());
}

TEST_F(ExecutorComTest, AsyncInsertCode) {
  CComPtr<AsyncICeeeTabExecutor> executor_call;
  ASSERT_NO_FATAL_FAILURE(CreateTabExecutor(&executor_call));
  const wchar_t* kCode = L"window.alert(\"You've been had.\")";
  const wchar_t* kFile = L"CONSOLE";
  ASSERT_HRESULT_SUCCEEDED(executor_call->Begin_InsertCode(
      CComBSTR(kCode), CComBSTR(kFile), TRUE, kCeeeTabCodeTypeJs));

  // Now set up the expectations we have for invocations
  // during the brief message loop.
  EXPECT_CALL(*executor_,
      InsertCode(StrEq(kCode), StrEq(kFile), TRUE, kCeeeTabCodeTypeJs))
          .WillOnce(Return(E_CAFEBABE));

  ASSERT_NO_FATAL_FAILURE(RunShortMessageLoop());

  // Verify that we return the expected HRESULT.
  ASSERT_EQ(E_CAFEBABE, executor_call->Finish_InsertCode());
}

// This unittest fails due to a bug in COM, whereby asynchronous calls across
// apartments in the same process cause an access violation on freeing an
// invalid pointer.
// This is true as of 15 Nov 2010, leaving this around in hopes Microsoft
// comes through with a fix.
TEST_F(ExecutorComTest, DISABLED_CrossApartmentCall) {
  RemoteObjectHost host;
  ASSERT_NO_FATAL_FAILURE(host.HostObject(executor_keeper_));

  // Register home and away proxies.
  std::vector<DWORD> invoker_cookies;
  host.RunSync(NewRunnableFunction(RegisterProxyStubs, &invoker_cookies));
  RegisterProxies();

  CComPtr<ICeeeTabExecutor> executor;
  ASSERT_HRESULT_SUCCEEDED(host.remote_object_.QueryInterface(&executor));

  // A navigation call should go through with no trouble.
  EXPECT_CALL(*executor_, Navigate(StrEq(kUrl), 0, StrEq(kTarget)))
      .WillOnce(Return(S_OK));

  EXPECT_HRESULT_SUCCEEDED(
      executor->Navigate(CComBSTR(kUrl), 0, CComBSTR(kTarget)));
}

class TestClassFactory
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<TestClassFactory>,
      public IClassFactory {
 public:
  BEGIN_COM_MAP(TestClassFactory)
    COM_INTERFACE_ENTRY(IClassFactory)
  END_COM_MAP()

  TestClassFactory() : executor_(NULL) {
  }

  HRESULT Initialize(TestCeeeExecutor* executor) {
    executor_ = executor;
    return S_OK;
  }

  STDMETHOD(CreateInstance)(IUnknown *outer, REFIID riid, void **out) {
    if (outer != NULL) {
      ADD_FAILURE() << "Attempted aggregation in CreateInstance";
      return CLASS_E_NOAGGREGATION;
    }

    return executor_->QueryInterface(riid, out);
  }

  STDMETHOD(LockServer)(BOOL lock) {
    ADD_FAILURE() << "Unexpected call to LockServer";
    return E_NOTIMPL;
  }

  TestCeeeExecutor* executor_;
};

const wchar_t* kHKCUReplacement =
  L"Software\\Google\\ExecutorComInvocationTest\\HKCU";

// Registry script to register iid->async iid mapping.
const wchar_t* kRegScript =
  L"%ROOT% {\n"
  L"  Software {\n"
  L"    Classes {\n"
  L"      NoRemove 'Interface' {\n"
  L"        '%IID%' = s '%NAME%' {\n"
  L"          AsynchronousInterface = s '%ASYNC_IID%'\n"
  L"        }\n"
  L"      }\n"
  L"    }\n"
  L"  }\n"
  L"}\n";



// This fixture assists in launching a sub-process to invoke on our executor.
// We can't do this from another thread in this process because of a bug
// in the COM  marshaling machinery that causes an access violation after
// an asynchronous invocation across single-threaded apartments in the
// same process.
class ExecutorComInvocationTest: public ExecutorComTest {
 public:
  // We register in HKCU if the user is non-admin, otherwise in
  // HKLM, because on UAC-enabled systems, HKCU is ignored for
  // administrative users.
  ExecutorComInvocationTest()
      : root_key_(::IsUserAnAdmin() ? L"HKLM" : L"HKCU"),
        clsid_(GUID_NULL),
        cookie_(0),
        invoker_(base::kNullProcessHandle) {
  }

  void UpdateRegistry(bool reg) {
    // Register or unregister the sync->async IID mapping in our HKCR.
    // Unfortunately there's no way to do this without changing machine-global
    // state. The COM marshaling machinery does lookups in HKCR, which seems
    // to be amalgamated from HKCU and HKLM in kernel-land, so
    // RegOverridePredefKey doesn't help.
    CRegObject reg_obj;
    ASSERT_HRESULT_SUCCEEDED(reg_obj.FinalConstruct());
    ASSERT_HRESULT_SUCCEEDED(
        reg_obj.AddReplacement(L"IID", CComBSTR(IID_ICeeeTabExecutor)));
    ASSERT_HRESULT_SUCCEEDED(
        reg_obj.AddReplacement(L"ASYNC_IID",
                               CComBSTR(IID_AsyncICeeeTabExecutor)));
    ASSERT_HRESULT_SUCCEEDED(
        reg_obj.AddReplacement(L"NAME", L"ICeeeTabExecutor"));
    ASSERT_HRESULT_SUCCEEDED(
        reg_obj.AddReplacement(L"ROOT", root_key_));

    if (reg) {
      ASSERT_HRESULT_SUCCEEDED(reg_obj.StringRegister(kRegScript));
    } else {
      ASSERT_HRESULT_SUCCEEDED(reg_obj.StringUnregister(kRegScript));
    }
  }

  virtual void SetUp() {
    ASSERT_NO_FATAL_FAILURE(ExecutorComTest::SetUp());
    ASSERT_NO_FATAL_FAILURE(UpdateRegistry(true));
    ASSERT_NO_FATAL_FAILURE(RegisterProxies());

    // Register a class factory with a new clsid.
    ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&clsid_));
    ASSERT_HRESULT_SUCCEEDED(
        TestClassFactory::CreateInitialized(executor_, &class_factory_));
    ASSERT_HRESULT_SUCCEEDED(::CoRegisterClassObject(clsid_,
                                                     class_factory_,
                                                     CLSCTX_LOCAL_SERVER,
                                                     REGCLS_MULTIPLEUSE,
                                                     &cookie_));
  }

  virtual void TearDown() {
    // Wait for the invoker and assert it returned E_CAFEBABE.
    if (invoker_ != base::kNullProcessHandle) {
      int exit_code = -1;
      EXPECT_TRUE(base::WaitForExitCode(invoker_, &exit_code));
      EXPECT_EQ(E_CAFEBABE, exit_code);
    }

    EXPECT_HRESULT_SUCCEEDED(::CoRevokeClassObject(cookie_));
    EXPECT_NO_FATAL_FAILURE(UpdateRegistry(false));
    ExecutorComTest::TearDown();
  }

  template <size_t N>
  void InvokeExecutor(const char* func, const char* (&args)[N]) {
    FilePath testing_invoke_executor;
    ASSERT_TRUE(PathService::Get(base::DIR_EXE, &testing_invoke_executor));
    testing_invoke_executor =
        testing_invoke_executor.Append(L"testing_invoke_executor.exe");

    wchar_t clsid_str[40] = {};
    ASSERT_NE(0, ::StringFromGUID2(clsid_, clsid_str, arraysize(clsid_str)));
    CommandLine cmd_line(testing_invoke_executor);
    cmd_line.AppendSwitchNative("class_id", clsid_str);
    cmd_line.AppendSwitchASCII("func", func);

    ASSERT_TRUE(N % 2 == 0);
    for (size_t i = 0; i < N; i += 2) {
      cmd_line.AppendSwitchASCII(args[i], args[i + 1]);
    }

    ASSERT_TRUE(base::LaunchApp(cmd_line, false, true, &invoker_));
  }

 protected:
  GUID clsid_;
  DWORD cookie_;
  CComPtr<IClassFactory> class_factory_;
  const wchar_t* root_key_;
  base::ProcessHandle invoker_;
};


TEST_F(ExecutorComInvocationTest, SyncInitialize) {
  const char* args[] = {
    "hwnd", "1457",
  };
  ASSERT_NO_FATAL_FAILURE(InvokeExecutor("Initialize", args));

  {
    InSequence sequence;

    // We expect the invocation to do a create tab call,
    // followed by a navigate.
    ExpectCreateTabCall(executor_);
    EXPECT_CALL(*executor_, Initialize(static_cast<CeeeWindowHandle>(1457)))
        .WillOnce(
            DoAll(
                QuitMessageLoop(&loop_),
                Return(E_CAFEBABE)));
  }

  loop_.Run();
}

TEST_F(ExecutorComInvocationTest, AsyncGetTabInfo) {
  const char* args[] = {
    "ignore", "me",
  };
  ASSERT_NO_FATAL_FAILURE(InvokeExecutor("GetTabInfo", args));

  {
    InSequence sequence;

    // We expect the invocation to do a create tab call,
    // followed by a navigate.
    ExpectCreateTabCall(executor_);
    EXPECT_CALL(*executor_, GetTabInfo(NotNull()))
        .WillOnce(
            DoAll(
                QuitMessageLoop(&loop_),
                Return(E_CAFEBABE)));
  }

  loop_.Run();
}

TEST_F(ExecutorComInvocationTest, AsyncNavigate) {
  const char* args[] = {
    "url", "http://www.google.com/",
    "flags", "74565",
    "target", "_blank",
  };
  ASSERT_NO_FATAL_FAILURE(InvokeExecutor("Navigate", args));

  {
    InSequence sequence;

    // We expect the invocation to do a create tab call,
    // followed by a navigate.
    ExpectCreateTabCall(executor_);
    EXPECT_CALL(*executor_, Navigate(StrEq(L"http://www.google.com/"),
                                     kFlags,
                                     StrEq(kTarget)))
        .WillOnce(
            DoAll(
                QuitMessageLoop(&loop_),
                Return(E_CAFEBABE)));
  }

  loop_.Run();
}

TEST_F(ExecutorComInvocationTest, AsyncInsertCode) {
  const char* args[] = {
    "code", "alert('you\'ve been had')",
    "file", "CONSOLE",
    "all_frames", "true",
    "type", "1",  // kCeeeTabCodeTypeJs
  };
  ASSERT_NO_FATAL_FAILURE(InvokeExecutor("InsertCode", args));

  {
    InSequence sequence;

    // We expect the invocation to do a create tab call,
    // followed by a navigate.
    ExpectCreateTabCall(executor_);
    EXPECT_CALL(*executor_, InsertCode(StrEq(L"alert('you\'ve been had')"),
                                       StrEq(L"CONSOLE"),
                                       TRUE,
                                       kCeeeTabCodeTypeJs))
            .WillOnce(
                DoAll(
                    QuitMessageLoop(&loop_),
                    Return(E_CAFEBABE)));
  }

  loop_.Run();
}

}  // namespace
