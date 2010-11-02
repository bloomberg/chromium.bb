// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Frame event handler unittests.
#include "ceee/ie/plugin/bho/frame_event_handler.h"

#include <atlctl.h>
#include <map>

#include "base/file_util.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/testing/mock_frame_event_handler_host.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mshtml_mocks.h"
#include "ceee/testing/utils/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::IOleObjecMockImpl;
using testing::IWebBrowser2MockImpl;
using testing::InstanceCountMixinBase;
using testing::InstanceCountMixin;

using testing::_;
using testing::CopyInterfaceToArgument;
using testing::DoAll;
using testing::Return;
using testing::StrEq;
using testing::StrictMock;
using testing::SetArgumentPointee;

ScriptHost::DebugApplication debug_app(L"FrameEventHandlerUnittest");

// We need to implement this interface separately, because
// there are name conflicts with methods in IConnectionPointImpl,
// and we don't want to override those methods.
class TestIOleObjectImpl: public StrictMock<IOleObjecMockImpl> {
 public:
  // Implement the advise functions.
  STDMETHOD(Advise)(IAdviseSink* sink, DWORD* advise_cookie) {
    return advise_holder_->Advise(sink, advise_cookie);
  }
  STDMETHOD(Unadvise)(DWORD advise_cookie) {
    return advise_holder_->Unadvise(advise_cookie);
  }
  STDMETHOD(EnumAdvise)(IEnumSTATDATA **enum_advise) {
    return advise_holder_->EnumAdvise(enum_advise);
  }

  HRESULT Initialize() {
    return ::CreateOleAdviseHolder(&advise_holder_);
  }

 public:
  CComPtr<IOleAdviseHolder> advise_holder_;
};

class IPersistMockImpl: public IPersist {
 public:
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetClassID, HRESULT(CLSID *clsid));
};

class MockDocument
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockDocument>,
      public InstanceCountMixin<MockDocument>,
      public StrictMock<IHTMLDocument2MockImpl>,
      public StrictMock<IPersistMockImpl>,
      public TestIOleObjectImpl,
      public IConnectionPointContainerImpl<MockDocument>,
      public IConnectionPointImpl<MockDocument, &IID_IPropertyNotifySink> {
 public:
  BEGIN_COM_MAP(MockDocument)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLDocument)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
    COM_INTERFACE_ENTRY(IOleObject)
    COM_INTERFACE_ENTRY(IPersist)
    COM_INTERFACE_ENTRY(IConnectionPointContainer)
  END_COM_MAP()

  BEGIN_CONNECTION_POINT_MAP(MockDocument)
    CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
  END_CONNECTION_POINT_MAP()

  MockDocument() : ready_state_(READYSTATE_UNINITIALIZED) {
  }

  void FireOnClose() {
    EXPECT_HRESULT_SUCCEEDED(advise_holder_->SendOnClose());
  }

  // Override to handle DISPID_READYSTATE.
  STDMETHOD(Invoke)(DISPID member, REFIID iid, LCID locale, WORD flags,
      DISPPARAMS* params,  VARIANT *result, EXCEPINFO* ex_info,
      unsigned int* arg_error) {
    if (member == DISPID_READYSTATE && flags == DISPATCH_PROPERTYGET) {
      result->vt = VT_I4;
      result->lVal = ready_state_;
      return S_OK;
    }

    return StrictMock<IHTMLDocument2MockImpl>::Invoke(member, iid, locale,
        flags, params, result, ex_info, arg_error);
  }

  STDMETHOD(get_URL)(BSTR* url) {
    return url_.CopyTo(url);
  }

  HRESULT Initialize(MockDocument** self) {
    *self = this;
    return TestIOleObjectImpl::Initialize();
  }


  READYSTATE ready_state() const { return ready_state_; }
  void set_ready_state(READYSTATE ready_state) { ready_state_ = ready_state; }

  void FireReadyStateChange() {
    CFirePropNotifyEvent::FireOnChanged(GetUnknown(), DISPID_READYSTATE);
  }

  // Sets our ready state and fires the change event.
  void SetReadyState(READYSTATE new_ready_state) {
    if (ready_state_ == new_ready_state)
      return;
    ready_state_ = new_ready_state;
    FireReadyStateChange();
  }

  const wchar_t *url() const { return com::ToString(url_); }
  void set_url(const wchar_t* url) { url_ = url; }

 protected:
  CComBSTR url_;
  READYSTATE ready_state_;
};

class MockBrowser
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockBrowser>,
      public InstanceCountMixin<MockBrowser>,
      public StrictMock<IWebBrowser2MockImpl> {
 public:
  BEGIN_COM_MAP(MockBrowser)
    COM_INTERFACE_ENTRY(IWebBrowser2)
    COM_INTERFACE_ENTRY(IWebBrowserApp)
    COM_INTERFACE_ENTRY(IWebBrowser)
  END_COM_MAP()

  HRESULT Initialize(MockBrowser** self) {
    *self = this;
    return S_OK;
  }

  STDMETHOD(get_Parent)(IDispatch** parent) {
    this->GetUnknown()->AddRef();
    *parent = this;
    return S_OK;
  }
};

class IFrameEventHandlerHostMockImpl : public IFrameEventHandlerHost {
 public:
  MOCK_METHOD1(GetReadyState, HRESULT(READYSTATE* readystate));
  MOCK_METHOD3(GetMatchingUserScriptsCssContent,
      HRESULT(const GURL& url, bool require_all_frames,
              std::string* css_content));
  MOCK_METHOD4(GetMatchingUserScriptsJsContent,
       HRESULT(const GURL& url,
               UserScript::RunLocation location,
               bool require_all_frames,
               UserScriptsLibrarian::JsFileList* js_file_list));
  MOCK_METHOD1(GetExtensionId, HRESULT(std::wstring* extension_id));
  MOCK_METHOD1(GetExtensionPath, HRESULT(std::wstring* extension_path));
  MOCK_METHOD1(GetExtensionPortMessagingProvider,
      HRESULT(IExtensionPortMessagingProvider** messaging_provider));
  MOCK_METHOD4(InsertCode, HRESULT(BSTR, BSTR, BOOL, CeeeTabCodeType));
};

class TestFrameEventHandlerHost
    : public testing::MockFrameEventHandlerHostBase<TestFrameEventHandlerHost> {
 public:
  HRESULT Initialize(TestFrameEventHandlerHost** self) {
    *self = this;
    return S_OK;
  }
  virtual HRESULT AttachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler) {
    // Get the identity unknown.
    CComPtr<IUnknown> browser_identity_unknown;
    EXPECT_HRESULT_SUCCEEDED(
        browser->QueryInterface(&browser_identity_unknown));

    std::pair<HandlerMap::iterator, bool> result =
        handlers_.insert(std::make_pair(browser_identity_unknown, handler));
    EXPECT_TRUE(result.second);
    return S_OK;
  }

  virtual HRESULT DetachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler) {
    // Get the identity unknown.
    CComPtr<IUnknown> browser_identity_unknown;
    EXPECT_HRESULT_SUCCEEDED(

        browser->QueryInterface(&browser_identity_unknown));
    EXPECT_EQ(1, handlers_.erase(browser_identity_unknown));
    return S_OK;
  }

  virtual HRESULT OnReadyStateChanged(READYSTATE ready_state) {
    return S_OK;
  }

  bool has_browser(IUnknown* browser) {
    CComPtr<IUnknown> browser_identity(browser);

    return handlers_.find(browser_identity) != handlers_.end();
  }

  FrameEventHandler* GetHandler(IUnknown* browser) {
    CComPtr<IUnknown> browser_identity(browser);

    HandlerMap::iterator it(handlers_.find(browser_identity));
    if (it != handlers_.end())
      return NULL;

    return static_cast<FrameEventHandler*>(it->second);
  }

 private:
  typedef std::map<IUnknown*, IFrameEventHandler*> HandlerMap;
  HandlerMap handlers_;
};

class MockContentScriptManager : public ContentScriptManager {
 public:
  MOCK_METHOD3(ExecuteScript, HRESULT(const wchar_t* code,
                                      const wchar_t* file_path,
                                      IHTMLDocument2* document));
  MOCK_METHOD2(InsertCss, HRESULT(const wchar_t* code,
                                  IHTMLDocument2* document));
};

// This testing class is used to test the higher-level event handling
// behavior of FrameEventHandler by mocking out the implementation
// functions invoked on readystate transitions.
class TestingFrameEventHandler
    : public FrameEventHandler,
      public InitializingCoClass<TestingFrameEventHandler>,
      public InstanceCountMixin<TestingFrameEventHandler> {
 public:
  TestingFrameEventHandler() {}
  ~TestingFrameEventHandler() {}

  HRESULT Initialize(TestingFrameEventHandler **self,
                     IWebBrowser2* browser,
                     IWebBrowser2* parent_browser,
                     IFrameEventHandlerHost* host) {
    *self = this;
    return FrameEventHandler::Initialize(browser, parent_browser, host);
  }

  virtual void InitializeContentScriptManager() {
    content_script_manager_.reset(new MockContentScriptManager);
  }
  MockContentScriptManager* GetContentScriptManager() {
    return reinterpret_cast<MockContentScriptManager*>(
        content_script_manager_.get());
  }

  // Mock out or publicize our internal helper methods.
  MOCK_METHOD2(GetExtensionResourceContents,
               HRESULT(const FilePath& file, std::string* contents));
  MOCK_METHOD3(GetCodeOrFileContents,
               HRESULT(BSTR code, BSTR file, std::wstring* contents));
  HRESULT CallGetCodeOrFileContents(BSTR code, BSTR file,
                                    std::wstring* contents) {
    return FrameEventHandler::GetCodeOrFileContents(code, file, contents);
  }

  const std::list<DeferredInjection>& deferred_injections() {
    return deferred_injections_;
  }

  void SetupForRedoDoneInjectionsTest(BSTR url) {
    browser_url_ = url;
    loaded_css_ = true;
    loaded_start_scripts_ = true;
    loaded_end_scripts_ = true;
  }

  // Disambiguate.
  using InitializingCoClass<TestingFrameEventHandler>::
      CreateInitialized;

  // Mock out the state transition implementation functions.
  MOCK_METHOD1(LoadCss, void(const GURL& match_url));
  MOCK_METHOD1(LoadStartScripts, void(const GURL& match_url));
  MOCK_METHOD1(LoadEndScripts, void(const GURL& match_url));
};

class FrameEventHandlerTestBase: public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(
        MockBrowser::CreateInitialized(&browser_, &browser_keeper_));
    ASSERT_HRESULT_SUCCEEDED(
        MockDocument::CreateInitialized(&document_, &document_keeper_));
    ASSERT_HRESULT_SUCCEEDED(
        TestFrameEventHandlerHost::CreateInitializedIID(
            &host_, IID_IUnknown, &host_keeper_));

    ExpectGetDocument();
  }

  virtual void TearDown() {
    // Fire a close event just in case.
    if (document_)
      document_->FireOnClose();

    browser_ = NULL;
    browser_keeper_.Release();
    document_ = NULL;
    document_keeper_.Release();
    host_ = NULL;
    host_keeper_.Release();

    handler_keeper_.Release();

    ASSERT_EQ(0, InstanceCountMixinBase::all_instance_count());
  }

  void ExpectGetDocument() {
    EXPECT_CALL(*browser_, get_Document(_))
        .WillRepeatedly(DoAll(
            CopyInterfaceToArgument<0>(static_cast<IDispatch*>(document_)),
            Return(S_OK)));
  }

 protected:
  MockBrowser* browser_;
  CComPtr<IWebBrowser2> browser_keeper_;

  MockDocument* document_;
  CComPtr<IHTMLDocument2> document_keeper_;

  TestFrameEventHandlerHost* host_;
  CComPtr<IFrameEventHandlerHost> host_keeper_;

  CComPtr<IUnknown> handler_keeper_;
};

class FrameEventHandlerTest: public FrameEventHandlerTestBase {
 public:
  typedef FrameEventHandlerTestBase Base;

  static void SetUpTestCase() {
    // Never torn down as other threads in the test may need it after
    // teardown.
    ScriptHost::set_default_debug_application(&debug_app);
  }

  void TearDown() {
    handler_ = NULL;

    Base::TearDown();
  }

  void CreateHandler() {
    EXPECT_CALL(*document_, GetClassID(_)).WillOnce(
        DoAll(SetArgumentPointee<0>(CLSID_HTMLDocument), Return(S_OK)));
    IWebBrowser2* parent_browser = NULL;
    ASSERT_HRESULT_SUCCEEDED(
        TestingFrameEventHandler::CreateInitialized(
            &handler_, browser_, parent_browser, host_, &handler_keeper_));
  }

 protected:
  TestingFrameEventHandler* handler_;
};

TEST_F(FrameEventHandlerTest, WillNotAttachToNonHTMLDocument) {
  EXPECT_CALL(*document_, GetClassID(_)).WillOnce(
      DoAll(SetArgumentPointee<0>(GUID_NULL), Return(S_OK)));

  // If the document is not MSHTML, we should not attach, and
  // we should return E_DOCUMENT_NOT_MSHTML to our caller to signal this.
  IWebBrowser2* parent_browser = NULL;
  HRESULT hr = TestingFrameEventHandler::CreateInitialized(
      &handler_, browser_, parent_browser, host_, &handler_keeper_);

  EXPECT_EQ(E_DOCUMENT_NOT_MSHTML, hr);
  EXPECT_FALSE(host_->has_browser(browser_));
}

TEST_F(FrameEventHandlerTest, CreateAndDetachDoesNotCrash) {
  ASSERT_EQ(0, TestingFrameEventHandler::instance_count());

  CreateHandler();
  ASSERT_EQ(1, TestingFrameEventHandler::instance_count());

  // Assert that it registered.
  ASSERT_TRUE(host_->has_browser(browser_));

  // Release the handler early to ensure its last reference will
  // be released while handling FireOnClose.
  handler_keeper_.Release();
  handler_ = NULL;
  EXPECT_EQ(1, TestingFrameEventHandler::instance_count());

  // Should tear down and destroy itself on this event.
  document_->FireOnClose();
  ASSERT_EQ(0, TestingFrameEventHandler::instance_count());
}

const wchar_t kGoogleUrl[] =
    L"http://www.google.com/search?q=Google+Buys+Iceland";
const wchar_t kSlashdotUrl[] =
    L"http://hardware.slashdot.org/";

TEST_F(FrameEventHandlerTest, InjectsCSSAndStartScriptsOnLoadedReadystate) {
  CreateHandler();

  document_->set_url(kGoogleUrl);
  document_->set_ready_state(READYSTATE_LOADING);

  // Transitioning to loading should not cause any loads.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  // Notify the handler of the URL.
  handler_->SetUrl(CComBSTR(kGoogleUrl));
  document_->FireReadyStateChange();

  const GURL google_url(kGoogleUrl);
  // Transitioning to LOADED should load Css and start scripts.
  EXPECT_CALL(*handler_, LoadCss(google_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(google_url)).Times(1);

  // But not end scripts.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);
  document_->SetReadyState(READYSTATE_LOADED);

  // Now make like a re-navigation.
  document_->SetReadyState(READYSTATE_LOADING);

  // Transitioning back to LOADED should load Css and start scripts again.
  EXPECT_CALL(*handler_, LoadCss(google_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(google_url)).Times(1);

  // But not end scripts.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);
  document_->SetReadyState(READYSTATE_LOADED);

  // Now navigate to a different URL.
  document_->set_url(kSlashdotUrl);
  document_->set_ready_state(READYSTATE_LOADING);

  // Transitioning to loading should not cause any loads.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  handler_->SetUrl(CComBSTR(kSlashdotUrl));
  document_->FireReadyStateChange();

  const GURL slashdot_url(kSlashdotUrl);

  // Transitioning back to LOADED on the new URL should load
  // Css and start scripts again.
  EXPECT_CALL(*handler_, LoadCss(slashdot_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(slashdot_url)).Times(1);

  // But not end scripts.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);
  document_->SetReadyState(READYSTATE_LOADED);
}

TEST_F(FrameEventHandlerTest, InjectsEndScriptsOnCompleteReadystate) {
  CreateHandler();

  document_->set_url(kGoogleUrl);
  document_->set_ready_state(READYSTATE_LOADING);

  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);

  // Transitioning to loading should not cause any loads.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  // Notify the handler of the URL.
  handler_->SetUrl(CComBSTR(kGoogleUrl));
  document_->FireReadyStateChange();

  const GURL google_url(kGoogleUrl);
  // Transitioning to LOADED should load Css and start scripts.
  EXPECT_CALL(*handler_, LoadCss(google_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(google_url)).Times(1);

  // But not end scripts.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);
  document_->SetReadyState(READYSTATE_LOADED);

  // Transitioning to INTERACTIVE should be a no-op.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  document_->SetReadyState(READYSTATE_INTERACTIVE);

  // Transitioning to COMPLETE should load end scripts.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(google_url)).Times(1);

  document_->SetReadyState(READYSTATE_COMPLETE);

  // Now make like a re-navigation.
  document_->SetReadyState(READYSTATE_LOADING);

  // Transitioning back to LOADED should load Css and start scripts again.
  EXPECT_CALL(*handler_, LoadCss(google_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(google_url)).Times(1);

  // But not end scripts.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);
  document_->SetReadyState(READYSTATE_LOADED);

  // Transitioning back to INTERACTIVE should be a no-op.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  document_->SetReadyState(READYSTATE_INTERACTIVE);

  // Transitioning back to COMPLETE should load end scripts.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(google_url)).Times(1);

  document_->SetReadyState(READYSTATE_COMPLETE);

  // Now navigate to a different URL.
  document_->set_url(kSlashdotUrl);
  document_->set_ready_state(READYSTATE_LOADING);

  // Transitioning to loading should not cause any loads.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  handler_->SetUrl(CComBSTR(kSlashdotUrl));
  document_->FireReadyStateChange();

  const GURL slashdot_url(kSlashdotUrl);

  // Transitioning back to LOADED on the new URL should load
  // Css and start scripts again.
  EXPECT_CALL(*handler_, LoadCss(slashdot_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(slashdot_url)).Times(1);

  // But not end scripts.
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);
  document_->SetReadyState(READYSTATE_LOADED);

  // Back to INTERACTIVE is still a noop.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(_)).Times(0);

  document_->SetReadyState(READYSTATE_INTERACTIVE);

  // And COMPLETE loads end scripts again.
  EXPECT_CALL(*handler_, LoadCss(_)).Times(0);
  EXPECT_CALL(*handler_, LoadStartScripts(_)).Times(0);
  EXPECT_CALL(*handler_, LoadEndScripts(slashdot_url)).Times(1);

  document_->SetReadyState(READYSTATE_COMPLETE);
}

TEST_F(FrameEventHandlerTest, InsertCodeCss) {
  CreateHandler();
  MockContentScriptManager* content_script_manager =
      handler_->GetContentScriptManager();

  // Css code type
  EXPECT_CALL(*handler_, GetCodeOrFileContents(_, _, _)).WillOnce(Return(S_OK));
  // Must respond with non-empty extension path or call will get deferred.
  EXPECT_CALL(*host_, GetExtensionId(_)).WillOnce(
      DoAll(SetArgumentPointee<0>(std::wstring(L"hello")), Return(S_OK)));
  EXPECT_CALL(*content_script_manager, InsertCss(_, _)).WillOnce(Return(S_OK));

  ASSERT_HRESULT_SUCCEEDED(
      handler_->InsertCode(NULL, NULL, kCeeeTabCodeTypeCss));

  // Js code type with no file
  EXPECT_CALL(*handler_, GetCodeOrFileContents(_, _, _)).WillOnce(Return(S_OK));

  wchar_t* default_file = L"ExecuteScript.code";
  EXPECT_CALL(*content_script_manager, ExecuteScript(_, StrEq(default_file), _))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(*host_, GetExtensionId(_)).WillOnce(
      DoAll(SetArgumentPointee<0>(std::wstring(L"hello")), Return(S_OK)));

  ASSERT_HRESULT_SUCCEEDED(
      handler_->InsertCode(NULL, NULL, kCeeeTabCodeTypeJs));

  // Js code type with a file
  EXPECT_CALL(*handler_, GetCodeOrFileContents(_, _, _)).WillOnce(Return(S_OK));

  wchar_t* test_file = L"test_file.js";
  EXPECT_CALL(*content_script_manager, ExecuteScript(_, StrEq(test_file), _))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(*host_, GetExtensionId(_)).WillOnce(
      DoAll(SetArgumentPointee<0>(std::wstring(L"hello")), Return(S_OK)));

  CComBSTR test_file_bstr(test_file);
  ASSERT_HRESULT_SUCCEEDED(
      handler_->InsertCode(NULL, test_file_bstr, kCeeeTabCodeTypeJs));
}

TEST_F(FrameEventHandlerTest, DeferInsertCodeCss) {
  CreateHandler();

  // Does not set extension path, so it stays empty.
  EXPECT_CALL(*host_, GetExtensionId(_)).WillRepeatedly(Return(S_OK));

  ASSERT_HRESULT_SUCCEEDED(
      handler_->InsertCode(L"boo", NULL, kCeeeTabCodeTypeCss));
  ASSERT_HRESULT_SUCCEEDED(
      handler_->InsertCode(NULL, L"moo", kCeeeTabCodeTypeJs));
  ASSERT_EQ(2, handler_->deferred_injections().size());

  ASSERT_EQ(L"boo", handler_->deferred_injections().begin()->code);
  ASSERT_EQ(L"", handler_->deferred_injections().begin()->file);
  ASSERT_EQ(kCeeeTabCodeTypeCss,
      handler_->deferred_injections().begin()->type);

  // The ++ syntax is ugly but it's either this or make DeferredInjection
  // a public struct.
  ASSERT_EQ(L"", (++handler_->deferred_injections().begin())->code);
  ASSERT_EQ(L"moo", (++handler_->deferred_injections().begin())->file);
  ASSERT_EQ(kCeeeTabCodeTypeJs,
      (++handler_->deferred_injections().begin())->type);
}

TEST_F(FrameEventHandlerTest, RedoDoneInjections) {
  CreateHandler();
  MockContentScriptManager* content_script_manager =
      handler_->GetContentScriptManager();

  // Expects no calls since nothing to redo.
  handler_->RedoDoneInjections();

  CComBSTR url(L"http://www.google.com/");
  handler_->SetupForRedoDoneInjectionsTest(url);
  GURL match_url(com::ToString(url));

  // Does not set extension path, so it stays empty.
  EXPECT_CALL(*host_, GetExtensionId(_)).WillOnce(Return(S_OK));
  // Will get deferred.
  ASSERT_HRESULT_SUCCEEDED(handler_->InsertCode(L"boo", NULL,
                                                kCeeeTabCodeTypeCss));

  EXPECT_CALL(*handler_, LoadCss(match_url)).Times(1);
  EXPECT_CALL(*handler_, LoadStartScripts(match_url)).Times(1);
  EXPECT_CALL(*handler_, LoadEndScripts(match_url)).Times(1);

  // Expect to get this once, as we deferred it before.
  EXPECT_CALL(*handler_, GetCodeOrFileContents(_, _, _)).WillOnce(Return(S_OK));
  EXPECT_CALL(*host_, GetExtensionId(_)).WillOnce(
      DoAll(SetArgumentPointee<0>(std::wstring(L"hello")), Return(S_OK)));
  EXPECT_CALL(*content_script_manager, InsertCss(_, _)).WillOnce(Return(S_OK));
  ASSERT_HRESULT_SUCCEEDED(
      handler_->InsertCode(L"boo", NULL, kCeeeTabCodeTypeCss));

  EXPECT_CALL(*handler_, GetCodeOrFileContents(_, _, _)).WillOnce(Return(S_OK));
  EXPECT_CALL(*host_, GetExtensionId(_)).WillOnce(
      DoAll(SetArgumentPointee<0>(std::wstring(L"hello")), Return(S_OK)));
  EXPECT_CALL(*content_script_manager, InsertCss(_, _)).WillOnce(Return(S_OK));
  handler_->RedoDoneInjections();
}

TEST_F(FrameEventHandlerTest, GetCodeOrFileContents) {
  CreateHandler();

  CComBSTR code(L"test");
  CComBSTR file(L"test.js");
  CComBSTR empty;
  std::wstring contents;

  // Failure cases.
  EXPECT_CALL(*handler_, GetExtensionResourceContents(_, _)).Times(0);

  ASSERT_HRESULT_FAILED(handler_->CallGetCodeOrFileContents(NULL, NULL,
                                                            &contents));
  ASSERT_HRESULT_FAILED(handler_->CallGetCodeOrFileContents(code, file,
                                                            &contents));
  ASSERT_HRESULT_FAILED(handler_->CallGetCodeOrFileContents(empty, NULL,
                                                            &contents));
  ASSERT_HRESULT_FAILED(handler_->CallGetCodeOrFileContents(NULL, empty,
                                                            &contents));
  ASSERT_HRESULT_FAILED(handler_->CallGetCodeOrFileContents(empty, empty,
                                                            &contents));

  EXPECT_CALL(*handler_, GetExtensionResourceContents(_, _))
      .WillOnce(Return(E_FAIL));

  ASSERT_HRESULT_FAILED(handler_->CallGetCodeOrFileContents(NULL, file,
                                                            &contents));

  // Success cases.
  EXPECT_CALL(*handler_, GetExtensionResourceContents(_, _)).Times(0);

  ASSERT_HRESULT_SUCCEEDED(handler_->CallGetCodeOrFileContents(code, NULL,
                                                               &contents));
  ASSERT_HRESULT_SUCCEEDED(handler_->CallGetCodeOrFileContents(code, empty,
                                                               &contents));

  EXPECT_CALL(*handler_, GetExtensionResourceContents(_, _)).Times(2)
      .WillRepeatedly(Return(S_OK));

  ASSERT_HRESULT_SUCCEEDED(handler_->CallGetCodeOrFileContents(NULL, file,
                                                               &contents));
  ASSERT_HRESULT_SUCCEEDED(handler_->CallGetCodeOrFileContents(empty, file,
                                                               &contents));
}

}  // namespace
