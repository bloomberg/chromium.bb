// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Content script manager implementation unit tests.
#include "ceee/ie/plugin/scripting/content_script_manager.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "ceee/ie/plugin/scripting/userscripts_librarian.h"
#include "ceee/ie/testing/mock_frame_event_handler_host.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/dispex_mocks.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mshtml_mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::CopyInterfaceToArgument;
using testing::CopyVariantToArgument;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

typedef UserScriptsLibrarian::JsFileList JsFileList;
typedef UserScriptsLibrarian::JsFile JsFile;

using testing::InstanceCountMixin;
using testing::IActiveScriptSiteMockImpl;
using testing::IActiveScriptSiteDebugMockImpl;

// An arbitrary valid extension ID.
const wchar_t kExtensionId[] = L"fepbkochiplomghbdfgekenppangbiap";

class TestingContentScriptManager: public ContentScriptManager {
 public:
  // Expose public for testing.
  using ContentScriptManager::LoadScriptsImpl;
  using ContentScriptManager::GetOrCreateScriptHost;
  using ContentScriptManager::InitializeScriptHost;

  MOCK_METHOD1(CreateScriptHost, HRESULT(IScriptHost** host));
  MOCK_METHOD2(GetHeadNode,
      HRESULT(IHTMLDocument* document, IHTMLDOMNode** dom_head));
  MOCK_METHOD3(InjectStyleTag,
      HRESULT(IHTMLDocument2* document, IHTMLDOMNode* dom_head,
              const wchar_t* code));
  MOCK_METHOD2(InsertCss,
      HRESULT(const wchar_t* code, IHTMLDocument2* document));
};


class IScriptHostMockImpl: public IScriptHost {
 public:
  MOCK_METHOD3(RegisterScriptObject,
               HRESULT(const wchar_t* name, IDispatch* disp_obj, bool global));
  MOCK_METHOD2(RunScript,
               HRESULT(const wchar_t* file_path, const wchar_t* code));
  MOCK_METHOD3(RunScript, HRESULT(const wchar_t* file_path,
                                  size_t char_offset,
                                  const wchar_t* code));
  MOCK_METHOD2(RunExpression,
               HRESULT(const wchar_t* code, VARIANT* result));
  MOCK_METHOD0(Close, HRESULT());
};

class MockDomNode
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockDomNode>,
      public StrictMock<IHTMLDOMNodeMockImpl> {
  BEGIN_COM_MAP(MockDomNode)
    COM_INTERFACE_ENTRY(IHTMLDOMNode)
  END_COM_MAP()

  HRESULT Initialize() {
    return S_OK;
  }
};

class MockScriptHost
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockScriptHost>,
      public InstanceCountMixin<MockScriptHost>,
      public StrictMock<IActiveScriptSiteMockImpl>,
      public StrictMock<IActiveScriptSiteDebugMockImpl>,
      public IObjectWithSiteImpl<MockScriptHost>,
      public StrictMock<IScriptHostMockImpl> {
 public:
  BEGIN_COM_MAP(MockScriptHost)
    COM_INTERFACE_ENTRY(IActiveScriptSite)
    COM_INTERFACE_ENTRY(IActiveScriptSiteDebug)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY_IID(IID_IScriptHost, IScriptHost)
  END_COM_MAP()

  HRESULT Initialize(MockScriptHost** script_host) {
    *script_host = this;
    return S_OK;
  }
};

class MockWindow
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockWindow>,
      public InstanceCountMixin<MockWindow>,
      public StrictMock<IHTMLWindow2MockImpl> {
 public:
  BEGIN_COM_MAP(MockWindow)
    COM_INTERFACE_ENTRY(IHTMLWindow2)
  END_COM_MAP()

  HRESULT Initialize(MockWindow** self) {
    *self = this;
    return S_OK;
  }
};

class MockDispatch
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockDispatch>,
      public InstanceCountMixin<MockDispatch>,
      public StrictMock<testing::IDispatchExMockImpl> {
 public:
  BEGIN_COM_MAP(MockDispatch)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  HRESULT Initialize(MockDispatch** self) {
    *self = this;
    return S_OK;
  }
};

class MockDocument
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockDocument>,
      public InstanceCountMixin<MockDocument>,
      public StrictMock<IHTMLDocument2MockImpl> {
 public:
  BEGIN_COM_MAP(MockDocument)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
  END_COM_MAP()

  HRESULT Initialize(MockDocument** self) {
    *self = this;
    return S_OK;
  }
};

class MockFrameEventHandlerAsApiHost
    : public testing::MockFrameEventHandlerHostBase<
        MockFrameEventHandlerAsApiHost> {
 public:
  HRESULT Initialize(MockFrameEventHandlerAsApiHost** self) {
    *self = this;
    return S_OK;
  }

  // Always supply ourselves as native API host.
  HRESULT GetExtensionPortMessagingProvider(
      IExtensionPortMessagingProvider** messaging_provider) {
    GetUnknown()->AddRef();
    *messaging_provider = this;
    return S_OK;
  }
};

class ContentScriptManagerTest: public testing::Test {
 public:
  void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(
        MockFrameEventHandlerAsApiHost::CreateInitializedIID(
            &frame_host_, IID_IFrameEventHandlerHost, &frame_host_keeper_));

    ASSERT_HRESULT_SUCCEEDED(
        MockScriptHost::CreateInitializedIID(
            &script_host_, IID_IScriptHost, &script_host_keeper_));

    ASSERT_HRESULT_SUCCEEDED(
        MockWindow::CreateInitialized(&window_, &window_keeper_));

    ASSERT_HRESULT_SUCCEEDED(
        MockDocument::CreateInitialized(&document_, &document_keeper_));

    ASSERT_HRESULT_SUCCEEDED(
        MockDispatch::CreateInitialized(&function_, &function_keeper_));

    // Set the document up to return the content window if queried.
    EXPECT_CALL(*document_, get_parentWindow(_))
        .WillRepeatedly(
            DoAll(
                CopyInterfaceToArgument<0>(window_keeper_),
                Return(S_OK)));
  }

  void TearDown() {
    document_ = NULL;
    document_keeper_.Release();

    window_ = NULL;
    window_keeper_.Release();

    script_host_ = NULL;
    script_host_keeper_.Release();

    frame_host_ = NULL;
    frame_host_keeper_.Release();

    function_ = NULL;
    function_keeper_.Release();

    // Test for leakage.
    EXPECT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }

  // Set up to expect scripting initialization.
  void ExpectScriptInitialization() {
    EXPECT_CALL(*frame_host_, GetExtensionId(_)).WillOnce(DoAll(
        SetArgumentPointee<0>(std::wstring(kExtensionId)),
        Return(S_OK)));

    EXPECT_CALL(*script_host_,
        RunScript(testing::StartsWith(L"ceee-content://"), _))
        .Times(5).WillRepeatedly(Return(S_OK));

    // Return the mock function for start/end init.
    EXPECT_CALL(*script_host_, RunExpression(StrEq(L"ceee.startInit_"), _))
        .WillOnce(DoAll(
            CopyVariantToArgument<1>(CComVariant(function_keeper_)),
            Return(S_OK)));
    EXPECT_CALL(*script_host_, RunExpression(StrEq(L"ceee.endInit_"), _))
        .WillOnce(DoAll(
            CopyVariantToArgument<1>(CComVariant(function_keeper_)),
            Return(S_OK)));

    // Register the window object and initialize its globals.
    EXPECT_CALL(*script_host_,
        RegisterScriptObject(StrEq(L"window"), _, true))
            .WillOnce(Return(S_OK));

    // And expect three invocations.
    // TODO(siggi@chromium.org): be more specific?
    EXPECT_CALL(*function_, Invoke(_, _, _, _, _, _, _, _))
        .Times(2).WillRepeatedly(Return(S_OK));
  }

  void ExpectCreateScriptHost(TestingContentScriptManager* manager) {
    EXPECT_CALL(*manager, CreateScriptHost(_))
        .WillOnce(
            DoAll(
                CopyInterfaceToArgument<0>(script_host_keeper_),
                Return(S_OK)));
  }

  // Set up to expect NO scripting initialization.
  void ExpectNoScriptInitialization() {
    EXPECT_CALL(*script_host_, RegisterScriptObject(_, _, _))
        .Times(0);
  }

  // Set up to expect a query CSS content.
  void ExpectCSSQuery(const GURL& url) {
    // Expect CSS query.
    EXPECT_CALL(*frame_host_,
                GetMatchingUserScriptsCssContent(url, false, _)).
        WillOnce(Return(S_OK));
  }

  void SetScriptQueryResults(const GURL& url,
                             UserScript::RunLocation location,
                             const JsFileList& js_file_list) {
    EXPECT_CALL(*frame_host_,
                GetMatchingUserScriptsJsContent(url, location, false, _))
        .WillOnce(
            DoAll(
                SetArgumentPointee<3>(js_file_list),
                Return(S_OK)));
  }

 protected:
  TestingContentScriptManager manager;

  MockFrameEventHandlerAsApiHost* frame_host_;
  CComPtr<IFrameEventHandlerHost> frame_host_keeper_;

  MockScriptHost* script_host_;
  CComPtr<IScriptHost> script_host_keeper_;

  MockWindow* window_;
  CComPtr<IHTMLWindow2> window_keeper_;

  MockDocument* document_;
  CComPtr<IHTMLDocument2> document_keeper_;

  // Standin for JS functions.
  MockDispatch *function_;
  CComPtr<IDispatch> function_keeper_;
};

TEST_F(ContentScriptManagerTest, InitializationAndTearDownSucceed) {
  ContentScriptManager manager;

  ASSERT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  manager.TearDown();
}

TEST_F(ContentScriptManagerTest, InitializeScripting) {
  TestingContentScriptManager manager;

  EXPECT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  ExpectScriptInitialization();
  ASSERT_HRESULT_SUCCEEDED(
      manager.InitializeScriptHost(document_, script_host_));
}

const GURL kTestUrl(
    L"http://www.google.com/search?q=Google+Buys+Iceland");

// Verify that we don't initialize scripting when there's nothing to inject.
TEST_F(ContentScriptManagerTest, NoScriptInitializationOnEmptyScripts) {
  TestingContentScriptManager manager;
  ASSERT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  // No script host creation.
  EXPECT_CALL(manager, CreateScriptHost(_)).Times(0);

  SetScriptQueryResults(kTestUrl, UserScript::DOCUMENT_START, JsFileList());
  ASSERT_HRESULT_SUCCEEDED(
      manager.LoadStartScripts(kTestUrl, document_));

  SetScriptQueryResults(kTestUrl, UserScript::DOCUMENT_END, JsFileList());
  ASSERT_HRESULT_SUCCEEDED(
      manager.LoadEndScripts(kTestUrl, document_));

  ASSERT_HRESULT_SUCCEEDED(manager.TearDown());
}

const wchar_t kJsFilePath1[] = L"foo.js";
const char kJsFileContent1[] = "window.alert('XSS!!');";
const wchar_t kJsFilePath2[] = L"bar.js";
const char kJsFileContent2[] =
    "window.alert = function () { console.log('gotcha'); }";

// Verify that we initialize scripting and inject when there's a URL match.
TEST_F(ContentScriptManagerTest, ScriptInitializationOnUrlMatch) {
  TestingContentScriptManager manager;
  ASSERT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  JsFileList list;
  list.push_back(JsFile());
  JsFile& file1 = list.back();
  file1.file_path = kJsFilePath1;
  file1.content = kJsFileContent1;
  list.push_back(JsFile());
  JsFile& file2 = list.back();
  file2.file_path = kJsFilePath2;
  file2.content = kJsFileContent2;

  SetScriptQueryResults(kTestUrl, UserScript::DOCUMENT_START, list);

  ExpectScriptInitialization();
  ExpectCreateScriptHost(&manager);
  const std::wstring content1(UTF8ToWide(kJsFileContent1));
  EXPECT_CALL(*script_host_,
      RunScript(StrEq(kJsFilePath1), StrEq(content1.c_str())))
          .WillOnce(Return(S_OK));

  const std::wstring content2(UTF8ToWide(kJsFileContent2));
  EXPECT_CALL(*script_host_,
      RunScript(StrEq(kJsFilePath2), StrEq(content2.c_str())))
          .WillOnce(Return(S_OK));

  // This should initialize scripting and evaluate our script.
  ASSERT_HRESULT_SUCCEEDED(
      manager.LoadStartScripts(kTestUrl, document_));

  // Drop the second script.
  list.pop_back();
  SetScriptQueryResults(kTestUrl, UserScript::DOCUMENT_END, list);

  EXPECT_CALL(*script_host_,
      RunScript(StrEq(kJsFilePath1), StrEq(content1.c_str())))
          .WillOnce(Return(S_OK));

  // This should only evaluate the script.
  ASSERT_HRESULT_SUCCEEDED(
      manager.LoadEndScripts(kTestUrl, document_));

  // The script host needs to be shut down on teardown.
  EXPECT_CALL(*script_host_, Close()).Times(1);
  EXPECT_CALL(*frame_host_, CloseAll(_)).Times(1);
  ASSERT_HRESULT_SUCCEEDED(manager.TearDown());
}

const wchar_t kCssContent[] = L".foo {};";
// Verify that we inject CSS into the document.
TEST_F(ContentScriptManagerTest, CssInjectionOnUrlMatch) {
  TestingContentScriptManager manager;
  ASSERT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  EXPECT_CALL(*frame_host_,
      GetMatchingUserScriptsCssContent(kTestUrl, false, _))
          .WillOnce(Return(S_OK));

  // This should not cause any CSS injection.
  ASSERT_HRESULT_SUCCEEDED(manager.LoadCss(kTestUrl, document_));

  EXPECT_CALL(*frame_host_,
      GetMatchingUserScriptsCssContent(kTestUrl, false, _))
          .WillOnce(
              DoAll(
                  SetArgumentPointee<2>(std::string(CW2A(kCssContent))),
                  Return(S_OK)));

  EXPECT_CALL(manager,
      InsertCss(StrEq(kCssContent), document_))
          .WillOnce(Return(S_OK));

  // We now expect to see the document injected.
  ASSERT_HRESULT_SUCCEEDED(manager.LoadCss(kTestUrl, document_));
}

const wchar_t kTestCode[] = L"function foo {};";
const wchar_t kTestFilePath[] = L"TestFilePath";
TEST_F(ContentScriptManagerTest, ExecuteScript) {
  TestingContentScriptManager manager;
  ASSERT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  CComPtr<IHTMLDOMNode> head_node;
  ASSERT_HRESULT_SUCCEEDED(MockDomNode::CreateInitialized(&head_node));

  ExpectCreateScriptHost(&manager);
  ExpectScriptInitialization();

  EXPECT_CALL(*script_host_,
      RunScript(kTestFilePath, kTestCode))
          .WillOnce(Return(S_OK));

  // We now expect to see the document injected.
  ASSERT_HRESULT_SUCCEEDED(manager.ExecuteScript(kTestCode,
                                                 kTestFilePath,
                                                 document_));

  // The script host needs to be shut down on teardown.
  EXPECT_CALL(*script_host_, Close()).Times(1);
  EXPECT_CALL(*frame_host_, CloseAll(_)).Times(1);
  ASSERT_HRESULT_SUCCEEDED(manager.TearDown());
}

TEST_F(ContentScriptManagerTest, InsertCss) {
  TestingContentScriptManager manager;
  ASSERT_HRESULT_SUCCEEDED(
      manager.Initialize(frame_host_keeper_, false));

  CComPtr<IHTMLDOMNode> head_node;
  ASSERT_HRESULT_SUCCEEDED(MockDomNode::CreateInitialized(&head_node));

  EXPECT_CALL(manager, GetHeadNode(document_, _))
      .WillOnce(
          DoAll(
              CopyInterfaceToArgument<1>(head_node),
              Return(S_OK)));

  EXPECT_CALL(manager,
      InjectStyleTag(document_, head_node.p, StrEq(kCssContent)))
              .WillOnce(Return(S_OK));

  ASSERT_HRESULT_SUCCEEDED(
        manager.ContentScriptManager::InsertCss(kCssContent, document_));
}

}  // namespace
