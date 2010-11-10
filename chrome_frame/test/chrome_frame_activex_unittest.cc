// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for ChromeFrameActivex.

#include "chrome_frame/chrome_frame_activex.h"

#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/mock_com.h"
#include "chrome_frame/test/chrome_tab_mocks.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {

using testing::_;
using testing::AddRef;
using testing::DoAll;
using testing::IOleClientSiteMockImpl;
using testing::IServiceProviderMockImpl;
using testing::MockIChromeFramePrivileged;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

// Just to unhide the static function.
class TestingChromeFrameActivex : public ChromeFrameActivex {
 public:
  static HRESULT TestingShouldShowVersionMismatchDialog(
      bool is_privileged, IOleClientSite* client_site) {
    return ShouldShowVersionMismatchDialog(is_privileged, client_site);
  }
};

class MockChromeFrameClientSite
  : public CComObjectRootEx<CComSingleThreadModel>,
    public StrictMock<IOleClientSiteMockImpl>,
    public StrictMock<IServiceProviderMockImpl> {
 public:
  DECLARE_NOT_AGGREGATABLE(MockChromeFrameClientSite)
  BEGIN_COM_MAP(MockChromeFrameClientSite)
    COM_INTERFACE_ENTRY(IOleClientSite)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT Initialize(MockChromeFrameClientSite** client_site) {
    *client_site = this;
    return S_OK;
  }
};

// http://code.google.com/p/chromium/issues/detail?id=62636
TEST(ChromeFrameActivex, DISABLED_ShouldShowVersionMismatchDialog) {
  // If not privileged, we always show the dialog.
  ASSERT_TRUE(
      TestingChromeFrameActivex::TestingShouldShowVersionMismatchDialog(
          false, NULL));

  MockChromeFrameClientSite* cs_keeper;
  ScopedComPtr<IOleClientSite> cs;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockChromeFrameClientSite>::CreateInitialized(
          &cs_keeper, cs.Receive()));

  MockIChromeFramePrivileged* cfp_keeper;
  ScopedComPtr<IChromeFramePrivileged> cfp;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockIChromeFramePrivileged>::CreateInitialized(
          &cfp_keeper, cfp.Receive()));

  EXPECT_CALL(*cs_keeper, QueryService(SID_ChromeFramePrivileged,
                                       IID_IChromeFramePrivileged, _))
      .WillRepeatedly(DoAll(AddRef(cfp.get()),
                      SetArgumentPointee<2>(static_cast<void*>(cfp)),
                      Return(S_OK)));

  EXPECT_CALL(*cfp_keeper, ShouldShowVersionMismatchDialog())
      .WillOnce(Return(S_OK));
  ASSERT_TRUE(
      TestingChromeFrameActivex::TestingShouldShowVersionMismatchDialog(
          true, cs_keeper));

  EXPECT_CALL(*cfp_keeper, ShouldShowVersionMismatchDialog())
      .WillOnce(Return(S_FALSE));
  ASSERT_FALSE(
      TestingChromeFrameActivex::TestingShouldShowVersionMismatchDialog(
          true, cs_keeper));

  // Also test that we fail safe, showing the dialog unless we got
  // an affirmative do-not-show.
  EXPECT_CALL(*cfp_keeper, ShouldShowVersionMismatchDialog())
      .WillOnce(Return(E_UNEXPECTED));
  ASSERT_TRUE(
      TestingChromeFrameActivex::TestingShouldShowVersionMismatchDialog(
          true, cs_keeper));
}

}  // namespace
