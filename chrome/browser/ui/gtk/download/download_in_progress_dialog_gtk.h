// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

class DownloadInProgressDialogGtk {
 public:
  static void Show(Browser* browser, gfx::NativeWindow parent_window);

 private:
  explicit DownloadInProgressDialogGtk(Browser* browser,
                                       gfx::NativeWindow parent_window);
  ~DownloadInProgressDialogGtk();

  CHROMEGTK_CALLBACK_1(DownloadInProgressDialogGtk, void, OnResponse, int);

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_GTK_H_
