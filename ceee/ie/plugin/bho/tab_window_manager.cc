// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ceee/ie/plugin/bho/tab_window_manager.h"

#include "base/logging.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/common/window_utils.h"
#include "ceee/ie/common/ie_tab_interfaces.h"

namespace {

// Adapter for tab-window interfaces on IE7/IE8.
template <class IeTabWindow>
class TabWindowAdapter : public TabWindow {
 public:
  explicit TabWindowAdapter(IeTabWindow* tab_window)
      : tab_window_(tab_window) {
  }

  STDMETHOD(GetBrowser)(IDispatch** browser) {
    DCHECK(tab_window_ != NULL);
    DCHECK(browser != NULL);
    DCHECK(*browser == NULL);
    return tab_window_->GetBrowser(browser);
  }

  STDMETHOD(GetID)(long* id) {
    DCHECK(tab_window_ != NULL);
    DCHECK(id != NULL);
    return tab_window_->GetID(id);
  }

  STDMETHOD(Close)() {
    DCHECK(tab_window_ != NULL);
    return tab_window_->Close();
  }

 private:
  CComPtr<IeTabWindow> tab_window_;
  DISALLOW_COPY_AND_ASSIGN(TabWindowAdapter);
};

// Adapter for tab-window-manager interfaces on IE7/IE8.
template <class IeTabWindowManager, class IeTabWindow>
class TabWindowManagerAdapter : public TabWindowManager {
 public:
  explicit TabWindowManagerAdapter(IeTabWindowManager* manager)
      : manager_(manager) {
  }

  STDMETHOD(IndexFromHWND)(HWND window, long* index) {
    DCHECK(manager_ != NULL);
    DCHECK(index != NULL);
    return manager_->IndexFromHWND(window, index);
  }

  STDMETHOD(SelectTab)(long index) {
    DCHECK(manager_ != NULL);
    return manager_->SelectTab(index);
  }

  STDMETHOD(GetCount)(long* count) {
    DCHECK(manager_ != NULL);
    DCHECK(count != NULL);
    return manager_->GetCount(count);
  }

  STDMETHOD(GetItemWrapper)(long index, scoped_ptr<TabWindow>* tab_window) {
    DCHECK(manager_ != NULL);
    DCHECK(tab_window != NULL);

    CComPtr<IUnknown> tab_unk;
    HRESULT hr = E_FAIL;
    hr = manager_->GetItem(index, &tab_unk);
    DCHECK(SUCCEEDED(hr)) << "GetItem failed for index: " << index << ". " <<
                             com::LogHr(hr);
    if (FAILED(hr))
      return hr;

    CComPtr<IeTabWindow> ie_tab_window;
    hr = tab_unk.QueryInterface(&ie_tab_window);
    DCHECK(SUCCEEDED(hr)) << "QI failed IeTabWindow. " << com::LogHr(hr);
    if (FAILED(hr))
      return hr;

    tab_window->reset(new TabWindowAdapter<IeTabWindow>(ie_tab_window));
    return S_OK;
  }

  STDMETHOD(RepositionTab)(long moving_id, long dest_id, int unused) {
    DCHECK(manager_ != NULL);
    return manager_->RepositionTab(moving_id, dest_id, unused);
  }

  STDMETHOD(CloseAllTabs)() {
    DCHECK(manager_ != NULL);
    return manager_->CloseAllTabs();
  }

 private:
  CComPtr<IeTabWindowManager> manager_;
  DISALLOW_COPY_AND_ASSIGN(TabWindowManagerAdapter);
};

typedef TabWindowManagerAdapter<ITabWindowManagerIe9, ITabWindowIe9>
    TabWindowManagerAdapter9;
typedef TabWindowManagerAdapter<ITabWindowManagerIe8, ITabWindowIe8_1>
    TabWindowManagerAdapter8_1;
typedef TabWindowManagerAdapter<ITabWindowManagerIe8, ITabWindowIe8>
    TabWindowManagerAdapter8;
typedef TabWindowManagerAdapter<ITabWindowManagerIe7, ITabWindowIe7>
    TabWindowManagerAdapter7;

// Faked tab-window class for IE6.
class TabWindow6 : public TabWindow {
 public:
  explicit TabWindow6(TabWindowManager *manager) : manager_(manager) {}

  STDMETHOD(GetBrowser)(IDispatch** browser) {
    // Currently nobody calls this method.
    NOTREACHED();
    return E_NOTIMPL;
  }

  STDMETHOD(GetID)(long* id) {
    DCHECK(id != NULL);
    *id = 0;
    return S_OK;
  }

  STDMETHOD(Close)() {
    DCHECK(manager_ != NULL);
    // IE6 has only one tab for each frame window.
    // So closing one tab means closing all the tabs.
    return manager_->CloseAllTabs();
  }

 private:
  TabWindowManager* manager_;
  DISALLOW_COPY_AND_ASSIGN(TabWindow6);
};

// Faked tab-window-manager class for IE6.
class TabWindowManager6 : public TabWindowManager {
 public:
  explicit TabWindowManager6(HWND frame_window) : frame_window_(frame_window) {}

  STDMETHOD(IndexFromHWND)(HWND window, long* index) {
    DCHECK(window != NULL);
    DCHECK(index != NULL);
    *index = 0;
    return S_OK;
  }

  STDMETHOD(SelectTab)(long index) {
    DCHECK_EQ(0, index);
    return S_OK;
  }

  STDMETHOD(GetCount)(long* count) {
    DCHECK(count != NULL);
    *count = 1;
    return S_OK;
  }

  STDMETHOD(GetItemWrapper)(long index, scoped_ptr<TabWindow>* tab_window) {
    DCHECK(tab_window != NULL);
    DCHECK_EQ(0, index);
    tab_window->reset(new TabWindow6(this));
    return S_OK;
  }

  STDMETHOD(RepositionTab)(long moving_id, long dest_id, int unused) {
    DCHECK_EQ(0, moving_id);
    DCHECK_EQ(0, dest_id);
    return S_OK;
  }

  STDMETHOD(CloseAllTabs)() {
    DCHECK(IsWindow(frame_window_));
    ::PostMessage(frame_window_, WM_CLOSE, 0, 0);
    return S_OK;
  }
 private:
  HWND frame_window_;
};

}  // anonymous namespace

HRESULT CreateTabWindowManager(HWND frame_window,
                               scoped_ptr<TabWindowManager>* manager) {
  CComPtr<IUnknown> manager_unknown;
  HRESULT hr = ie_tab_interfaces::TabWindowManagerFromFrame(
      frame_window,
      __uuidof(IUnknown),
      reinterpret_cast<void**>(&manager_unknown));
  if (SUCCEEDED(hr)) {
    DCHECK(manager_unknown != NULL);
    CComQIPtr<ITabWindowManagerIe9> manager_ie9(manager_unknown);
    if (manager_ie9 != NULL) {
      manager->reset(new TabWindowManagerAdapter9(manager_ie9));
      return S_OK;
    }

    CComQIPtr<ITabWindowManagerIe8> manager_ie8(manager_unknown);
    if (manager_ie8 != NULL) {
      // On IE8, there was a version change that introduced a new version
      // of the ITabWindow interface even though the ITabWindowManager didn't
      // change. So we must find which one before we make up our mind.
      CComPtr<IUnknown> tab_window_punk;
      hr = manager_ie8->GetItem(0, &tab_window_punk);
      DCHECK(SUCCEEDED(hr) && tab_window_punk != NULL) << com::LogHr(hr);
      CComQIPtr<ITabWindowIe8> tab_window8(tab_window_punk);
      if (tab_window8 != NULL) {
        manager->reset(new TabWindowManagerAdapter8(manager_ie8));
        return S_OK;
      }
      CComQIPtr<ITabWindowIe8_1> tab_window8_1(tab_window_punk);
      if (tab_window8_1 != NULL) {
        manager->reset(new TabWindowManagerAdapter8_1(manager_ie8));
        return S_OK;
      }
      NOTREACHED() << "Found an ITabWindow Punk that is not known by us!!!";
      return E_UNEXPECTED;
    }

    CComQIPtr<ITabWindowManagerIe7> manager_ie7(manager_unknown);
    if (manager_ie7 != NULL) {
      manager->reset(new TabWindowManagerAdapter7(manager_ie7));
      return S_OK;
    }

    // Maybe future IE would have another interface. Consider it as IE6 anyway.
    NOTREACHED();
  }

  LOG(WARNING) <<
      "Could not create a sensible brower interface, defaulting to IE6";
  manager->reset(new TabWindowManager6(frame_window));
  return S_OK;
}
