// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_JS_MODAL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_JS_MODAL_DIALOG_COCOA_H_
#pragma once

#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

#if __OBJC__
@class NSAlert;
@class JavaScriptAppModalDialogHelper;
#else
class NSAlert;
class JavaScriptAppModalDialogHelper;
#endif

class JSModalDialogCocoa : public NativeAppModalDialog {
 public:
  explicit JSModalDialogCocoa(JavaScriptAppModalDialog* dialog);
  virtual ~JSModalDialogCocoa();

  // Overridden from NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const;
  virtual void ShowAppModalDialog();
  virtual void ActivateAppModalDialog();
  virtual void CloseAppModalDialog();
  virtual void AcceptAppModalDialog();
  virtual void CancelAppModalDialog();

  JavaScriptAppModalDialog* dialog() const { return dialog_.get(); }

 private:
  scoped_ptr<JavaScriptAppModalDialog> dialog_;

  scoped_nsobject<JavaScriptAppModalDialogHelper> helper_;
  NSAlert* alert_; // weak, owned by |helper_|.

  DISALLOW_COPY_AND_ASSIGN(JSModalDialogCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_JS_MODAL_DIALOG_COCOA_H_

