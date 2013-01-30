// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PROTOCOL_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_PROTOCOL_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/ui/protocol_dialog_delegate.h"
#include "googleurl/src/gurl.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

// Gtk implementation of a dialog for handling special protocols.
class ProtocolDialogGtk {
 public:
  explicit ProtocolDialogGtk(scoped_ptr<const ProtocolDialogDelegate> delegate);
  virtual ~ProtocolDialogGtk();

 private:
  CHROMEGTK_CALLBACK_1(ProtocolDialogGtk, void, OnResponse, int);

  const scoped_ptr<const ProtocolDialogDelegate> delegate_;
  base::TimeTicks creation_time_;

  GtkWidget* dialog_;
  GtkWidget* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_PROTOCOL_DIALOG_GTK_H_
