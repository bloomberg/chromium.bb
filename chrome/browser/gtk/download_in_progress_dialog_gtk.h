// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_

#include "app/gtk_signal.h"
#include "base/basictypes.h"

class Browser;

typedef struct _GtkWidget GtkWidget;

class DownloadInProgressDialogGtk {
 public:
  explicit DownloadInProgressDialogGtk(Browser* browser);

 private:
  CHROMEGTK_CALLBACK_1(DownloadInProgressDialogGtk, void, OnResponse, int);

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
