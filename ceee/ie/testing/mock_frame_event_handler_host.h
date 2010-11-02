// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock implementation of ChromeFrameHost.
#ifndef CEEE_IE_TESTING_MOCK_FRAME_EVENT_HANDLER_HOST_H_
#define CEEE_IE_TESTING_MOCK_FRAME_EVENT_HANDLER_HOST_H_

#include <string>

#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/plugin/bho/frame_event_handler.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "gmock/gmock.h"

namespace testing {

class IFrameEventHandlerHostMockImpl : public IFrameEventHandlerHost {
 public:
  MOCK_METHOD3(AttachBrowser, HRESULT(IWebBrowser2* browser,
                                      IWebBrowser2* parent_browser,
                                      IFrameEventHandler* handler));
  MOCK_METHOD3(DetachBrowser, HRESULT(IWebBrowser2* browser,
                                      IWebBrowser2* parent_browser,
                                      IFrameEventHandler* handler));
  MOCK_METHOD1(GetTopLevelBrowser, HRESULT(IWebBrowser2** browser));
  MOCK_METHOD1(GetReadyState, HRESULT(READYSTATE* ready_state));
  MOCK_METHOD1(OnReadyStateChanged, HRESULT(READYSTATE ready_state));
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

class IExtensionPortMessagingProviderMockImpl
    : public IExtensionPortMessagingProvider {
 public:
  MOCK_METHOD1(CloseAll, void(IContentScriptNativeApi* instance));
  MOCK_METHOD4(OpenChannelToExtension,
      HRESULT(IContentScriptNativeApi* instance,
              const std::string& extension,
              const std::string& channel_name,
              int cookie));
  MOCK_METHOD2(PostMessage, HRESULT(int port_id, const std::string& message));
};

template<class BaseClass>
class MockFrameEventHandlerHostBase
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<BaseClass>,
      public InstanceCountMixin<BaseClass>,
      public StrictMock<IFrameEventHandlerHostMockImpl>,
      public StrictMock<IExtensionPortMessagingProviderMockImpl> {
 public:
  BEGIN_COM_MAP(MockFrameEventHandlerHostBase)
    COM_INTERFACE_ENTRY_IID(IID_IFrameEventHandlerHost, IFrameEventHandlerHost)
  END_COM_MAP()
};

class MockFrameEventHandlerHost
    : public MockFrameEventHandlerHostBase<MockFrameEventHandlerHost> {
 public:
  HRESULT Initialize(MockFrameEventHandlerHost** self) {
    *self = this;
    return S_OK;
  }
};

}  // namespace testing

#endif  // CEEE_IE_TESTING_MOCK_FRAME_EVENT_HANDLER_HOST_H_
