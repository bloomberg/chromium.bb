// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_MAC_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_MAC_H_

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/constrained_window_mac.h"
#include "chrome/browser/download/download_request_dialog_delegate.h"
#include "chrome/browser/download/download_request_manager.h"

@class SheetCallback;
class TabContents;

class DownloadRequestDialogDelegateMac : public DownloadRequestDialogDelegate,
                                public ConstrainedWindowMacDelegateSystemSheet {
 public:
  DownloadRequestDialogDelegateMac(TabContents* tab,
      DownloadRequestManager::TabDownloadState* host);

  // Implement ConstrainedWindowMacDelegateSystemSheet.
  virtual void DeleteDelegate();

  // Called when the sheet is done.
  void SheetDidEnd(int returnCode);

 private:
  // DownloadRequestDialogDelegate methods.
  virtual void CloseWindow();

  // The ConstrainedWindow that is hosting our DownloadRequestDialog.
  ConstrainedWindow* window_;

  // Has a button on the sheet been clicked?
  bool responded_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestDialogDelegateMac);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_DIALOG_DELEGATE_MAC_H_

