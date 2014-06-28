// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_

#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

#if __OBJC__
@class NSAlert;
@class JavaScriptAppModalDialogHelper;
#else
class NSAlert;
class JavaScriptAppModalDialogHelper;
#endif

class JavaScriptAppModalDialogCocoa : public NativeAppModalDialog {
 public:
  explicit JavaScriptAppModalDialogCocoa(JavaScriptAppModalDialog* dialog);
  virtual ~JavaScriptAppModalDialogCocoa();

  // Overridden from NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const OVERRIDE;
  virtual void ShowAppModalDialog() OVERRIDE;
  virtual void ActivateAppModalDialog() OVERRIDE;
  virtual void CloseAppModalDialog() OVERRIDE;
  virtual void AcceptAppModalDialog() OVERRIDE;
  virtual void CancelAppModalDialog() OVERRIDE;

  JavaScriptAppModalDialog* dialog() const { return dialog_.get(); }

 private:
  // Returns the NSAlert associated with the modal dialog.
  NSAlert* GetAlert() const;

  scoped_ptr<JavaScriptAppModalDialog> dialog_;

  // Created in the constructor and destroyed in the destructor.
  base::scoped_nsobject<JavaScriptAppModalDialogHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialogCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_JAVASCRIPT_APP_MODAL_DIALOG_COCOA_H_
