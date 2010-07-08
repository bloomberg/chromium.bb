// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_UPDATE_RECOMMENDED_DIALOG_H_
#define CHROME_BROWSER_GTK_UPDATE_RECOMMENDED_DIALOG_H_

#include "app/gtk_integers.h"
#include "app/gtk_signal.h"
#include "base/basictypes.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class UpdateRecommendedDialog {
 public:
  static void Show(GtkWindow* parent);

 private:
  CHROMEGTK_CALLBACK_1(UpdateRecommendedDialog, void, OnResponse, gint);

  explicit UpdateRecommendedDialog(GtkWindow* parent);
  ~UpdateRecommendedDialog();

  GtkWidget* dialog_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRecommendedDialog);
};

#endif  // CHROME_BROWSER_GTK_UPDATE_RECOMMENDED_DIALOG_H_
