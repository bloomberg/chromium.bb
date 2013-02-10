// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_DELETE_CHROME_HISTORY_H_
#define CHROME_FRAME_DELETE_CHROME_HISTORY_H_

#include <atlbase.h>
#include <atlwin.h>
#include <atlcom.h>

#include <deletebrowsinghistory.h>

#include "base/message_loop.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/chrome_frame_plugin.h"
#include "chrome_frame/chrome_tab.h"
#include "grit/chrome_frame_resources.h"

class Bho;

// DeleteChromeHistory: Implements IDeleteBrowsingHistory and
// pumps cache clearing operations over automation. Blocks the UI
// thread while operating.  You have been warned.
class ATL_NO_VTABLE DeleteChromeHistory
    : public CComTearOffObjectBase<Bho, CComSingleThreadModel>,
      public CWindowImpl<DeleteChromeHistory>,
      public ChromeFramePlugin<DeleteChromeHistory>,
      public IDeleteBrowsingHistory {
 public:
  DeleteChromeHistory();
  ~DeleteChromeHistory();

  HRESULT FinalConstruct();

DECLARE_CLASSFACTORY_SINGLETON(DeleteChromeHistory)

BEGIN_COM_MAP(DeleteChromeHistory)
  COM_INTERFACE_ENTRY(IDeleteBrowsingHistory)
END_COM_MAP()

BEGIN_MSG_MAP(DeleteChromeHistory)
  CHAIN_MSG_MAP(ChromeFramePlugin<DeleteChromeHistory>)
END_MSG_MAP()

  // IDeleteBrowsingHistory methods
  STDMETHOD(DeleteBrowsingHistory)(DWORD flags);

 protected:
  // ChromeFrameDelegate overrides
  virtual void OnAutomationServerReady();
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version);

  virtual void GetProfilePath(const std::wstring& profile_name,
                              base::FilePath* profile_path);

 private:
  unsigned long remove_mask_;
  MessageLoopForUI loop_;
};

#endif  // CHROME_FRAME_DELETE_CHROME_HISTORY_H_
