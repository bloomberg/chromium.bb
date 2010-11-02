// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Script host implementation unit tests.

#include "ceee/ie/plugin/scripting/script_host.h"

#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/dispex_mocks.h"
#include "ceee/testing/utils/mshtml_mocks.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::InstanceCountMixin;
using testing::MockDispatchEx;
using testing::MockIServiceProvider;

using testing::_;
using testing::AddRef;
using testing::AllOf;
using testing::CopyInterfaceToArgument;
using testing::DispParamArgEq;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::StrEq;

class MockIHTMLWindow2
    : public CComObjectRootEx<CComSingleThreadModel>,
      public StrictMock<IHTMLWindow2MockImpl>,
      public InstanceCountMixin<MockIHTMLWindow2> {
 public:
  BEGIN_COM_MAP(MockIHTMLWindow2)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLWindow2)
    COM_INTERFACE_ENTRY(IHTMLFramesCollection2)
  END_COM_MAP()

  HRESULT Initialize(MockIHTMLWindow2** self) {
    *self = this;
    return S_OK;
  }
};

class MockProcessDebugManager
    : public CComObjectRootEx<CComSingleThreadModel>,
      public StrictMock<testing::IProcessDebugManagerMockImpl>,
      public InstanceCountMixin<MockProcessDebugManager> {
 public:
  BEGIN_COM_MAP(MockProcessDebugManager)
    COM_INTERFACE_ENTRY(IProcessDebugManager)
  END_COM_MAP()

  HRESULT Initialize(MockProcessDebugManager** debug_manager) {
    *debug_manager = this;
    return S_OK;
  }
};

class MockDebugApplication
    : public CComObjectRootEx<CComSingleThreadModel>,
      public StrictMock<testing::IDebugApplicationMockImpl>,
      public InstanceCountMixin<MockDebugApplication> {
 public:
  BEGIN_COM_MAP(MockDebugApplication)
    COM_INTERFACE_ENTRY(IDebugApplication)
  END_COM_MAP()

  HRESULT Initialize(MockDebugApplication** debug_application) {
    *debug_application = this;
    return S_OK;
  }
};

class MockDebugDocumentHelper
    : public CComObjectRootEx<CComSingleThreadModel>,
      public StrictMock<testing::IDebugDocumentHelperMockImpl>,
      public InstanceCountMixin<MockDebugDocumentHelper> {
 public:
  BEGIN_COM_MAP(MockDebugDocumentHelper)
    COM_INTERFACE_ENTRY(IDebugDocumentHelper)
  END_COM_MAP()

  HRESULT Initialize(MockDebugDocumentHelper** debug_document) {
    *debug_document = this;
    return S_OK;
  }
};

const wchar_t* kDebugApplicationName = L"ScriptHostTest";

class TestingDebugApplication : public ScriptHost::DebugApplication {
 public:
  TestingDebugApplication()
      : ScriptHost::DebugApplication(kDebugApplicationName) {
  }

  MOCK_METHOD1(CreateProcessDebugManager,
      HRESULT(IProcessDebugManager** manager));
};

class MockIActiveScriptAndParse
    : public CComObjectRootEx<CComSingleThreadModel>,
      public StrictMock<testing::IActiveScriptMockImpl>,
      public StrictMock<testing::IActiveScriptParseMockImpl>,
      public StrictMock<testing::IObjectSafetyMockImpl>,
      public InstanceCountMixin<MockIActiveScriptAndParse> {
 public:
  BEGIN_COM_MAP(MockIActiveScriptAndParse)
    COM_INTERFACE_ENTRY(IActiveScript)
    COM_INTERFACE_ENTRY(IActiveScriptParse)
    COM_INTERFACE_ENTRY(IObjectSafety)
  END_COM_MAP()

  HRESULT Initialize(MockIActiveScriptAndParse** script) {
    *script = this;
    return S_OK;
  }
};

class TestingScriptHost
    : public ScriptHost,
      public InstanceCountMixin<TestingScriptHost> {
 public:
  HRESULT Initialize(ScriptHost::DebugApplication* debug,
                     IActiveScript* script) {
    my_script_ = script;
    ScriptHost::Initialize(debug);
    return S_OK;
  }

 private:
  HRESULT CreateDebugManager(IProcessDebugManager** debug_manager) {
    return my_debug_manager_.CopyTo(debug_manager);
  }
  HRESULT CreateScriptEngine(IActiveScript** script) {
    return my_script_.CopyTo(script);
  }

  CComPtr<IProcessDebugManager> my_debug_manager_;
  CComPtr<IActiveScript> my_script_;
};

const DISPID kDispId = 5;
const wchar_t* kDispObjName = L"tpain";
const wchar_t* kDispObjName2 = L"akon";
const wchar_t* kJsFilePath = L"liljon.js";
const wchar_t* kJsCode = L"alert('WWHHHATTT? OOOKKAAYY.')";
const DWORD kAddFlags = SCRIPTITEM_ISSOURCE | SCRIPTITEM_ISVISIBLE;
const DWORD kAddGlobalFlags = kAddFlags | SCRIPTITEM_GLOBALMEMBERS;
const DWORD kSourceContext = 123456;
const DWORD kUnknownSourceContext = 654321;
const DWORD kNoSourceContext = 0;
const DWORD kScriptFlags =
    SCRIPTTEXT_HOSTMANAGESSOURCE | SCRIPTTEXT_ISVISIBLE;
const DWORD kExpressionFlags = SCRIPTTEXT_ISEXPRESSION;
const ULONG kCharOffset = 1234;
const ULONG kNumChars = 4321;

class ScriptHostTest : public testing::Test {
 public:
  ScriptHostTest() : mock_debug_manager_(NULL), mock_debug_application_(NULL),
      mock_script_(NULL), debug_(kDebugApplicationName) {
  }

  void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(
        InitializingCoClass<MockProcessDebugManager>::
            CreateInitializedIID(&mock_debug_manager_,
                                 IID_IProcessDebugManager,
                                 &debug_manager_));
    ASSERT_HRESULT_SUCCEEDED(
        InitializingCoClass<MockDebugApplication>::
            CreateInitializedIID(&mock_debug_application_,
                                 IID_IDebugApplication,
                                 &debug_application_));
    ASSERT_HRESULT_SUCCEEDED(
        InitializingCoClass<MockIActiveScriptAndParse>::CreateInitializedIID(
            &mock_script_, IID_IActiveScript, &script_));

    debug_.Initialize(debug_manager_, debug_application_);
  }

  void ExpectEngineInitialization() {
    EXPECT_CALL(*mock_script_, SetScriptSite(_)).WillOnce(
        Return(S_OK));
    EXPECT_CALL(*mock_script_, InitNew()).WillOnce(Return(S_OK));
    EXPECT_CALL(*mock_script_, SetScriptState(SCRIPTSTATE_CONNECTED)).WillOnce(
        Return(S_OK));
    EXPECT_CALL(*mock_script_, SetInterfaceSafetyOptions(
        IID_IDispatch, INTERFACE_USES_SECURITY_MANAGER,
        INTERFACE_USES_SECURITY_MANAGER)).WillOnce(Return(S_OK));
  }

  void TearDown() {
    if (script_host_) {
      EXPECT_CALL(*mock_script_, Close()).WillOnce(Return(S_OK));

      script_host_->Close();
    }

    mock_debug_manager_ = NULL;
    debug_manager_.Release();

    mock_debug_application_ = NULL;
    debug_application_.Release();

    mock_script_ = NULL;
    script_.Release();

    script_host_.Release();

    debug_.Terminate();

    // Everything should have been relinquished.
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }

  void CreateScriptHost() {
    ExpectEngineInitialization();

    ASSERT_HRESULT_SUCCEEDED(
        InitializingCoClass<TestingScriptHost>::CreateInitializedIID(
            &debug_, script_, IID_IScriptHost, &script_host_));
  }

  void ExpectCreateDebugDocumentHelper(
      MockDebugDocumentHelper** mock_debug_document) {
    CComPtr<IDebugDocumentHelper> debug_document;
    ASSERT_HRESULT_SUCCEEDED(
        InitializingCoClass<MockDebugDocumentHelper>::CreateInitialized(
            mock_debug_document, &debug_document));

    EXPECT_CALL(*mock_debug_manager_, CreateDebugDocumentHelper(NULL, _))
        .WillOnce(DoAll(CopyInterfaceToArgument<1>(debug_document),
                        Return(S_OK)));

    EXPECT_CALL(**mock_debug_document,
        Init(Eq(debug_application_), StrEq(kJsFilePath), StrEq(kJsFilePath), _))
        .WillOnce(Return(S_OK));
    EXPECT_CALL(**mock_debug_document, Attach(NULL)).WillOnce(Return(S_OK));
    EXPECT_CALL(**mock_debug_document, SetDebugDocumentHost(_)).WillOnce(
        Return(S_OK));
    EXPECT_CALL(**mock_debug_document,
                DefineScriptBlock(_, _, Eq(script_), _, _))
        .WillOnce(DoAll(SetArgumentPointee<4>(kSourceContext), Return(S_OK)));

    // Detach will be called on this document when the script host is closed
    EXPECT_CALL(**mock_debug_document, Detach()).WillOnce(Return(S_OK));
  }

  void ExpectParseScriptText(DWORD source_context, DWORD flags) {
    EXPECT_CALL(*mock_script_, ParseScriptText(StrEq(kJsCode), NULL, NULL, NULL,
                source_context, 0, flags, _, _))
        .WillOnce(Return(S_OK));
  }

  MockProcessDebugManager* mock_debug_manager_;
  CComPtr<IProcessDebugManager> debug_manager_;
  MockDebugApplication* mock_debug_application_;
  CComPtr<IDebugApplication> debug_application_;
  MockIActiveScriptAndParse* mock_script_;
  CComPtr<IActiveScript> script_;
  CComPtr<IScriptHost> script_host_;

  ScriptHost::DebugApplication debug_;
};

TEST_F(ScriptHostTest, DebugApplicationDefaultInitialize) {
  TestingDebugApplication debug;

  EXPECT_CALL(debug, CreateProcessDebugManager(_))
      .WillOnce(
          DoAll(CopyInterfaceToArgument<0>(debug_manager_),
          Return(S_OK)));

  EXPECT_CALL(*mock_debug_manager_, CreateApplication(_)).WillOnce(
      DoAll(CopyInterfaceToArgument<0>(debug_application_), Return(S_OK)));
  EXPECT_CALL(*mock_debug_application_, SetName(StrEq(kDebugApplicationName)))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(*mock_debug_manager_, AddApplication(
      static_cast<IDebugApplication*>(debug_application_), _))
          .WillOnce(DoAll(SetArgumentPointee<1>(42), Return(S_OK)));

  debug.Initialize();

  // We expect the script host to undo debug app registration.
  EXPECT_CALL(*mock_debug_manager_, RemoveApplication(42))
      .WillOnce(Return(S_OK));
  debug.Terminate();
}

// Test initializing debugging with a service provider.
TEST_F(ScriptHostTest, DebugApplicationInitializeWithServiceProvider) {
  TestingDebugApplication debug;

  MockIServiceProvider* sp_keeper;
  CComPtr<IServiceProvider> sp;
  ASSERT_HRESULT_SUCCEEDED(
    InitializingCoClass<MockIServiceProvider>::CreateInitialized(
        &sp_keeper, &sp));


  EXPECT_CALL(*sp_keeper,
      QueryService(IID_IDebugApplication, IID_IDebugApplication, _))
          .WillOnce(DoAll(
              AddRef(debug_manager_.p),
              SetArgumentPointee<2>(static_cast<void*>(debug_manager_)),
              Return(S_OK)));

  // Initialization with a service provider that yields
  // a debug application should only query service, and
  // not try to create a debug manager.
  EXPECT_CALL(debug, CreateProcessDebugManager(_)).Times(0);

  debug.Initialize(sp);

  // And there should be no app unregistration.
  debug.Terminate();
}

TEST_F(ScriptHostTest, DebugApplicationInitializeWithEmptyServiceProvider) {
  TestingDebugApplication debug;

  MockIServiceProvider* sp_keeper;
  CComPtr<IServiceProvider> sp;
  ASSERT_HRESULT_SUCCEEDED(
    InitializingCoClass<MockIServiceProvider>::CreateInitialized(
        &sp_keeper, &sp));


  EXPECT_CALL(*sp_keeper,
      QueryService(IID_IDebugApplication, IID_IDebugApplication, _))
          .WillOnce(Return(E_NOINTERFACE));

  // Initialization with a service provider that yields
  // no debug application should go the default route.
  EXPECT_CALL(debug, CreateProcessDebugManager(_))
      .WillOnce(
          DoAll(CopyInterfaceToArgument<0>(debug_manager_),
          Return(S_OK)));

  EXPECT_CALL(*mock_debug_manager_, CreateApplication(_)).WillOnce(
      DoAll(CopyInterfaceToArgument<0>(debug_application_), Return(S_OK)));
  EXPECT_CALL(*mock_debug_application_, SetName(StrEq(kDebugApplicationName)))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(*mock_debug_manager_, AddApplication(
      static_cast<IDebugApplication*>(debug_application_), _))
          .WillOnce(DoAll(SetArgumentPointee<1>(42), Return(S_OK)));

  debug.Initialize(sp);

  // And the registration should be undone.
  EXPECT_CALL(*mock_debug_manager_, RemoveApplication(42))
      .WillOnce(Return(S_OK));
  debug.Terminate();
}

TEST_F(ScriptHostTest, DebugApplicationInitFailure) {
  TestingDebugApplication debug;

  EXPECT_CALL(debug, CreateProcessDebugManager(_))
      .WillOnce(Return(REGDB_E_CLASSNOTREG));

  debug.Initialize();

  CComPtr<IDebugDocumentHelper> helper;
  EXPECT_HRESULT_SUCCEEDED(
      debug.CreateDebugDocumentHelper(kJsFilePath,
                                      kJsCode,
                                      0,
                                      &helper));
  EXPECT_TRUE(helper == NULL);

  // This must fail when debugging is not present.
  CComPtr<IDebugApplication> debug_app;
  EXPECT_HRESULT_FAILED(debug.GetDebugApplication(&debug_app));
  ASSERT_TRUE(debug_app == NULL);

  // Whereas this should succeed, but return a NULL node.
  CComPtr<IDebugApplicationNode> root_node;
  EXPECT_HRESULT_SUCCEEDED(debug.GetRootApplicationNode(&root_node));
  ASSERT_TRUE(root_node == NULL);

  debug.Terminate();
}

TEST_F(ScriptHostTest, QueryInterface) {
  CreateScriptHost();

  CComQIPtr<IActiveScriptSite> script_site(script_host_);
  ASSERT_TRUE(script_site != NULL);

  CComQIPtr<IActiveScriptSiteDebug> script_site_debug(script_host_);
  ASSERT_TRUE(script_site_debug != NULL);
}

TEST_F(ScriptHostTest, RegisterScriptObject) {
  CreateScriptHost();

  MockIHTMLWindow2* mock_dispatch_obj;
  CComPtr<IHTMLWindow2> dispatch_obj;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockIHTMLWindow2>::CreateInitialized(
          &mock_dispatch_obj, &dispatch_obj));

  // Add a non global script object.
  EXPECT_CALL(*mock_script_, AddNamedItem(StrEq(kDispObjName), kAddFlags))
      .WillOnce(Return(S_OK));
  HRESULT hr = script_host_->RegisterScriptObject(kDispObjName, dispatch_obj,
                                                  false);
  ASSERT_HRESULT_SUCCEEDED(hr);

  // Add a global script object.
  EXPECT_CALL(*mock_script_,
              AddNamedItem(StrEq(kDispObjName2), kAddGlobalFlags))
      .WillOnce(Return(S_OK));
  hr = script_host_->RegisterScriptObject(kDispObjName2, dispatch_obj,
                                                  true);
  ASSERT_HRESULT_SUCCEEDED(hr);

  // Add a duplicate named object.
  hr = script_host_->RegisterScriptObject(kDispObjName, dispatch_obj, false);
  EXPECT_EQ(hr, E_ACCESSDENIED);
}

TEST_F(ScriptHostTest, RegisterScriptObjectAndGetItemInfo) {
  CreateScriptHost();

  MockDispatchEx* mock_dispatch_obj;
  CComPtr<IDispatchEx> dispatch_obj;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(
          &mock_dispatch_obj, &dispatch_obj));

  // Add a non global script object.
  EXPECT_CALL(*mock_script_, AddNamedItem(StrEq(kDispObjName), kAddFlags))
      .WillOnce(Return(S_OK));
  HRESULT hr = script_host_->RegisterScriptObject(kDispObjName, dispatch_obj,
                                                  false);
  ASSERT_HRESULT_SUCCEEDED(hr);

  // Make sure we return the object when it's called for.
  EXPECT_CALL(*mock_dispatch_obj, GetTypeInfo(0, LANG_NEUTRAL, _))
      .WillOnce(Return(S_OK));
  CComQIPtr<IActiveScriptSite> script_site(script_host_);
  ASSERT_TRUE(script_site != NULL);

  CComPtr<IUnknown> item_iunknown;
  CComPtr<ITypeInfo> item_itypeinfo;
  hr = script_site->GetItemInfo(kDispObjName,
                                SCRIPTINFO_IUNKNOWN | SCRIPTINFO_ITYPEINFO,
                                &item_iunknown, &item_itypeinfo);
  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(dispatch_obj, item_iunknown);
}

TEST_F(ScriptHostTest, RunScript) {
  CreateScriptHost();

  MockDebugDocumentHelper* mock_debug_document;
  ExpectCreateDebugDocumentHelper(&mock_debug_document);
  ExpectParseScriptText(kSourceContext, kScriptFlags);

  HRESULT hr = script_host_->RunScript(kJsFilePath, kJsCode);
  ASSERT_HRESULT_SUCCEEDED(hr);
}

TEST_F(ScriptHostTest, RunExpression) {
  CreateScriptHost();

  ExpectParseScriptText(kNoSourceContext, kExpressionFlags);

  CComVariant dummy;
  HRESULT hr = script_host_->RunExpression(kJsCode, &dummy);
  ASSERT_HRESULT_SUCCEEDED(hr);
}

TEST_F(ScriptHostTest, RunScriptAndGetDocumentContextFromPosition) {
  CreateScriptHost();

  MockDebugDocumentHelper* mock_debug_document;
  ExpectCreateDebugDocumentHelper(&mock_debug_document);
  ExpectParseScriptText(kSourceContext, kScriptFlags);

  script_host_->RunScript(kJsFilePath, kJsCode);

  CComQIPtr<IActiveScriptSiteDebug> script_site_debug(script_host_);
  ASSERT_TRUE(script_site_debug != NULL);

  EXPECT_CALL(*mock_debug_document,
              GetScriptBlockInfo(kSourceContext, NULL, _, NULL))
      .WillOnce(DoAll(SetArgumentPointee<2>(kCharOffset), Return(S_OK)));
  EXPECT_CALL(*mock_debug_document,
              CreateDebugDocumentContext(2 * kCharOffset, kNumChars, _))
      .WillOnce(Return(S_OK));


  // Call with a known source context.
  CComPtr<IDebugDocumentContext> dummy_debug_document_context;
  HRESULT hr = script_site_debug->GetDocumentContextFromPosition(
      kSourceContext, kCharOffset, kNumChars, &dummy_debug_document_context);
  ASSERT_HRESULT_SUCCEEDED(hr);

  // Call with an unknown source context.
  hr = script_site_debug->GetDocumentContextFromPosition(
      kUnknownSourceContext, kCharOffset, kNumChars,
      &dummy_debug_document_context);
  ASSERT_TRUE(hr == E_FAIL);
}

}  // namespace
