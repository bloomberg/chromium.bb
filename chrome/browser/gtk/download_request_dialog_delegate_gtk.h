// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_REQUEST_DIALOG_DELEGATE_GTK_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_REQUEST_DIALOG_DELEGATE_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/download/download_request_dialog_delegate.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"

class DownloadRequestDialogDelegateGtk : public DownloadRequestDialogDelegate,
                                         public ConstrainedWindowGtkDelegate {
 public:
  DownloadRequestDialogDelegateGtk(TabContents* tab,
      DownloadRequestManager::TabDownloadState* host);

  virtual ~DownloadRequestDialogDelegateGtk();

 private:
  // DownloadRequestDialogDelegate methods.
  virtual void CloseWindow();

  // ConstrainedWindowGtkDelegate methods.
  virtual GtkWidget* GetWidgetRoot();
  virtual void DeleteDelegate();

  // Other methods.
  static void OnAllowClicked(GtkButton *button,
                             DownloadRequestDialogDelegateGtk* handler);
  static void OnDenyClicked(GtkButton *button,
                             DownloadRequestDialogDelegateGtk* handler);

  // The ConstrainedWindow that is hosting our DownloadRequestDialog.
  ConstrainedWindow* window_;

  // Our root widget.
  OwnedWidgetGtk root_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestDialogDelegateGtk);
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_REQUEST_DIALOG_DELEGATE_GTK_H_
