// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/native_widget_types.h"

class DownloadInProgressDialogGtk {
 public:
  static void Show(gfx::NativeWindow parent_window,
                   int download_count,
                   Browser::DownloadClosePreventionType dialog_type,
                   bool app_modal,
                   const base::Callback<void(bool)>& callback);

 private:
  DownloadInProgressDialogGtk(gfx::NativeWindow parent_window,
                              int download_count,
                              Browser::DownloadClosePreventionType dialog_type,
                              bool app_modal,
                              const base::Callback<void(bool)>& callback);
  ~DownloadInProgressDialogGtk();

  CHROMEGTK_CALLBACK_1(DownloadInProgressDialogGtk, void, OnResponse, int);

  const bool app_modal_;
  const base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
