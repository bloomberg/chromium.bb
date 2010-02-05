// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIE_MODAL_DIALOG_H_
#define CHROME_BROWSER_COOKIE_MODAL_DIALOG_H_

#include <string>

#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "googleurl/src/gurl.h"


// A controller+model class for cookie and local storage warning prompt.
// |NativeDialog| is a platform specific view.
class CookiePromptModalDialog : public AppModalDialog {
 public:
  // A union of data necessary to determine the type of message box to
  // show.
  CookiePromptModalDialog(TabContents* tab_contents,
                          const GURL& url,
                          const std::string& cookie_line,
                          CookiePromptModalDialogDelegate* delegate);
  CookiePromptModalDialog(
      TabContents* tab_contents,
      const BrowsingDataLocalStorageHelper::LocalStorageInfo& storage_info,
      CookiePromptModalDialogDelegate* delegate);
  virtual ~CookiePromptModalDialog() {}

  // AppModalDialog overrides.
  virtual int GetDialogButtons();
  virtual void AcceptWindow();
  virtual void CancelWindow();

 protected:
  // AppModalDialog overrides.
  virtual NativeDialog CreateNativeDialog();
#if defined(OS_LINUX)
  virtual void HandleDialogResponse(GtkDialog* dialog, gint response_id);
#endif

 private:
  // Cookie url.
  GURL url_;

  // Cookie to display.
  std::string cookie_line_;

  // Local storage info to display.
  BrowsingDataLocalStorageHelper::LocalStorageInfo storage_info_;

  // Whether we're showing cookie UI as opposed to other site data.
  bool cookie_ui_;

  // Delegate. The caller should provide one in order to receive results
  // from this delegate.
  CookiePromptModalDialogDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CookiePromptModalDialog);
};

#endif  // CHROME_BROWSER_COOKIE_MODAL_DIALOG_H_

