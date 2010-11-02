// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ITabWindow and ITabWindowManager interfaces and approaches
// related to them.

#ifndef CEEE_IE_COMMON_IE_TAB_INTERFACES_H_
#define CEEE_IE_COMMON_IE_TAB_INTERFACES_H_

#include <atlbase.h>
#include <exdisp.h>  // IWebBrowser2
#include <guiddef.h>  // DEFINE_GUID
#include <mshtml.h>  // IHTMLDocument2

// Service ID to get the tab window manager.
// {122F0301-9AB9-4CBE-B5F6-CEADCF6AA9B7}
DEFINE_GUID(SID_STabWindowManager,
            0x122F0301L,
            0x9AB9,
            0x4CBE,
            0xB5, 0xF6, 0xCE, 0xAD, 0xCF, 0x6A, 0xA9, 0xB7);

// Adapted from documentation available at
// http://www.geoffchappell.com/viewer.htm?doc=studies/windows/ie/ieframe/interfaces/itabwindowmanager.htm
//
// Available in IE7, IE8 and IE9. Retrieve by QueryService for
// SID_STabWindowManager on the IServiceProvider that is the
// IDropTarget you can retrieve from the browser frame window.
//
// The ITabWindowManager interface is NOT the same in IE7, IE8 and
// IE9, and has different IIDs in each.

interface __declspec(uuid("CAE57FE7-5E06-4804-A285-A985E76708CD"))
    ITabWindowManagerIe7 : IUnknown {
  STDMETHOD(AddTab)(LPCITEMIDLIST pidl, UINT, ULONG, long*) PURE;
  STDMETHOD(SelectTab)(long tab) PURE;
  STDMETHOD(CloseAllTabs)() PURE;
  // Actually ITabWindow**
  STDMETHOD(GetActiveTab)(IUnknown** active_tab) PURE;
  STDMETHOD(GetCount)(long* count) PURE;
  // Actually ITabWindow**
  STDMETHOD(GetItem)(long index, IUnknown** tab_window) PURE;
  STDMETHOD(IndexFromID)(long id, long* index) PURE;
  // Window class of window should be "TabWindowClass"
  STDMETHOD(IndexFromHWND)(HWND window, long* index) PURE;
  // Actually IBrowserFrame**
  STDMETHOD(GetBrowserFrame)(IUnknown** browser_frame) PURE;
  STDMETHOD(AddBlankTab)(unsigned long ul, long* l) PURE;
  STDMETHOD(AddTabGroup)(LPCITEMIDLIST* pidl, long l, unsigned long ul) PURE;
  STDMETHOD(GetCurrentTabGroup)(LPCITEMIDLIST** pidl, long* l1, long* l2) PURE;
  STDMETHOD(OpenHomePages)(int flags) PURE;
  // @param moving_id ID (as in ITabWindow::GetID) of the tab being moved.
  // @param dest_id ID of the tab currently in the desired destination position.
  STDMETHOD(RepositionTab)(long moving_id, long dest_id, int) PURE;
};

interface __declspec(uuid("9706DA66-D17C-48a5-B42D-39963D174DC0"))
    ITabWindowManagerIe8 : IUnknown {
  STDMETHOD(AddTab)(LPCITEMIDLIST pidl, UINT, ULONG, long*) PURE;
  STDMETHOD(_AddTabByPosition)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(SelectTab)(long) PURE;
  STDMETHOD(CloseAllTabs)() PURE;
  // Actually ITabWindow**
  STDMETHOD(GetActiveTab)(IUnknown** active_tab) PURE;
  STDMETHOD(GetCount)(long* count) PURE;
  STDMETHOD(_GetFilteredCount)(void * UNKNOWN_ARGUMENTS) PURE;
  // Actually ITabWindow**
  STDMETHOD(GetItem)(long index, IUnknown** tab_window) PURE;
  STDMETHOD(IndexFromID)(long id, long* index) PURE;
  STDMETHOD(_FilteredIndexFromID)(void * UNKNOWN_ARGUMENTS) PURE;
  // Window class of window should be "TabWindowClass"
  STDMETHOD(IndexFromHWND)(HWND window, long* index) PURE;
  // Actually IBrowserFrame**
  STDMETHOD(GetBrowserFrame)(IUnknown** browser_frame) PURE;
  STDMETHOD(AddBlankTab)(unsigned long, long*) PURE;
  STDMETHOD(_AddBlankTabEx)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(AddTabGroup)(LPCITEMIDLIST* pidl, long, unsigned long) PURE;
  STDMETHOD(GetCurrentTabGroup)(LPCITEMIDLIST** pidl, long*, long*) PURE;
  STDMETHOD(OpenHomePages)(int flags) PURE;
  // @param moving_id ID (as in ITabWindow::GetID) of the tab being moved.
  // @param dest_id ID of the tab currently in the desired destination position.
  STDMETHOD(RepositionTab)(long moving_id, long dest_id, int) PURE;
};

// In IE9 IID and definition of the interface has changed.
interface __declspec(uuid("8059E123-28D5-4C75-A298-664B3720ACAE"))
    ITabWindowManagerIe9 : IUnknown {
  STDMETHOD(AddTab)(LPCITEMIDLIST pidl, UINT, ULONG, IUnknown*, long*) PURE;
  STDMETHOD(AddTabByPosition)(LPCITEMIDLIST pidl, UINT, ULONG, long,
                              IUnknown*, DWORD*, long*) PURE;
  STDMETHOD(SelectTab)(long) PURE;
  STDMETHOD(CloseAllTabs)(void) PURE;
  // Actually ITabWindow**
  STDMETHOD(GetActiveTab)(IUnknown**) PURE;
  STDMETHOD(GetCount)(long*) PURE;
  STDMETHOD(GetFilteredCount)(long*, long) PURE;
  STDMETHOD(GetItem)(long index, IUnknown** tab_window) PURE;
  STDMETHOD(IndexFromID)(long id, long* index) PURE;
  STDMETHOD(FilteredIndexFromID)(long, long, long*) PURE;
  STDMETHOD(IndexFromHWND)(HWND window, long* index) PURE;
  // Actual IBrowserFrame **
  STDMETHOD(GetBrowserFrame)(IUnknown**) PURE;
  STDMETHOD(AddBlankTab)(ULONG, long*) PURE;
  STDMETHOD(AddBlankTabEx)(ULONG, DWORD*, long*) PURE;
  STDMETHOD(AddTabGroup)(DWORD, long, ULONG) PURE;
  STDMETHOD(GetCurrentTabGroup)(DWORD, long*, long*) PURE;
  STDMETHOD(OpenHomePages)(int) PURE;
  STDMETHOD(RepositionTab)(long, long, int) PURE;
  // Actually IClosedTabManager**
  STDMETHOD(GetUndoTabManager)(IUnknown**) PURE;
  STDMETHOD(GetRestoreTabManager)(IUnknown**) PURE;
  // Actually, ITabWindowEvents* (a new interface)
  STDMETHOD(AddTabWindowEventHandler)(IUnknown*) PURE;
  // Actually, ITabWindowEvents* (a new interface)
  STDMETHOD(UnregisterTabWindowEventHandler)(IUnknown*) PURE;
  STDMETHOD(CloseTabGroup)(long) PURE;
  STDMETHOD(CreateGroupMapping)(long*) PURE;
  STDMETHOD(DestroyGroupMapping)(long) PURE;
  STDMETHOD(SetDecorationPreference)(DWORD, DWORD*) PURE;
  STDMETHOD(FindTabAdjacentToGroup)(long, long,
                                    DWORD, IUnknown**, long*) PURE;
  STDMETHOD(GetNewGroupID)(long*) PURE;
  STDMETHOD(CloseOldTabIfFailed)(void) PURE;
  STDMETHOD(CloseAllTabsExcept)(long) PURE;
  STDMETHOD(CloseAllTabsExceptActive)(void) PURE;
};

// Adapted from documentation available at
// http://www.geoffchappell.com/viewer.htm?doc=studies/windows/ie/ieframe/interfaces/itabwindow.htm
//
// Also available differently in IE7 and IE8.
interface __declspec(uuid("9BAB3405-EE3F-4040-8836-25AA9C2D408E"))
    ITabWindowIe7 : IUnknown {
  STDMETHOD(GetID)(long* id) PURE;
  STDMETHOD(Close)() PURE;
  STDMETHOD(AsyncExec)(REFGUID cmd_group, DWORD cmd_id, DWORD exec_opt,
                       VARIANT* in_args, VARIANT* out_args) PURE;
  STDMETHOD(GetTabWindowManager)(ITabWindowManagerIe7** tab_manager) PURE;
  STDMETHOD(OnBrowserCreated)(int, int, int, int, int, void*) PURE;
  STDMETHOD(OnNewWindow)(ULONG, IDispatch**) PURE;
  STDMETHOD(OnBrowserClosed)() PURE;
  // Actually enum tagTAB_ATTENTION_STATE
  STDMETHOD(OnRequestAttention)(int i) PURE;
  STDMETHOD(FrameTranslateAccelerator)(MSG* msg, ULONG) PURE;
  STDMETHOD(SetTitle)(LPCWSTR title, int title_length) PURE;
  STDMETHOD(SetIcon)(HICON, int) PURE;
  STDMETHOD(SetStatusBarState)(int, long) PURE;
  STDMETHOD(GetTitle)(LPWSTR title, ULONG, int) PURE;
  STDMETHOD(GetIcon)(HICON* icon, int*, int) PURE;
  STDMETHOD(GetLocationPidl)(LPCITEMIDLIST* pidl) PURE;
  STDMETHOD(GetNavigationState)(ULONG* state) PURE;
  // First param is enum tagNAVIGATION_BAND_PROGRESS_STATE*
  STDMETHOD(GetProgress)(int*, long*, long*) PURE;
  STDMETHOD(GetFlags)(ULONG* flags) PURE;
  STDMETHOD(GetBrowser)(IDispatch** browser) PURE;
  STDMETHOD(GetBrowserToolbarWindow)(HWND* window) PURE;
  // Actually enum tagSEARCH_BAND_SEARCH_STATE*
  STDMETHOD(GetSearchState)(int* state) PURE;
  // Actually enum tagTAB_ATTENTION_STATE*
  STDMETHOD(GetAttentionState)(int* state) PURE;
  STDMETHOD(ResampleImageAsync)() PURE;
  STDMETHOD(OnTabImageResampled)(HBITMAP bitmap) PURE;
  STDMETHOD(GetStatusBarState)(int* bar, long* state) PURE;
};

interface __declspec(uuid("FF18630E-5B18-4A07-8A75-9FD3CE5A2D14"))
    ITabWindowIe8 : IUnknown {
  STDMETHOD(GetID)(long* id) PURE;
  STDMETHOD(Close)() PURE;
  STDMETHOD(AsyncExec)(REFGUID cmd_group, DWORD cmd_id, DWORD exec_opt,
                       VARIANT* in_args, VARIANT* out_args) PURE;
  STDMETHOD(GetTabWindowManager)(ITabWindowManagerIe8** tab_manager) PURE;
  STDMETHOD(SetBrowserWindowParent)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(OnBrowserCreated)(int, int, int, int, int, void*) PURE;
  STDMETHOD(OnNewWindow)(ULONG, IDispatch**) PURE;
  STDMETHOD(OnBrowserClosed)() PURE;
  // Actually enum tagTAB_ATTENTION_STATE
  STDMETHOD(OnRequestAttention)(int i) PURE;
  STDMETHOD(FrameTranslateAccelerator)(MSG* msg, ULONG) PURE;
  STDMETHOD(SetTitle)(LPCWSTR title, int title_length) PURE;
  // REMOVED from IE7 version: STDMETHOD(SetIcon)(HICON, int) PURE;
  STDMETHOD(SetStatusBarState)(int, long) PURE;

  STDMETHOD(GetTitle)(LPWSTR title, ULONG, int) PURE;
  STDMETHOD(GetIcon)(HICON* icon, int*, int) PURE;
  STDMETHOD(GetLocationPidl)(LPCITEMIDLIST* pidl) PURE;
  STDMETHOD(GetLocationUri)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(GetNavigationState)(ULONG* state) PURE;
  // First param is enum tagNAVIGATION_BAND_PROGRESS_STATE*
  STDMETHOD(GetProgress)(int*, long*, long*) PURE;
  STDMETHOD(GetFlags)(ULONG* flags) PURE;
  STDMETHOD(GetBrowser)(IDispatch** browser) PURE;
  STDMETHOD(GetBrowserToolbarWindow)(HWND* window) PURE;
  // Actually enum tagSEARCH_BAND_SEARCH_STATE*
  STDMETHOD(GetSearchState)(int* state) PURE;
  // Actually enum tagTAB_ATTENTION_STATE*
  STDMETHOD(GetAttentionState)(int* state) PURE;
  STDMETHOD(ResampleImageAsync)() PURE;
  STDMETHOD(OnTabImageResampled)(HBITMAP bitmap) PURE;
  STDMETHOD(GetStatusBarState)(int* bar, long* state) PURE;
};

// New version that was introduced between versions 8.0.6001.18928 and
// 8.0.7600.16385.
interface __declspec(uuid("F704B7E0-4760-46ff-BBDB-7439E0A2A814"))
    ITabWindowIe8_1 : IUnknown {
  STDMETHOD(GetID)(long* id) PURE;
  STDMETHOD(Close)() PURE;
  STDMETHOD(AsyncExec)(REFGUID cmd_group, DWORD cmd_id, DWORD exec_opt,
                       VARIANT* in_args, VARIANT* out_args) PURE;
  STDMETHOD(GetTabWindowManager)(ITabWindowManagerIe8** tab_manager) PURE;
  STDMETHOD(SetBrowserWindowParent)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(OnBrowserCreated)(int, int, int, int, int, void*) PURE;
  STDMETHOD(OnNewWindow)(ULONG, IDispatch**) PURE;
  STDMETHOD(OnBrowserClosed)() PURE;
  // Actually enum tagTAB_ATTENTION_STATE
  STDMETHOD(OnRequestAttention)(int i) PURE;
  STDMETHOD(FrameTranslateAccelerator)(MSG* msg, ULONG) PURE;
  STDMETHOD(SetTitle)(LPCWSTR title, int title_length) PURE;
  // REMOVED from IE7 version: STDMETHOD(SetIcon)(HICON, int) PURE;
  STDMETHOD(SetStatusBarState)(int, long) PURE;
  STDMETHOD(GetTitle)(LPWSTR title, ULONG, int) PURE;
  STDMETHOD(GetIcon)(HICON* icon, int*, int) PURE;
  STDMETHOD(GetIconWeakReference)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(GetLocationPidl)(LPCITEMIDLIST* pidl) PURE;
  STDMETHOD(GetLocationUri)(void * UNKNOWN_ARGUMENTS) PURE;
  STDMETHOD(GetNavigationState)(ULONG* state) PURE;
  // First param is enum tagNAVIGATION_BAND_PROGRESS_STATE*
  STDMETHOD(GetProgress)(int*, long*, long*) PURE;
  STDMETHOD(GetFlags)(ULONG* flags) PURE;
  STDMETHOD(GetBrowser)(IDispatch** browser) PURE;
  STDMETHOD(GetBrowserToolbarWindow)(HWND* window) PURE;
  // Actually enum tagSEARCH_BAND_SEARCH_STATE*
  STDMETHOD(GetSearchState)(int* state) PURE;
  // Actually enum tagTAB_ATTENTION_STATE*
  STDMETHOD(GetAttentionState)(int* state) PURE;
  STDMETHOD(ResampleImageAsync)() PURE;
  STDMETHOD(OnTabImageResampled)(HBITMAP bitmap) PURE;
  STDMETHOD(GetStatusBarState)(int* bar, long* state) PURE;
};

// Modified interface which appeared in IE9 beta.
interface __declspec(uuid("3927961B-9DB0-4174-B67A-39F34585A692"))
    ITabWindowIe9 : IUnknown {
  STDMETHOD(GetID)(long* id) PURE;
  STDMETHOD(Close)() PURE;
  STDMETHOD(AsyncExec)(GUID *, ULONG, ULONG, VARIANT *) PURE;
  STDMETHOD(GetTabWindowManager)(ITabWindowManagerIe8** tab_manager) PURE;
  STDMETHOD(SetBrowserWindowParent)(HWND) PURE;
  STDMETHOD(OnBrowserCreated)(HWND, HWND, HWND, IDispatch*,
                              DWORD*, long) PURE;
  STDMETHOD(OnNewWindow)(ULONG, ULONG, IDispatch**) PURE;
  STDMETHOD(OnBrowserClosed)(void) PURE;
  // Actually enum tagTAB_ATTENTION_STATE
  STDMETHOD(OnRequestAttention)(int i) PURE;
  STDMETHOD(FrameTranslateAccelerator)(MSG* msg, ULONG) PURE;
  STDMETHOD(SetTitle)(LPCWSTR title, int title_length) PURE;
  STDMETHOD(SetStatusBarState)(int, long) PURE;
  STDMETHOD(SetITBarHolderWindow)(HWND) PURE;
  STDMETHOD(EnsureITBar)(int, int) PURE;
  STDMETHOD(GetTitle)(LPWSTR title, ULONG, int) PURE;
  STDMETHOD(GetIcon)(HICON* icon, int*, int) PURE;
  STDMETHOD(GetIconWeakReference)(HICON**) PURE;
  STDMETHOD(GetLocationPidl)(LPCITEMIDLIST* pidl) PURE;
  STDMETHOD(GetLocationUri)(IUnknown**) PURE;
  STDMETHOD(GetNavigationState)(ULONG* state) PURE;
  // First param is enum tagNAVIGATION_BAND_PROGRESS_STATE*
  STDMETHOD(GetProgress)(int*, long*, long*) PURE;
  STDMETHOD(GetFlags)(ULONG* flags) PURE;
  STDMETHOD(GetBrowser)(IDispatch** browser) PURE;
  STDMETHOD(GetParentComponentHandle)(HWND* window) PURE;
  // Actually enum tagSEARCH_BAND_SEARCH_STATE*
  STDMETHOD(GetSearchState)(int* state) PURE;
  // Actually enum tagTAB_ATTENTION_STATE*
  STDMETHOD(GetAttentionState)(int* state) PURE;
  STDMETHOD(ResampleImageAsync)() PURE;
  STDMETHOD(OnTabImageResampled)(IStream *) PURE;
  STDMETHOD(GetStatusBarState)(int* bar, long* state) PURE;
  // Resolved first time here, but existed in earlier versions.
  STDMETHOD(GetThumbnailWindow)(HWND**) PURE;
  STDMETHOD(GetBrowserWindow)(HWND**) PURE;
  STDMETHOD(TransferRecoveryDataForSiteMode)(void) PURE;
  STDMETHOD(GetTabGroup)(long*) PURE;
  STDMETHOD(GetTabGroupDecoration)(DWORD*) PURE;
  STDMETHOD(JoinTabGroup)(long, long) PURE;
  STDMETHOD(LeaveTabGroup)(void) PURE;
  STDMETHOD(IsParticipatingInTabGroup)(int*) PURE;
  STDMETHOD(IsWaitingForGroupRecovery)(int*) PURE;
  STDMETHOD(SetWaitingForGroupRecovery)(long, int) PURE;
  STDMETHOD(BrowserTabIsHung)(void) PURE;
  STDMETHOD(FrameTabWillNotHang)(ULONG, ULONG) PURE;
  STDMETHOD(BrowserTabRespondsNow)(ULONG, int, int) PURE;
  STDMETHOD(BrowserTabRespondsNow_SetHungAsync)(ULONG, ULONG) PURE;
  STDMETHOD(BrowserTabIsPresumedResponsive)(void) PURE;
  STDMETHOD(RecoverHungTab)(void) PURE;
  STDMETHOD(Duplicate)(void) PURE;
  STDMETHOD(ResetBrowserLCIEProxy)(IDispatch*) PURE;
  STDMETHOD(SetPendingUrl)(LPCWSTR) PURE;
};

namespace ie_tab_interfaces {

// Retrieves the requested tab manager interface for the specified IEFrame
// window.
//
// @param ie_frame The top-level frame window you wish to manage.
// @param riid The identifier of the requested interface.
// @param manager Returns the IE7 tab window manager on success.
HRESULT TabWindowManagerFromFrame(HWND ie_frame, REFIID riid, void** manager);

}  // namespace ie_tab_interfaces

#endif  // CEEE_IE_COMMON_IE_TAB_INTERFACES_H_
