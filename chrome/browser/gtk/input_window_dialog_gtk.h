// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_INPUT_WINDOW_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_INPUT_WINDOW_DIALOG_GTK_H_

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/input_window_dialog.h"

class InputWindowDialogGtk : public InputWindowDialog {
 public:
  // Creates a dialog. Takes ownership of |delegate|.
  InputWindowDialogGtk(GtkWindow* parent,
                       const std::string& window_title,
                       const std::string& label,
                       const std::string& contents,
                       Delegate* delegate);
  virtual ~InputWindowDialogGtk();

  virtual void Show();
  virtual void Close();

 private:
  CHROMEG_CALLBACK_0(InputWindowDialogGtk, void, OnEntryChanged, GtkEditable*);

  CHROMEGTK_CALLBACK_1(InputWindowDialogGtk, void, OnResponse, int);

  CHROMEGTK_CALLBACK_1(InputWindowDialogGtk, gboolean,
                       OnWindowDeleteEvent, GdkEvent*);

  CHROMEGTK_CALLBACK_0(InputWindowDialogGtk, void, OnWindowDestroy);

  // The underlying gtk dialog window.
  GtkWidget* dialog_;

  // The GtkEntry in this form.
  GtkWidget* input_;

  // Our delegate. Consumes the window's output.
  scoped_ptr<InputWindowDialog::Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(InputWindowDialogGtk);
};
#endif  // CHROME_BROWSER_GTK_INPUT_WINDOW_DIALOG_GTK_H_
