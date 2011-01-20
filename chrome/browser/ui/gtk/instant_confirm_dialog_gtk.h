// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INSTANT_CONFIRM_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_INSTANT_CONFIRM_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;
typedef struct _GtkWindow GtkWindow;

// A dialog that explains some of the privacy implications surrounding instant.
// Shown when the user enables instant for the first time.
class InstantConfirmDialogGtk {
 public:
  InstantConfirmDialogGtk(GtkWindow* parent, Profile* profile);
  ~InstantConfirmDialogGtk();

 private:
  CHROMEGTK_CALLBACK_1(InstantConfirmDialogGtk, void, OnDialogResponse, int);
  CHROMEGTK_CALLBACK_0(InstantConfirmDialogGtk, void, OnLinkButtonClicked);

  GtkWidget* dialog_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InstantConfirmDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INSTANT_CONFIRM_DIALOG_GTK_H_
