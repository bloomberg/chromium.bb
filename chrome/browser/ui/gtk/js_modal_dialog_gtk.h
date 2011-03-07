// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_JS_MODAL_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_JS_MODAL_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/native_widget_types.h"

typedef struct _GtkWidget GtkWidget;

class JavaScriptAppModalDialog;

class JSModalDialogGtk : public NativeAppModalDialog {
 public:
  JSModalDialogGtk(JavaScriptAppModalDialog* dialog,
                   gfx::NativeWindow parent_window);
  virtual ~JSModalDialogGtk();

  // NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const;
  virtual void ShowAppModalDialog();
  virtual void ActivateAppModalDialog();
  virtual void CloseAppModalDialog();
  virtual void AcceptAppModalDialog();
  virtual void CancelAppModalDialog();

 private:
  CHROMEGTK_CALLBACK_1(JSModalDialogGtk, void, OnResponse, int);

  scoped_ptr<JavaScriptAppModalDialog> dialog_;
  GtkWidget* gtk_dialog_;

  DISALLOW_COPY_AND_ASSIGN(JSModalDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_JS_MODAL_DIALOG_GTK_H_
