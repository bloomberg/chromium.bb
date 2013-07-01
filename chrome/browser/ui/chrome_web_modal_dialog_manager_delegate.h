// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROME_WEB_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_CHROME_WEB_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

class ChromeWebModalDialogManagerDelegate
    : public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  ChromeWebModalDialogManagerDelegate();
  virtual ~ChromeWebModalDialogManagerDelegate();

 protected:
  // Overridden from web_modal::WebContentsModalDialogManagerDelegate:
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebModalDialogManagerDelegate);
};

#endif  // CHROME_BROWSER_UI_CHROME_WEB_MODAL_DIALOG_MANAGER_DELEGATE_H_
