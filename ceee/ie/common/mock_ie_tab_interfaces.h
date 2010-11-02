// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ie_tab_interfaces mocks.

#ifndef CEEE_IE_COMMON_MOCK_IE_TAB_INTERFACES_H_
#define CEEE_IE_COMMON_MOCK_IE_TAB_INTERFACES_H_

#include <atlbase.h>
#include <atlcom.h>

#include "ceee/ie/common/ie_tab_interfaces.h"
#include "ceee/testing/utils/mock_static.h"

namespace testing {

MOCK_STATIC_CLASS_BEGIN(MockIeTabInterfaces)
  MOCK_STATIC_INIT_BEGIN(MockIeTabInterfaces)
    MOCK_STATIC_INIT2(ie_tab_interfaces::TabWindowManagerFromFrame,
                      TabWindowManagerFromFrame)
  MOCK_STATIC_INIT_END()

  MOCK_STATIC3(HRESULT, , TabWindowManagerFromFrame, HWND, REFIID, void **)
MOCK_STATIC_CLASS_END(MockIeTabInterfaces)

class MockITabWindowManagerIe7
  : public CComObjectRootEx<CComSingleThreadModel>,
    public ITabWindowManagerIe7 {
 public:
  DECLARE_NOT_AGGREGATABLE(MockITabWindowManagerIe7)
  BEGIN_COM_MAP(MockITabWindowManagerIe7)
    COM_INTERFACE_ENTRY(ITabWindowManagerIe7)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, AddTab,
                             HRESULT(LPCITEMIDLIST, UINT, ULONG, long*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SelectTab, HRESULT(long));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, CloseAllTabs, HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetActiveTab, HRESULT(IUnknown**));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetCount, HRESULT(long*));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, GetItem, HRESULT(long, IUnknown**));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, IndexFromID, HRESULT(long, long*));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, IndexFromHWND, HRESULT(HWND, long*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetBrowserFrame, HRESULT(IUnknown**));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, AddBlankTab,
                             HRESULT(unsigned long, long*));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, AddTabGroup,
                             HRESULT(LPCITEMIDLIST*, long, unsigned long));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetCurrentTabGroup,
                             HRESULT(LPCITEMIDLIST**, long*, long*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OpenHomePages, HRESULT(int));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, RepositionTab,
                             HRESULT(long, long, int));
};

class MockITabWindowIe7
  : public CComObjectRootEx<CComSingleThreadModel>,
    public ITabWindowIe7 {
 public:
  DECLARE_NOT_AGGREGATABLE(MockITabWindowIe7)
  BEGIN_COM_MAP(MockITabWindowIe7)
    COM_INTERFACE_ENTRY(ITabWindowIe7)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetID, HRESULT(long*));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Close, HRESULT());
  MOCK_METHOD5_WITH_CALLTYPE(__stdcall, AsyncExec,
      HRESULT(REFGUID, DWORD, DWORD, VARIANT*, VARIANT*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetTabWindowManager,
                             HRESULT(ITabWindowManagerIe7**));
  MOCK_METHOD6_WITH_CALLTYPE(__stdcall, OnBrowserCreated,
                             HRESULT(int, int, int, int, int, void*));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnNewWindow,
                             HRESULT(ULONG, IDispatch**));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, OnBrowserClosed, HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnRequestAttention, HRESULT(int));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, FrameTranslateAccelerator,
                             HRESULT(MSG*, ULONG));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, SetTitle, HRESULT(LPCWSTR, int));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, SetIcon, HRESULT(HICON, int));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, SetStatusBarState, HRESULT(int, long));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetTitle, HRESULT(LPWSTR, ULONG, int));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetIcon, HRESULT(HICON*, int*, int));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetLocationPidl,
                             HRESULT(LPCITEMIDLIST*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetNavigationState, HRESULT(ULONG*));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetProgress,
                             HRESULT(int*, long*, long*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetFlags, HRESULT(ULONG*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetBrowser, HRESULT(IDispatch**));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetBrowserToolbarWindow,
                             HRESULT(HWND*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetSearchState, HRESULT(int*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetAttentionState, HRESULT(int*));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, ResampleImageAsync, HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnTabImageResampled, HRESULT(HBITMAP));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, GetStatusBarState,
                             HRESULT(int*, long*));
};

}  // namespace testing

#endif  // CEEE_IE_COMMON_MOCK_IE_TAB_INTERFACES_H_
