// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_UPDATE_RECOMMENDED_DIALOG_H_
#define CHROME_BROWSER_UI_GTK_UPDATE_RECOMMENDED_DIALOG_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_integers.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class UpdateRecommendedDialog {
 public:
  static void Show(GtkWindow* parent);

 private:
  explicit UpdateRecommendedDialog(GtkWindow* parent);
  ~UpdateRecommendedDialog();

  CHROMEGTK_CALLBACK_1(UpdateRecommendedDialog, void, OnResponse, int);

  GtkWidget* dialog_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRecommendedDialog);
};

#endif  // CHROME_BROWSER_UI_GTK_UPDATE_RECOMMENDED_DIALOG_H_
