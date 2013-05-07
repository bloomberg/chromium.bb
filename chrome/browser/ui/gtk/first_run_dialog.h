// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_GTK_FIRST_RUN_DIALOG_H_

typedef struct _GtkButton GtkButton;
typedef struct _GtkWidget GtkWidget;

#include "base/compiler_specific.h"
#include "chrome/browser/first_run/first_run.h"
#include "ui/base/gtk/gtk_signal.h"

class FirstRunDialog {
 public:
  // Displays the first run UI for reporting opt-in, import data etc.
  // Returns true if the dialog was shown.
  static bool Show();

 private:
  FirstRunDialog();
  virtual ~FirstRunDialog();

  CHROMEGTK_CALLBACK_1(FirstRunDialog, void, OnResponseDialog, int);
  CHROMEG_CALLBACK_0(FirstRunDialog, void, OnLearnMoreLinkClicked, GtkButton*);

  void ShowReportingDialog();

  // This method closes the first run window and quits the message loop so that
  // the Chrome startup can continue. This should be called when all the
  // first run tasks are done.
  void FirstRunDone();

  // Dialog that holds the bug reporting and default browser checkboxes.
  GtkWidget* dialog_;

  // Crash reporting checkbox
  GtkWidget* report_crashes_;

  // Make browser default checkbox
  GtkWidget* make_default_;

  // Whether we should show the dialog asking the user whether to report
  // crashes and usage stats.
  bool show_reporting_dialog_;

  // User response (accept or cancel) is returned through this.
  int* response_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunDialog);
};

#endif  // CHROME_BROWSER_UI_GTK_FIRST_RUN_DIALOG_H_
