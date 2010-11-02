// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock implementation of ChromeFrameHost.
#ifndef CEEE_IE_TESTING_MOCK_CHROME_FRAME_HOST_H_
#define CEEE_IE_TESTING_MOCK_CHROME_FRAME_HOST_H_

#include <string>
#include "ceee/ie/common/chrome_frame_host.h"
#include "gmock/gmock.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/instance_count_mixin.h"

namespace testing {

class IChromeFrameHostMockImpl : public IChromeFrameHost {
 public:
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, SetAsChromeFrameMaster, void());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetChromeProfileName,
                             void(const wchar_t* chrome_profile_name));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetUrl, HRESULT(BSTR url));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, StartChromeFrame, HRESULT());
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, PostMessage,
                             HRESULT(BSTR message, BSTR target));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, TearDown, HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetEventSink,
                             void(IChromeFrameHostEvents* event_sink));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, InstallExtension,
                             HRESULT(BSTR crx_path));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, LoadExtension,
                             HRESULT(BSTR extension_dir));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, GetEnabledExtensions, HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetExtensionApisToAutomate,
                             HRESULT(BSTR* enabled_functions));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetSessionId, HRESULT(int*));
};

// A mock implementation of ChromeFrameHost.
class MockChromeFrameHost
  : public CComObjectRootEx<CComSingleThreadModel>,
    public InitializingCoClass<StrictMock<MockChromeFrameHost> >,
    public InstanceCountMixin<MockChromeFrameHost>,
    public StrictMock<IChromeFrameHostMockImpl> {
 public:
  BEGIN_COM_MAP(MockChromeFrameHost)
    COM_INTERFACE_ENTRY_IID(IID_IChromeFrameHost, IChromeFrameHost)
  END_COM_MAP()

  HRESULT Initialize() {
    return S_OK;
  }

  HRESULT Initialize(MockChromeFrameHost** self) {
    *self = this;
    return S_OK;
  }
};

}  // namespace testing

#endif  // CEEE_IE_TESTING_MOCK_CHROME_FRAME_HOST_H_
