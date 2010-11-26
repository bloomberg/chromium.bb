// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// JS unittests for our extension API bindings.
#include <iostream>

#include "base/path_service.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"

#include "ceee/ie/plugin/scripting/content_script_manager.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "ceee/ie/plugin/scripting/script_host.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/dispex_mocks.h"
#include "ceee/testing/utils/instance_count_mixin.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <initguid.h>  // NOLINT

namespace {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;
using testing::InstanceCountMixin;
using testing::InstanceCountMixinBase;
using testing::IDispatchExMockImpl;

// This is the chromium buildbot extension id.
const wchar_t kExtensionId[] = L"fepbkochiplomghbdfgekenppangbiap";
// And the gmail checker.
const wchar_t kAnotherExtensionId[] = L"kgeddobpkdopccblmihponcjlbdpmbod";
const wchar_t kFileName[] = TEXT(__FILE__);

// The class and macros belwo make it possible to execute and debug JavaScript
// snippets interspersed with C++ code, which is a humane way to write, but
// particularly to read and debug the unittests below.
// To use this, declare a JavaScript block as follows:
// <code>
// BEGIN_SCRIPT_BLOCK(some_identifier) /*
//   window.alert('here I am!'
// */ END_SCRIPT_BLOCK()
//
// HRESULT hr = some_identifier.Execute(script_host);
// </code>
#define BEGIN_SCRIPT_BLOCK(x) ScriptBlock x(TEXT(__FILE__), __LINE__);
#define END_SCRIPT_BLOCK()
class ScriptBlock {
 public:
  ScriptBlock(const wchar_t* file, size_t line) : file_(file), line_(line) {
  }

  HRESULT Execute(IScriptHost* script_host) {
    FilePath self_path(file_);

    if (!self_path.IsAbsolute()) {
      // Construct the absolute path to this source file.
      // The __FILE__ macro may expand to a solution-relative path to the file.
      FilePath src_root;
      EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &src_root));

      self_path = src_root.Append(L"ceee")
                          .Append(L"ie")
                          .Append(file_);
    }

    // Slurp the file.
    std::string contents;
    if (!file_util::ReadFileToString(self_path, &contents))
      return E_UNEXPECTED;

    // Walk the lines to ours.
    std::string::size_type start_pos = 0;
    for (size_t i = 0; i < line_; ++i) {
      // Find the next newline.
      start_pos = contents.find('\n', start_pos);
      if (start_pos == contents.npos)
        return E_UNEXPECTED;

      // Walk past the newline char.
      start_pos++;
    }

    // Now find the next occurrence of END_SCRIPT_BLOCK.
    std::string::size_type end_pos = contents.find("END_SCRIPT_BLOCK",
                                                   start_pos);
    if (end_pos == contents.npos)
      return E_UNEXPECTED;

    // And walk back to the start of that line.
    end_pos = contents.rfind('\n', end_pos);
    if (end_pos == contents.npos)
      return E_UNEXPECTED;

    CComPtr<IDebugDocumentHelper> doc;
    ScriptHost* host = static_cast<ScriptHost*>(script_host);
    host->AddDebugDocument(file_, CA2W(contents.c_str()), &doc);

    std::string script = contents.substr(start_pos, end_pos - start_pos);
    return host->RunScriptSnippet(start_pos, CA2W(script.c_str()), doc);
  }

 private:
  const wchar_t* file_;
  const size_t line_;
};


class TestingContentScriptNativeApi
    : public ContentScriptNativeApi,
      public InstanceCountMixin<TestingContentScriptNativeApi>,
      public InitializingCoClass<TestingContentScriptNativeApi> {
 public:
  // Disambiguate.
  using InitializingCoClass<TestingContentScriptNativeApi>::CreateInitialized;

  HRESULT Initialize(TestingContentScriptNativeApi** self) {
    *self = this;
    return S_OK;
  }

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, Log,
      HRESULT(BSTR level, BSTR message));
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, OpenChannelToExtension,
      HRESULT(BSTR source_id, BSTR target_id, BSTR name, LONG* port_id));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, CloseChannel,
      HRESULT(LONG port_id));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, PortAddRef,
      HRESULT(LONG port_id));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, PortRelease,
      HRESULT(LONG port_id));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, PostMessage,
      HRESULT(LONG port_id, BSTR msg));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, AttachEvent,
      HRESULT(BSTR event_name));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, DetachEvent,
      HRESULT(BSTR event_name));
};

class TestingContentScriptManager
    : public ContentScriptManager {
 public:
  // Make accessible for testing.
  using ContentScriptManager::BootstrapScriptHost;
};

class TestingScriptHost
    : public ScriptHost,
      public InitializingCoClass<TestingScriptHost>,
      public InstanceCountMixin<TestingScriptHost> {
 public:
  using InitializingCoClass<TestingScriptHost>::CreateInitializedIID;

  HRESULT Initialize(ScriptHost::DebugApplication* debug,
                     TestingScriptHost** self) {
    *self = this;
    return ScriptHost::Initialize(debug);
  }
};

class RendererExtensionBindingsTest: public testing::Test {
 public:
  RendererExtensionBindingsTest() : api_(NULL), script_host_(NULL) {
  }

  static void SetUpTestCase() {
    EXPECT_HRESULT_SUCCEEDED(::CoInitialize(NULL));
    debug_.Initialize();
  }

  static void TearDownTestCase() {
    debug_.Terminate();

    ::CoUninitialize();
  }

  void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(::CoInitialize(NULL));

    ASSERT_HRESULT_SUCCEEDED(
        TestingScriptHost::CreateInitializedIID(&debug_,
                                                &script_host_,
                                                IID_IScriptHost,
                                                &script_host_keeper_));
    ASSERT_HRESULT_SUCCEEDED(
        TestingContentScriptNativeApi::CreateInitialized(&api_, &api_keeper_));
  }

  void TearDown() {
    if (api_) {
      EXPECT_HRESULT_SUCCEEDED(api_->TearDown());
      api_ = NULL;
      api_keeper_.Release();
    }

    script_host_ = NULL;
    if (script_host_keeper_ != NULL)
      script_host_keeper_->Close();
    script_host_keeper_.Release();

    EXPECT_EQ(0, InstanceCountMixinBase::all_instance_count());
  }

  void Initialize() {
    ASSERT_HRESULT_SUCCEEDED(
        manager_.BootstrapScriptHost(script_host_keeper_,
                                     api_keeper_,
                                     kExtensionId));
  }

  void AssertNameExists(const wchar_t* name) {
    CComVariant result;
    ASSERT_HRESULT_SUCCEEDED(script_host_->RunExpression(name, &result));
    ASSERT_NE(VT_EMPTY, V_VT(&result));
  }

  void ExpectFirstConnection() {
    EXPECT_CALL(*api_, AttachEvent(StrEq(L"")))
      .WillOnce(Return(S_OK));
  }
  void ExpectConnection(const wchar_t* src_extension_id,
                        const wchar_t* dst_extension_id,
                        const wchar_t* port_name,
                        LONG port_id) {
    EXPECT_CALL(*api_,
        OpenChannelToExtension(StrEq(src_extension_id),
                               StrEq(dst_extension_id),
                               StrEq(port_name),
                               _))
          .WillOnce(
              DoAll(
                  SetArgumentPointee<3>(port_id),
                  Return(S_OK)));

    EXPECT_CALL(*api_, PortAddRef(port_id))
        .WillOnce(Return(S_OK));
  }

 protected:
  TestingContentScriptNativeApi* api_;
  CComPtr<ICeeeContentScriptNativeApi> api_keeper_;
  TestingContentScriptManager manager_;

  TestingScriptHost* script_host_;
  CComPtr<IScriptHost> script_host_keeper_;

  static ScriptHost::DebugApplication debug_;
};

ScriptHost::DebugApplication
    RendererExtensionBindingsTest::debug_(L"RendererExtensionBindingsTest");

TEST_F(RendererExtensionBindingsTest, TestNamespace) {
  Initialize();

  AssertNameExists(L"chrome");
  AssertNameExists(L"chrome.extension");
  AssertNameExists(L"chrome.extension.connect");

  AssertNameExists(L"JSON");
  AssertNameExists(L"JSON.parse");
}

TEST_F(RendererExtensionBindingsTest, GetUrl) {
  Initialize();

  CComVariant result;

  ASSERT_HRESULT_SUCCEEDED(
      script_host_->RunExpression(
          L"chrome.extension.getURL('foo')", &result));

  ASSERT_EQ(VT_BSTR, V_VT(&result));
  ASSERT_STREQ(StringPrintf(L"chrome-extension://%ls/foo",
                            kExtensionId).c_str(),
               V_BSTR(&result));
}

TEST_F(RendererExtensionBindingsTest, PortConnectDisconnect) {
  Initialize();
  const LONG kPortId = 42;
  ExpectConnection(kExtensionId, kExtensionId, L"", kPortId);
  ExpectFirstConnection();

  EXPECT_HRESULT_SUCCEEDED(script_host_->RunScript(
      kFileName, L"port = chrome.extension.connect()"));
}

TEST_F(RendererExtensionBindingsTest, PortConnectWithName) {
  Initialize();
  const LONG kPortId = 42;
  const wchar_t* kPortName = L"A Port Name";
  ExpectConnection(kExtensionId, kExtensionId, kPortName, kPortId);
  ExpectFirstConnection();

  EXPECT_HRESULT_SUCCEEDED(script_host_->RunScript(
    kFileName, StringPrintf(
        L"port = chrome.extension.connect({name: \"%ls\"});",
        kPortName).c_str()));
}

TEST_F(RendererExtensionBindingsTest, PortConnectToExtension) {
  Initialize();
  const LONG kPortId = 42;
  ExpectConnection(kExtensionId, kAnotherExtensionId, L"", kPortId);
  ExpectFirstConnection();

  EXPECT_HRESULT_SUCCEEDED(script_host_->RunScript(
    kFileName, StringPrintf(L"port = chrome.extension.connect(\"%ls\");",
                            kAnotherExtensionId).c_str()));
}

TEST_F(RendererExtensionBindingsTest, PostMessage) {
  Initialize();
  const LONG kPortId = 42;
  ExpectConnection(kExtensionId, kAnotherExtensionId, L"", kPortId);
  ExpectFirstConnection();

  EXPECT_HRESULT_SUCCEEDED(script_host_->RunScript(
      kFileName, StringPrintf(L"port = chrome.extension.connect(\"%ls\");",
                              kAnotherExtensionId).c_str()));

  const wchar_t* kMsg = L"Message in a bottle, yeah!";
  // Note the extra on-the-wire quotes, due to JSON encoding the input.
  EXPECT_CALL(*api_,
      PostMessage(kPortId,
                  StrEq(StringPrintf(L"\"%ls\"", kMsg).c_str())));

  EXPECT_HRESULT_SUCCEEDED(script_host_->RunScript(
      kFileName, StringPrintf(L"port.postMessage(\"%ls\");", kMsg).c_str()));
}

TEST_F(RendererExtensionBindingsTest, OnConnect) {
  Initialize();
  const LONG kPortId = 42;
  const wchar_t kPortName[] = L"A port of call";
  BEGIN_SCRIPT_BLOCK(script) /*
    function onConnect(port) {
      if (port.name == 'A port of call')
        console.log('SUCCESS');
      else
        console.log(port.name);
    };
    chrome.extension.onConnect.addListener(onConnect);
  */ END_SCRIPT_BLOCK()

  EXPECT_CALL(*api_, AttachEvent(StrEq(L""))).
      WillOnce(Return(S_OK));

  EXPECT_HRESULT_SUCCEEDED(script.Execute(script_host_));

  // A 'SUCCESS' log signals success.
  EXPECT_CALL(*api_, Log(StrEq(L"info"), StrEq(L"SUCCESS"))).Times(1);

  EXPECT_CALL(*api_, AttachEvent(StrEq(L""))).
      WillOnce(Return(S_OK));
  EXPECT_CALL(*api_, PortAddRef(kPortId)).
      WillOnce(Return(S_OK));

  EXPECT_HRESULT_SUCCEEDED(
      api_->CallOnPortConnect(kPortId,
                              kPortName,
                              L"",
                              kExtensionId,
                              kExtensionId));
}

TEST_F(RendererExtensionBindingsTest, OnDisconnect) {
  Initialize();
  const LONG kPortId = 42;
  ExpectConnection(kExtensionId, kExtensionId, L"", kPortId);
  ExpectFirstConnection();

  BEGIN_SCRIPT_BLOCK(script1) /*
    var port = chrome.extension.connect()
  */ END_SCRIPT_BLOCK()
  EXPECT_HRESULT_SUCCEEDED(script1.Execute(script_host_));

  BEGIN_SCRIPT_BLOCK(script2) /*
    port.onDisconnect.addListener(function (port) {
      console.log('SUCCESS');
    });
  */ END_SCRIPT_BLOCK()

  EXPECT_CALL(*api_, AttachEvent(StrEq(L"")))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(script2.Execute(script_host_));

  // A 'SUCCESS' log signals success.
  EXPECT_CALL(*api_, Log(StrEq(L"info"), StrEq(L"SUCCESS"))).Times(1);
  EXPECT_CALL(*api_, CloseChannel(kPortId));

  EXPECT_HRESULT_SUCCEEDED(api_->CallOnPortDisconnect(kPortId));
}

TEST_F(RendererExtensionBindingsTest, OnMessage) {
  Initialize();
  const LONG kPortId = 42;
  ExpectConnection(kExtensionId, kExtensionId, L"", kPortId);
  ExpectFirstConnection();

  BEGIN_SCRIPT_BLOCK(connect_script) /*
    // Connect to our extension.
    var port = chrome.extension.connect();
  */ END_SCRIPT_BLOCK()
  EXPECT_HRESULT_SUCCEEDED(connect_script.Execute(script_host_));

  BEGIN_SCRIPT_BLOCK(add_listener_script) /*
    // Log the received message to console.
    function onMessage(msg, port) {
      console.log(msg);
    };
    port.onMessage.addListener(onMessage);
  */ END_SCRIPT_BLOCK()

  EXPECT_CALL(*api_, AttachEvent(StrEq(L"")))
      .WillOnce(Return(S_OK));

  EXPECT_HRESULT_SUCCEEDED(add_listener_script.Execute(script_host_));

  const wchar_t kMessage[] = L"A message in a bottle, yeah!";
  // The message logged signals success.
  EXPECT_CALL(*api_, Log(StrEq(L"info"), StrEq(kMessage))).Times(1);

  EXPECT_HRESULT_SUCCEEDED(
      api_->CallOnPortMessage(StringPrintf(L"\"%ls\"", kMessage).c_str(),
                              kPortId));
}

TEST_F(RendererExtensionBindingsTest, OnLoad) {
  Initialize();

  BEGIN_SCRIPT_BLOCK(script) /*
    var chromeHidden = ceee.GetChromeHidden();
    function onLoad(extension_id) {
      console.log(extension_id);
    }
    chromeHidden.onLoad.addListener(onLoad);
  */ END_SCRIPT_BLOCK()

  EXPECT_CALL(*api_, AttachEvent(StrEq(L"")));
  EXPECT_HRESULT_SUCCEEDED(script.Execute(script_host_));

  EXPECT_CALL(*api_, Log(StrEq(L"info"), StrEq(kExtensionId)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(api_->CallOnLoad(kExtensionId));
}

TEST_F(RendererExtensionBindingsTest, OnUnload) {
  Initialize();
  const LONG kPort1Id = 42;
  const LONG kPort2Id = 57;
  ExpectConnection(kExtensionId, kExtensionId, L"port1", kPort1Id);
  ExpectConnection(kExtensionId, kExtensionId, L"port2", kPort2Id);
  ExpectFirstConnection();

  BEGIN_SCRIPT_BLOCK(script) /*
    var port1 = chrome.extension.connect({name: 'port1'});
    var port2 = chrome.extension.connect({name: 'port2'});
  */ END_SCRIPT_BLOCK()

  EXPECT_HRESULT_SUCCEEDED(script.Execute(script_host_));

  EXPECT_CALL(*api_, PortRelease(kPort1Id)).WillOnce(Return(S_OK));
  EXPECT_CALL(*api_, PortRelease(kPort2Id)).WillOnce(Return(S_OK));
  EXPECT_CALL(*api_, DetachEvent(StrEq(L""))).WillOnce(Return(S_OK));

  EXPECT_HRESULT_SUCCEEDED(api_->CallOnUnload());
}

}  // namespace
