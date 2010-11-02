// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEEE_IE_PLUGIN_BHO_TAB_WINDOW_MANAGER_H_
#define CEEE_IE_PLUGIN_BHO_TAB_WINDOW_MANAGER_H_

#include <atlbase.h>

#include "base/scoped_ptr.h"

// A unified tab-window interface for IE6/IE7/IE8.
class TabWindow {
 public:
  STDMETHOD(GetBrowser)(IDispatch** browser) = 0;
  STDMETHOD(GetID)(long* id) = 0;
  STDMETHOD(Close)() = 0;
  virtual ~TabWindow() {}
};

// A unified tab-window-manager interface for IE6/IE7/IE8.
class TabWindowManager {
 public:
  STDMETHOD(IndexFromHWND)(HWND window, long* index) = 0;
  STDMETHOD(SelectTab)(long index) = 0;
  STDMETHOD(GetCount)(long* count) = 0;
  STDMETHOD(GetItemWrapper)(long index, scoped_ptr<TabWindow>* tab_window) = 0;
  STDMETHOD(RepositionTab)(long moving_id, long dest_id, int unused) = 0;
  STDMETHOD(CloseAllTabs)() = 0;
  virtual ~TabWindowManager() {}
};

// Creates a TabWindowManager object for the specified IEFrame window.
// @param frame_window The top-level frame window you wish to manage.
// @param manager The created TabWindowManager object.
HRESULT CreateTabWindowManager(HWND frame_window,
                               scoped_ptr<TabWindowManager>* manager);

#endif  // CEEE_IE_PLUGIN_BHO_TAB_WINDOW_MANAGER_H_
