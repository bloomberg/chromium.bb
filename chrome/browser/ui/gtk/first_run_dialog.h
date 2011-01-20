// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_GTK_FIRST_RUN_DIALOG_H_
#pragma once

typedef struct _GtkButton GtkButton;
typedef struct _GtkWidget GtkWidget;

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "ui/base/gtk/gtk_signal.h"

class TemplateURL;
class TemplateURLModel;

class FirstRunDialog : public TemplateURLModelObserver {
 public:
  // Displays the first run UI for reporting opt-in, import data etc.
  static bool Show(Profile* profile, bool randomize_search_engine_order);

  virtual void OnTemplateURLModelChanged();

 private:
  FirstRunDialog(Profile* profile,
                 bool show_reporting_dialog,
                 bool show_search_engines_dialog,
                 int* response);
  virtual ~FirstRunDialog();

  CHROMEGTK_CALLBACK_1(FirstRunDialog, void, OnResponseDialog, int);
  CHROMEGTK_CALLBACK_0(FirstRunDialog, void, OnSearchEngineButtonClicked);
  CHROMEGTK_CALLBACK_0(FirstRunDialog, void, OnSearchEngineWindowDestroy);
  CHROMEG_CALLBACK_0(FirstRunDialog, void, OnLearnMoreLinkClicked, GtkButton*);

  void ShowSearchEngineWindow();
  void ShowReportingDialog();

  // This method closes the first run window and quits the message loop so that
  // the Chrome startup can continue. This should be called when all the
  // first run tasks are done.
  void FirstRunDone();

  // The search engine choice window. This is created and shown before
  // |dialog_|.
  GtkWidget* search_engine_window_;

  // Dialog that holds the bug reporting and default browser checkboxes.
  GtkWidget* dialog_;

  // Container for the search engine choices.
  GtkWidget* search_engine_hbox_;

  // Crash reporting checkbox
  GtkWidget* report_crashes_;

  // Make browser default checkbox
  GtkWidget* make_default_;

  // Our current profile
  Profile* profile_;

  // Owned by the profile_.
  TemplateURLModel* search_engines_model_;

  // The search engine the user chose, or NULL if the user has not chosen a
  // search engine.
  TemplateURL* chosen_search_engine_;

  // Whether we should show the dialog asking the user whether to report
  // crashes and usage stats.
  bool show_reporting_dialog_;

  // User response (accept or cancel) is returned through this.
  int* response_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunDialog);
};

#endif  // CHROME_BROWSER_UI_GTK_FIRST_RUN_DIALOG_H_
