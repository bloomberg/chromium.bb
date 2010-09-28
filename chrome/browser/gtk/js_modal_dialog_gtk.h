// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_JS_MODAL_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_JS_MODAL_DIALOG_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/native_app_modal_dialog.h"
#include "gfx/native_widget_types.h"

class JavaScriptAppModalDialog;

class JSModalDialogGtk : public NativeAppModalDialog {
 public:
  JSModalDialogGtk(JavaScriptAppModalDialog* dialog,
                   gfx::NativeWindow parent_window);
  virtual ~JSModalDialogGtk();

  // Overridden from NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const;
  virtual void ShowAppModalDialog();
  virtual void ActivateAppModalDialog();
  virtual void CloseAppModalDialog();
  virtual void AcceptAppModalDialog();
  virtual void CancelAppModalDialog();

 private:
  void HandleDialogResponse(GtkDialog* dialog, gint response_id);
  static void OnDialogResponse(GtkDialog* gtk_dialog, gint response_id,
                               JSModalDialogGtk* dialog);

  scoped_ptr<JavaScriptAppModalDialog> dialog_;
  GtkWidget* gtk_dialog_;

  DISALLOW_COPY_AND_ASSIGN(JSModalDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_JS_MODAL_DIALOG_GTK_H_

